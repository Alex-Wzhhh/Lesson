#include "MainWindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QStringList>
#include <QUrl>
#include <QMenuBar>
#include <QAction>

#include "WSIHandler.h"
#include "WSIView.h"
#include "InferenceClient.h"
#include "DetectionResult.h"

// 从 config/settings.json 读取后端 URL（找不到则用默认）
static QUrl loadBackendUrl() {
    QUrl def("http://127.0.0.1:5001");
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("settings.json"),
        QDir(appDir).filePath("../config/settings.json"),
        QDir(appDir).filePath("../../frontend/config/settings.json") // 兼容 build 目录
    };
    for (const auto &p : candidates) {
        QFile f(p);
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                const auto obj = doc.object();
                const auto s = obj.value("backend_base_url").toString();
                if (!s.isEmpty()) return QUrl(s);
            }
        }
    }
    return def;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 后端 URL
    const QUrl backendBase = loadBackendUrl();

    // 中心视图
    m_view = new WSIView(this);
    setCentralWidget(m_view);

    // 业务对象
    m_handler = std::make_unique<WSIHandler>(backendBase);
    m_infer   = std::make_unique<InferenceClient>(backendBase);

    // 不再依赖 .ui 中的 QAction —— 这里统一创建菜单与动作
    {
        QMenu* fileMenu = nullptr;
        QMenu* runMenu  = nullptr;

        // 若 .ui 里已经有菜单可直接复用；否则创建新的
        if (menuBar()->findChild<QMenu*>("menuFile")) {
            fileMenu = menuBar()->findChild<QMenu*>("menuFile");
        } else {
            fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
            fileMenu->setObjectName("menuFile");
        }
        if (menuBar()->findChild<QMenu*>("menuRun")) {
            runMenu = menuBar()->findChild<QMenu*>("menuRun");
        } else {
            runMenu = menuBar()->addMenu(QStringLiteral("识别"));
            runMenu->setObjectName("menuRun");
        }

        // 动作：打开、保存/加载JSON、运行识别
        auto* actOpen = new QAction(QStringLiteral("打开 WSI/图像"), this);
        auto* actRun  = new QAction(QStringLiteral("运行识别（当前视口）"), this);
        auto* actSave = new QAction(QStringLiteral("保存识别结果 JSON"), this);
        auto* actLoad = new QAction(QStringLiteral("加载识别结果 JSON"), this);

        fileMenu->addAction(actOpen);
        fileMenu->addAction(actSave);
        fileMenu->addAction(actLoad);
        runMenu->addAction(actRun);

        connect(actOpen, &QAction::triggered, this, &MainWindow::openWSI);
        connect(actRun,  &QAction::triggered, this, &MainWindow::runInferenceOnViewport);
        connect(actSave, &QAction::triggered, this, &MainWindow::saveResults);
        connect(actLoad, &QAction::triggered, this, &MainWindow::loadResults);
    }

    // 视口变化时更新状态
    connect(m_view, &WSIView::viewportChanged, this, &MainWindow::updateStatus);

    statusBar()->showMessage(QStringLiteral("准备就绪（后端：%1）").arg(backendBase.toString()));
}

MainWindow::~MainWindow(){ delete ui; }

void MainWindow::openWSI() {
    const QString filters = QStringLiteral("WSI/Images (*.svs *.tif *.tiff *.ndpi *.png *.jpg *.jpeg *.bmp)");
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 WSI/图像"), QString(), filters);
    if (path.isEmpty()) return;

    if (!m_handler->open(path)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法打开文件：%1").arg(path));
        return;
    }

    // 初次读取一块区域（可按需调整尺寸）
    QImage img = m_handler->readRegionAtCurrentScale(0, 0, 2048, 2048);
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), QStringLiteral("未能从后端读取图像区域"));
        return;
    }

    m_result.clear();
    m_view->setImage(img);
    m_view->setDetections(m_result.boxes());
    statusBar()->showMessage(QStringLiteral("已打开：%1  （%2×%3）")
                             .arg(path).arg(img.width()).arg(img.height()));
}

void MainWindow::runInferenceOnViewport() {
    if (m_view->isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先打开 WSI 文件。"));
        return;
    }
    const QImage viewport = m_view->grabViewportImage();
    if (viewport.isNull()) {
        QMessageBox::warning(this, QStringLiteral("抓取失败"), QStringLiteral("无法获取视口图像"));
        return;
    }

    const QVector<DetBox> boxes = m_infer->analyzeViewport(viewport);
    m_result.setBoxes(boxes);
    m_view->setDetections(m_result.boxes());
    updateStatus();
}

void MainWindow::saveResults() {
    const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("保存识别结果 JSON"), QString(), QStringLiteral("JSON (*.json)"));
    if (out.isEmpty()) return;
    if (!m_result.saveToJson(out)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法保存 JSON：%1").arg(out));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已保存：%1").arg(out));
}

void MainWindow::loadResults() {
    const QString in = QFileDialog::getOpenFileName(this, QStringLiteral("加载识别结果 JSON"), QString(), QStringLiteral("JSON (*.json)"));
    if (in.isEmpty()) return;
    if (!m_result.loadFromJson(in)) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("无法解析 JSON：%1").arg(in));
        return;
    }
    m_view->setDetections(m_result.boxes());
    updateStatus();
}

void MainWindow::updateStatus() {
    statusBar()->showMessage(QStringLiteral("检测目标：%1 个").arg(m_result.count()));
}

