#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , nam_(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    connect(nam_, &QNetworkAccessManager::finished, this, &MainWindow::onReplyFinished);
    connect(ui->wsiLabel, &ImageLabel::regionSelected, this, &MainWindow::onRegionSelected);
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
        int width = obj.value("width").toInt();
        int height = obj.value("height").toInt();
        QString thumbPath = obj.value("thumbnail_path").toString();
        QPixmap px(thumbPath);
        if (!px.isNull()) {
            ui->wsiLabel->setPixmap(px);
            scaleX_ = double(width) / px.width();
            scaleY_ = double(height) / px.height();
        }
        ui->resultTextEdit->appendPlainText(QString("WSI 大小: %1 x %2, 层级: %3")
                                                .arg(width).arg(height)
                                                .arg(obj.value("level_count").toInt()));
    } else if (path.endsWith("/wsi/region")) {
        QString regionPath = obj.value("region_path").toString();
        if (!regionPath.isEmpty()) {
            QPixmap px(regionPath);
            ui->roiLabel->setPixmap(px);
        }
    }
}

void MainWindow::onRegionSelected(const QRect &rect) {
    if (currentWSIPath_.isEmpty()) return;
    QJsonObject obj;
    obj["image_path"] = currentWSIPath_;
    obj["level"] = 0;
    obj["x"] = int(rect.x() * scaleX_);
    obj["y"] = int(rect.y() * scaleY_);
    obj["w"] = int(rect.width() * scaleX_);
    obj["h"] = int(rect.height() * scaleY_);
    QNetworkRequest req(QUrl("http://127.0.0.1:5001/wsi/region"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    nam_->post(req, QJsonDocument(obj).toJson());
    ui->resultTextEdit->appendPlainText("请求ROI区域...");
}
