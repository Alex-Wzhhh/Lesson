#include "MainWindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent): QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    // 创建中心视图
    m_view = new WSIView(this);
    setCentralWidget(m_view);

    // 初始化 handler 与后端客户端
    m_handler = std::make_unique<WSIHandler>();
    m_infer = std::make_unique<InferenceClient>(QUrl("http://127.0.0.1:5001"));

    // 连接 UI 动作
    connect(ui->actionOpenWSI, &QAction::triggered, this, &MainWindow::openWSI);
    connect(ui->actionRunInference, &QAction::triggered, this, &MainWindow::runInferenceOnViewport);
    connect(ui->actionSaveJSON, &QAction::triggered, this, &MainWindow::saveResults);
    connect(ui->actionLoadJSON, &QAction::triggered, this, &MainWindow::loadResults);
    connect(m_view, &WSIView::viewportChanged, this, &MainWindow::updateStatus);
    statusBar()->showMessage("准备就绪");
}

MainWindow::~MainWindow(){ delete ui; }

void MainWindow::openWSI(){
#ifdef DUMMY_WSI
    QString path = QFileDialog::getOpenFileName(this, "打开大图（演示模式，无OpenSlide）", QString(), "Images (*.png *.jpg *.jpeg *.bmp)");
#else
    QString path = QFileDialog::getOpenFileName(this, "打开WSI（*.svs 等）", QString(), "WSI (*.svs *.tiff *.tif);;Images (*.png *.jpg *.jpeg *.bmp)");
#endif
    if(path.isEmpty()) return;
    if(!m_handler->open(path)){
        QMessageBox::warning(this, "打开失败", "无法打开文件：" + path);
        return;
    }
    // 读取中心区域作为初始显示
    QImage img = m_handler->readRegionAtCurrentScale(0, 0, 2048, 2048);
    m_view->setImage(img);
    m_result.clear();
    m_view->setDetections(m_result.boxes());
    statusBar()->showMessage(QString("已打开: %1, 尺寸: %2 x %3").arg(path).arg(img.width()).arg(img.height()));
}

void MainWindow::runInferenceOnViewport(){
    if(m_view->isEmpty()){
        QMessageBox::information(this, "提示", "请先打开 WSI 文件。");
        return;
    }
    // 抓取当前视口图像（当前 pixmap 渲染）
    QImage viewport = m_view->grabViewportImage();
    auto boxes = m_infer->analyzeViewport(viewport);
    m_result.setBoxes(boxes);
    m_view->setDetections(m_result.boxes());
    updateStatus();
}

void MainWindow::saveResults(){
    QString path = QFileDialog::getSaveFileName(this, "保存识别结果 JSON", QString(), "JSON (*.json)");
    if(path.isEmpty()) return;
    if(!m_result.saveToJson(path)){
        QMessageBox::warning(this, "保存失败", "无法保存 JSON。");
        return;
    }
    statusBar()->showMessage("已保存识别结果到: " + path);
}

void MainWindow::loadResults(){
    QString path = QFileDialog::getOpenFileName(this, "加载识别结果 JSON", QString(), "JSON (*.json)");
    if(path.isEmpty()) return;
    if(!m_result.loadFromJson(path)){
        QMessageBox::warning(this, "加载失败", "无法解析 JSON。");
        return;
    }
    m_view->setDetections(m_result.boxes());
    updateStatus();
}

void MainWindow::updateStatus(){
    statusBar()->showMessage(QString("检测目标：%1 个").arg(m_result.count()));
}
