#include "mainwindow.h"
#include"ui_mainwindow.h"
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
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_btnProcess_clicked() {
    // 选择一张本地图片（先用 by-path 接口）
    QString imgPath = QFileDialog::getOpenFileName(this, "选择图像", "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (imgPath.isEmpty()) return;

    // 组织 JSON
    QJsonObject obj;
    obj["image_path"] = imgPath.replace("/", "\\"); // Windows 路径
    QJsonDocument doc(obj);

    // 发送请求
    QNetworkRequest req(QUrl("http://127.0.0.1:5001/process/by-path"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    nam_->post(req, doc.toJson());
    ui->resultTextEdit->appendPlainText("请求已发送...");
}

void MainWindow::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        ui->resultTextEdit->appendPlainText("请求失败: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError err{};
    QJsonDocument jd = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !jd.isObject()) {
        ui->resultTextEdit->appendPlainText("解析 JSON 失败");
        return;
    }
    QJsonObject obj = jd.object();
    bool ok = obj.value("ok").toBool(false);
    if (!ok) {
        ui->resultTextEdit->appendPlainText("后端返回错误: " + obj.value("error").toString());
        return;
    }

    lastOutputPath_ = obj.value("output_path").toString();
    int edgeCount = obj.value("metrics").toObject().value("edge_count").toInt();

    ui->resultTextEdit->appendPlainText("处理完成，edge_count=" + QString::number(edgeCount));
    // 显示输出图像
    if (!lastOutputPath_.isEmpty()) {
        QPixmap px(lastOutputPath_);
        if (!px.isNull()) {
            ui->imageLabel->setPixmap(px);
        } else {
            ui->resultTextEdit->appendPlainText("加载输出图像失败: " + lastOutputPath_);
        }
    }
}
