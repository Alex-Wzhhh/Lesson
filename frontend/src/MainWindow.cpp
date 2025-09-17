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
#include <QRect>

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
    m_view->setHandler(m_handler.get());

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
    connect(m_view, &WSIView::levelChanged, this, &MainWindow::handleLevelChanged);
    connect(m_view, &WSIView::levelChanged, this, &MainWindow::updateStatus);


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
    m_view->setLevelCount(m_handler->levelCount());
    m_handler->setCurrentLevel(m_view->currentLevel());

    // 初次读取一块区域（可按需调整尺寸）
    const auto downsamples = m_handler->levelDownsamples();
    const auto levelSizes = m_handler->levelSizes();
    if (levelSizes.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("未获取到切片层级信息"));
        return;
    }

    m_result.clear();
    m_view->setSlideInfo(downsamples, levelSizes);
    m_view->setDetections(m_result.boxes());
    const auto downsamples = m_handler->levelDownsamples();
    const auto levelSizes = m_handler->levelSizes();
    if (levelSizes.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("未获取到切片层级信息"));
    }
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

    const QRect sceneRect = m_view->lastGrabbedSceneRect();
    if(sceneRect.isEmpty()){
        QMessageBox::warning(this, QStringLiteral("抓取失败"), QStringLiteral("无法确定视口区域"));
        return;
    }

    ViewportMeta meta;
    meta.slideId = m_handler ? m_handler->slideId() : -1;
    meta.level = m_currentLevel;
    meta.originX = static_cast<double>(m_regionX + sceneRect.x());
    meta.originY = static_cast<double>(m_regionY + sceneRect.y());

    const QVector<DetBox> boxes = m_infer->analyzeViewport(viewport, meta);
    const double scale = m_view->viewScale();
    if (scale <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前缩放无效"));
        return;
    }
    const QRectF worldRect = m_view->viewWorldRect();
    QVector<DetBox> worldBoxes;
    worldBoxes.reserve(boxes.size());
    for (const auto& box : boxes) {
        DetBox converted = box;
        const QPointF topLeft(worldRect.left() + box.rect.left() / scale,
                              worldRect.top() + box.rect.top() / scale);
        const QSizeF size(box.rect.width() / scale, box.rect.height() / scale);
        converted.rect = QRectF(topLeft, size);
        worldBoxes.push_back(converted);
    }

    m_result.setBoxes(worldBoxes);
    m_view->setDetections(toSceneDetections(m_result.boxes()));
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
    m_view->setDetections(toSceneDetections(m_result.boxes()));
    updateStatus();
}

void MainWindow::updateStatus() {
    if (!m_view || m_view->isEmpty()) {
        statusBar()->showMessage(QStringLiteral("未打开切片"));
        return;
    }

    const int level = m_view->currentLevel();
    const double zoom = m_view->viewScale() * 100.0;
    const QPointF center = m_view->viewportCenterWorld();
    statusBar()->showMessage(QStringLiteral("Level: %1  Zoom: %2%  Center: (%.0f, %.0f)  检测目标：%3 个")
                                 .arg(level)
                                 .arg(zoom, 0, 'f', 1)
                                 .arg(center.x(), 0, 'f', 0)
                                 .arg(center.y(), 0, 'f', 0)
                                 .arg(m_result.count()));
    if (m_handler && m_handler->isOpen()) {
        msg += QStringLiteral("  |  Level：%1").arg(m_view->currentLevel());
    }
    statusBar()->showMessage(msg);
}

void MainWindow::handleLevelChanged(int level) {
    if (m_handler) {
        m_handler->setCurrentLevel(level);
    }
    updateStatus();
}

QVector<DetBox> MainWindow::toSceneDetections(const QVector<DetBox>& level0Boxes) const {
    if (!m_handler || !m_handler->isOpen()) return level0Boxes;
    const double down = m_handler->levelDownsample(m_currentLevel);
    const double invDown = (down > 0.0) ? (1.0 / down) : 1.0;
    const double originX = static_cast<double>(m_regionX);
    const double originY = static_cast<double>(m_regionY);

    QVector<DetBox> converted;
    converted.reserve(level0Boxes.size());
    for (const auto& box : level0Boxes) {
        DetBox localBox = box;
        const double levelX = box.rect.x() * invDown;
        const double levelY = box.rect.y() * invDown;
        const double levelW = box.rect.width() * invDown;
        const double levelH = box.rect.height() * invDown;
        localBox.rect = QRectF(levelX - originX,
                               levelY - originY,
                               levelW,
                               levelH);
        converted.push_back(localBox);
    }
    return converted;
}

