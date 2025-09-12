#include "WSIHandler.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

WSIHandler::WSIHandler(const QUrl& backendBase): m_base(backendBase) {}
WSIHandler::~WSIHandler() = default;

bool WSIHandler::open(const QString& path){
    QUrl url(m_base);
    url.setPath("/open_wsi");
    QUrlQuery q; q.addQueryItem("path", path); url.setQuery(q);

    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QNetworkReply* reply = mgr.post(req, QByteArray());
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if(!timer.isActive() || reply->error()!=QNetworkReply::NoError){
        reply->deleteLater();
        return false;
    }
    const auto data = reply->readAll();
    reply->deleteLater();

    const auto doc = QJsonDocument::fromJson(data);
    if(!doc.isObject()) return false;
    const auto obj = doc.object();

    m_slideId = obj.value("id").toInt(-1);
    m_levelCount = obj.value("level_count").toInt(0);
    m_levelDims.clear();

    const auto dims = obj.value("level_dimensions").toArray();
    m_levelDims.reserve(dims.size());
    for(const auto& it : dims){
        const auto pair = it.toArray();
        if(pair.size() == 2){
            m_levelDims.push_back(QSize(pair.at(0).toInt(), pair.at(1).toInt()));
        }
    }
    return m_slideId > 0;
}

bool WSIHandler::isOpen() const { return m_slideId > 0; }

QSize WSIHandler::levelSize(int level) const {
    if(level < 0 || level >= m_levelDims.size()) return {};
    return m_levelDims[level];
}

QImage WSIHandler::requestRegion(int level, qint64 x, qint64 y, int w, int h){
    QUrl url(m_base);
    url.setPath("/region");
    QUrlQuery q;
    q.addQueryItem("id", QString::number(m_slideId));
    q.addQueryItem("level", QString::number(level));
    q.addQueryItem("x", QString::number(x));
    q.addQueryItem("y", QString::number(y));
    q.addQueryItem("w", QString::number(w));
    q.addQueryItem("h", QString::number(h));
    url.setQuery(q);

    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QNetworkReply* reply = mgr.get(req);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    QImage out;
    if(timer.isActive() && reply->error()==QNetworkReply::NoError){
        const auto bytes = reply->readAll();
        out.loadFromData(bytes, "PNG");
    }
    reply->deleteLater();
    return out;
}

QImage WSIHandler::readRegionAtCurrentScale(qint64 x, qint64 y, int w, int h){
    return requestRegion(/*level*/0, x, y, w, h);
}

