#include "HttpClient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>

QJsonObject HttpClient::postJsonSync(const QUrl& base, const QString& path, const QJsonObject& payload, int timeoutMs){
    QNetworkAccessManager mgr;
    QUrl url(base);
    url.setPath(path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    QNetworkReply* reply = mgr.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    QJsonObject out;
    if(timer.isActive() && reply->error()==QNetworkReply::NoError){
        auto data = reply->readAll();
        auto doc = QJsonDocument::fromJson(data);
        if(doc.isObject()) out = doc.object();
    }
    reply->deleteLater();
    return out;
}
