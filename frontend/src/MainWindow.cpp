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
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QDockWidget>
#include <cmath>
#include <algorithm>

#include "WSIHandler.h"
#include "WSIView.h"
#include "InferenceClient.h"
#include "DetectionResult.h"
#include "MiniMapWidget.h"

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

    if (ui->resultTextEdit) {
        ui->resultTextEdit->setReadOnly(true);
    }
    if (ui->mainSplitter) {
        ui->mainSplitter->setStretchFactor(0, 3);
        ui->mainSplitter->setStretchFactor(1, 7);
        ui->mainSplitter->setCollapsible(0, false);
        ui->mainSplitter->setCollapsible(1, false);
    }
    if (ui->heatmapLabel) {
        ui->heatmapLabel->setText(QStringLiteral("暂无热力图数据"));
        ui->heatmapLabel->setAlignment(Qt::AlignCenter);
    }

    // 中心视图使用 UI 中的占位控件
    m_view = ui->wsiView;
    if (!m_view) {
        m_view = new WSIView(this);
        if (ui->mainSplitter) {
            ui->mainSplitter->addWidget(m_view);
        } else {
            setCentralWidget(m_view);
        }
    }

    // 迷你图停靠窗口
    m_miniMap = new MiniMapWidget(this);
    m_miniMapDock = new QDockWidget(QStringLiteral("迷你图"), this);
    m_miniMapDock->setObjectName(QStringLiteral("MiniMapDock"));
    m_miniMapDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_miniMapDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_miniMapDock->setWidget(m_miniMap);
    m_miniMapDock->setMinimumWidth(220);
    m_miniMapDock->setMinimumHeight(220);
    addDockWidget(Qt::RightDockWidgetArea, m_miniMapDock);

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

    connect(m_view, &WSIView::miniMapReady, m_miniMap, &MiniMapWidget::setMiniMapImage);
    connect(m_view, &WSIView::viewportChanged, this, [this]() {
        if (m_miniMap && m_view) {
            m_miniMap->setViewWorldRect(m_view->viewWorldRect());
        }
    });
    connect(m_miniMap, &MiniMapWidget::requestCenterOn, m_view, &WSIView::centerOnWorld);


    statusBar()->showMessage(QStringLiteral("准备就绪（后端：%1）").arg(backendBase.toString()));

    updateDetectionDetails();
    updateHeatmapVisualization();
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
    const QVector<double> downsamples = m_handler->levelDownsamples();
    const QVector<QSize> levelSizes = m_handler->levelSizes();
    if (levelSizes.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("未获取到切片层级信息"));
        return;
    }

    m_result.clear();
    m_view->setSlideInfo(downsamples, levelSizes);
    m_view->setDetections(m_result.boxes());
    updateDetectionDetails();
    updateHeatmapVisualization();
    m_currentLevel = m_view->currentLevel();
    if (m_handler) {
        m_handler->setCurrentLevel(m_currentLevel);
    }
    updateStatus();
    if (m_miniMap && m_view) {
        m_miniMap->setViewWorldRect(m_view->viewWorldRect());
    }
}

void MainWindow::runInferenceOnViewport() {
    if (m_view->isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先打开 WSI 文件。"));
        return;
    }
    if (!m_handler || !m_handler->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("后端切片尚未打开"));
        return;
    }
    const QImage viewport = m_view->grabViewportImage();
    if (viewport.isNull()) {
        QMessageBox::warning(this, QStringLiteral("抓取失败"), QStringLiteral("无法获取视口图像"));
        return;
    }

    ViewportMeta meta;
    meta.slideId = m_handler->slideId();
    meta.level = m_view->currentLevel();

    const QRectF worldRect = m_view->viewWorldRect();
    if (worldRect.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("抓取失败"), QStringLiteral("当前视口区域无效"));
        return;
    }
    const double downsample = m_handler->levelDownsample(meta.level);
    const double safeDown = downsample > 0.0 ? downsample : 1.0;
    meta.originX = worldRect.left() / safeDown;
    meta.originY = worldRect.top() / safeDown;


    const QVector<DetBox> boxes = m_infer->analyzeViewport(viewport, meta);

    m_result.setBoxes(boxes);
    m_view->setDetections(m_result.boxes());
    updateDetectionDetails();
    updateHeatmapVisualization();
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
    updateDetectionDetails();
    updateHeatmapVisualization();
    updateStatus();
}

