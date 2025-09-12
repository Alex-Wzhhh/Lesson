#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QSpinBox>
#include <QByteArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nam_(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    connect(nam_, &QNetworkAccessManager::finished, this, &MainWindow::onReplyFinished);
    connect(ui->wsiView, &WSIViewer::viewChanged, this, &MainWindow::requestRegion);
    connect(ui->comboLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLevelChanged);
    connect(ui->sliderLevel, &QSlider::valueChanged, this, &MainWindow::onLevelChanged);
    connect(ui->levelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLevelChanged);
    connect(ui->infoTreeWidget, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeItemChanged);
    connect(ui->levelSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onLevelChanged);
}
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_btnLoadWSI_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "选择WSI", "", "WSI (*.svs *.tif *.tiff)");
    if (path.isEmpty()) return;

    currentWSIPath_ = path.replace("/", "\\");
    QJsonObject obj;
    obj["image_path"] = currentWSIPath_;
    QNetworkRequest req(QUrl("http://127.0.0.1:5001/wsi/info"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    nam_->post(req, QJsonDocument(obj).toJson());
    ui->resultTextEdit->appendPlainText("请求WSI信息...");
}

void MainWindow::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        ui->resultTextEdit->append("请求失败: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError err{};
    QJsonDocument jd = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !jd.isObject()) {
        ui->resultTextEdit->append("解析 JSON 失败");
        return;
    }
    QJsonObject obj = jd.object();
    bool ok = obj.value("ok").toBool(false);
    if (!ok) {
        ui->resultTextEdit->append("后端返回错误: " + obj.value("error").toString());
        return;
    }

    QString path = reply->url().path();
    if (path.endsWith("/wsi/info")) {
        wsiWidth_ = obj.value("width").toInt();
        wsiHeight_ = obj.value("height").toInt();
        QJsonArray arr = obj.value("level_downsamples").toArray();
        levelDownsamples_.clear();
        for (auto v : arr) levelDownsamples_.append(v.toDouble());
        ui->levelSpinBox->setRange(0, levelDownsamples_.size() - 1);
        ui->wsiView->setWSISize(wsiWidth_, wsiHeight_);
        int levelCount = obj.value("level_count").toInt();
        ui->comboLevel->clear();
        for (int i = 0; i < levelCount; ++i) {
            ui->comboLevel->addItem(QString::number(i));
        }
        ui->sliderLevel->setRange(0, levelCount - 1);
        ui->comboLevel->setCurrentIndex(0);
        ui->sliderLevel->setValue(0);
        requestRegion();
        ui->resultTextEdit->appendPlainText(QString("WSI 大小: %1 x %2, 层级: %3")
                                                .arg(wsiWidth_).arg(wsiHeight_)
                                                .arg(levelCount));
        // parse and show level information
        ui->levelComboBox->clear();
        ui->infoTreeWidget->clear();
        levelItems_.clear();

        ui->infoTreeWidget->setColumnCount(3);
        ui->infoTreeWidget->setHeaderLabels(QStringList() << "Level" << "Dimension" << "Downsample");

        QTreeWidgetItem *levelsRoot = new QTreeWidgetItem(ui->infoTreeWidget, QStringList() << "Levels");

        QJsonArray dims = obj.value("level_dimensions").toArray();
        QJsonArray downs = obj.value("level_downsamples").toArray();
        for (int i = 0; i < dims.size(); ++i) {
            QJsonArray dimArr = dims.at(i).toArray();
            QString dimStr;
            if (dimArr.size() >= 2) {
                dimStr = QString("%1x%2").arg(dimArr.at(0).toInt()).arg(dimArr.at(1).toInt());
            }
            QString downStr;
            if (downs.size() > i) {
                downStr = QString::number(downs.at(i).toDouble());
            }
            QTreeWidgetItem *item = new QTreeWidgetItem(levelsRoot, QStringList()
                                                                        << QString::number(i)
                                                                        << dimStr
                                                                        << downStr);
            levelItems_.append(item);
            ui->levelComboBox->addItem(QString::number(i));
        }

        QTreeWidgetItem *propsRoot = new QTreeWidgetItem(ui->infoTreeWidget, QStringList() << "Properties");
        QJsonObject props = obj.value("properties").toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            new QTreeWidgetItem(propsRoot, QStringList() << it.key() << it.value().toVariant().toString());
        }
        ui->infoTreeWidget->expandAll();
        if (!levelItems_.isEmpty()) {
            ui->levelComboBox->setCurrentIndex(0);
            ui->infoTreeWidget->setCurrentItem(levelItems_.first());
        }
    } else if (path.endsWith("/wsi/region")) {
        QString regionPath = obj.value("region_path").toString();
        if (!regionPath.isEmpty()) {
            QPixmap px(regionPath);
            ui->roiLabel->setPixmap(px);
             ui->wsiView->setImage(px, lastX_, lastY_);
        }
    }
}

void MainWindow::onLevelChanged(int level) {
    if (level == currentLevel_) return;
    currentLevel_ = level;
    ui->comboLevel->blockSignals(true);
    ui->sliderLevel->blockSignals(true);
    ui->comboLevel->setCurrentIndex(level);
    ui->sliderLevel->setValue(level);
    ui->comboLevel->blockSignals(false);
    ui->sliderLevel->blockSignals(false);
    requestRegion();
}

void MainWindow::requestRegion() {
    if (currentWSIPath_.isEmpty()) return;
    QRectF rect = ui->wsiView->mapToScene(ui->wsiView->viewport()->rect()).boundingRect();
    lastX_ = int(rect.x());
    lastY_ = int(rect.y());
    QJsonObject obj;
    obj["image_path"] = currentWSIPath_;
    obj["level"] = ui->levelComboBox->currentIndex();
    obj["x"] = lastX_;
    obj["y"] = lastY_;
    obj["w"] = int(rect.width());
    obj["h"] = int(rect.height());
    QNetworkRequest req(QUrl("http://127.0.0.1:5001/wsi/region"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    nam_->post(req, QJsonDocument(obj).toJson());
     ui->resultTextEdit->appendPlainText("请求区域...");
}


void MainWindow::onLevelChanged(int level) {
    if (!ui->wsiLabel->pixmap()) return;
    if (level < 0 || level >= levelDownsamples_.size()) return;
    double down = levelDownsamples_[level];
    if (down <= 0) down = 1.0;
    scaleX_ = double(wsiWidth_) / ui->wsiLabel->pixmap()->width() / down;
    scaleY_ = double(wsiHeight_) / ui->wsiLabel->pixmap()->height() / down;
}

void MainWindow::onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *) {
    int idx = levelItems_.indexOf(current);
    if (idx >= 0) {
        ui->levelComboBox->setCurrentIndex(idx);
    }
}