void MainWindow::updateDetectionDetails() {
    if (!ui || !ui->resultTextEdit) return;

    if (m_result.count() == 0) {
        ui->resultTextEdit->setPlainText(QStringLiteral("暂无识别结果。\n请运行识别以查看检测详情。"));
        return;
    }

    QStringList lines;
    lines.reserve(m_result.count());
    int index = 1;
    for (const auto& box : m_result.boxes()) {
        const QString label = box.label.isEmpty() ? QStringLiteral("未标注") : box.label;
        lines << QStringLiteral("#%1 %2 置信度: %3\n区域: [x=%4, y=%5, w=%6, h=%7]")
                     .arg(index++)
                     .arg(label)
                     .arg(box.score, 0, 'f', 2)
                     .arg(box.rect.x(), 0, 'f', 0)
                     .arg(box.rect.y(), 0, 'f', 0)
                     .arg(box.rect.width(), 0, 'f', 0)
                     .arg(box.rect.height(), 0, 'f', 0);
    }

    ui->resultTextEdit->setPlainText(lines.join(QStringLiteral("\n\n")));
}

void MainWindow::updateHeatmapVisualization() {
    if (!ui || !ui->heatmapLabel) return;

    if (m_result.count() == 0) {
        ui->heatmapLabel->setPixmap(QPixmap());
        ui->heatmapLabel->setText(QStringLiteral("暂无热力图数据"));
        return;
    }

    QRectF bounds;
    bool firstBox = true;
    for (const auto& box : m_result.boxes()) {
        if (firstBox) {
            bounds = box.rect;
            firstBox = false;
        } else {
            bounds = bounds.united(box.rect);
        }
    }

    if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        bounds = QRectF(0.0, 0.0, 512.0, 512.0);
    }

    const double marginX = std::max(bounds.width() * 0.1, 50.0);
    const double marginY = std::max(bounds.height() * 0.1, 50.0);
    bounds.adjust(-marginX, -marginY, marginX, marginY);
    if (bounds.width() <= 0.0) bounds.setWidth(1.0);
    if (bounds.height() <= 0.0) bounds.setHeight(1.0);

    constexpr int kTargetWidth = 420;
    int heatHeight = static_cast<int>(std::round(bounds.height() / bounds.width() * kTargetWidth));
    if (heatHeight <= 0) {
        heatHeight = kTargetWidth;
    }
    heatHeight = std::clamp(heatHeight, 160, 720);

    QImage heatmap(kTargetWidth, heatHeight, QImage::Format_ARGB32_Premultiplied);
    heatmap.fill(QColor(30, 30, 30, 255));

    QPainter painter(&heatmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double scaleX = static_cast<double>(kTargetWidth) / bounds.width();
    const double scaleY = static_cast<double>(heatHeight) / bounds.height();

    for (const auto& box : m_result.boxes()) {
        const QPointF center = box.rect.center();
        const QPointF mapped((center.x() - bounds.left()) * scaleX, (center.y() - bounds.top()) * scaleY);
        const double radiusX = box.rect.width() * scaleX * 0.5;
        const double radiusY = box.rect.height() * scaleY * 0.5;
        double radiusPx = std::max(18.0, std::max(radiusX, radiusY));
        radiusPx = std::min(radiusPx, std::max(kTargetWidth, heatHeight) * 0.75);
        const double weight = std::clamp(box.score, 0.0, 1.0);
        QRadialGradient gradient(mapped, radiusPx);
        gradient.setColorAt(0.0, QColor(255, 255, 0, static_cast<int>(200 * weight + 55)));
        gradient.setColorAt(0.45, QColor(255, 140, 0, static_cast<int>(170 * weight + 40)));
        gradient.setColorAt(0.9, QColor(255, 0, 0, static_cast<int>(100 * weight + 25)));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(mapped, radiusPx, radiusPx);
    }

    painter.end();

    ui->heatmapLabel->setText(QString());
    ui->heatmapLabel->setPixmap(QPixmap::fromImage(heatmap));
}

void MainWindow::updateStatus() {
    QString msg;
    if (!m_view || m_view->isEmpty()) {
        msg = QStringLiteral("未打开切片");
    } else {
        const int level = m_view->currentLevel();
        const double zoom = m_view->viewScale() * 100.0;
        const QPointF center = m_view->viewportCenterWorld();
        msg = QStringLiteral("Level: %1  Zoom: %2%  Center: (%.0f, %.0f)  检测目标：%3 个")
                  .arg(level)
                  .arg(zoom, 0, 'f', 1)
                  .arg(center.x(), 0, 'f', 0)
                  .arg(center.y(), 0, 'f', 0)
                  .arg(m_result.count());
        if (m_handler && m_handler->isOpen()) {
            msg += QStringLiteral("  |  Slide ID：%1").arg(m_handler->slideId());
        }
    }
    statusBar()->showMessage(msg);
}

void MainWindow::handleLevelChanged(int level) {
    if (m_handler) {
        m_handler->setCurrentLevel(level);
    }
    m_currentLevel = level;
    updateStatus();
}


