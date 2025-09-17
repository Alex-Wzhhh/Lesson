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
#include <QPainter>
#include <QHashFunctions>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

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
    m_downsamples.clear();

    const auto dimsObj = obj.value("level_dimensions");
    if (dimsObj.isArray()) {
        const auto dims = dimsObj.toArray();
        for (const auto& it : dims) {
            if (it.isObject()) {
                const auto o = it.toObject();
                m_levelDims.push_back(QSize(o.value("w").toInt(), o.value("h").toInt()));
            } else if (it.isArray()) {
                const auto pair = it.toArray();
                if (pair.size() == 2) {
                    m_levelDims.push_back(QSize(pair.at(0).toInt(), pair.at(1).toInt()));
                }
            }
        }
    }
    const auto downsamples = obj.value("level_downsamples").toArray();
    for (const auto& it : downsamples) {
        m_downsamples.push_back(it.toDouble(1.0));
    }
    if (m_downsamples.isEmpty() && !m_levelDims.isEmpty()) {
        m_downsamples.reserve(m_levelDims.size());
        for (int i = 0; i < m_levelDims.size(); ++i) {
            m_downsamples.push_back(i == 0 ? 1.0 : std::pow(2.0, i));
        }
    }

    if (m_levelCount == 0) {
        m_levelCount = std::min(m_levelDims.size(), m_downsamples.size());
    } else {
        m_levelCount = std::min<int>(m_levelCount, std::min(m_levelDims.size(), m_downsamples.size()));
    }

    resetCache();
    const bool ok = (m_slideId > 0) && (m_levelCount > 0) && !m_levelDims.isEmpty() && !m_downsamples.isEmpty();
    if (!ok) {
        m_slideId = -1;
        m_levelCount = 0;
        m_levelDims.clear();
        m_downsamples.clear();
    }
    return ok;
}


bool WSIHandler::isOpen() const { return m_slideId > 0; }

QSize WSIHandler::levelSize(int level) const {
    if(level < 0 || level >= m_levelDims.size()) return {};
    return m_levelDims[level];
}

void WSIHandler::resetCache() {
    m_tileCache.clear();
    m_lru.clear();
}

void WSIHandler::touchTile(const TileKey& key) {
    m_lru.removeAll(key);
    m_lru.prepend(key);
    while (m_lru.size() > m_cacheCapacity) {
        const TileKey last = m_lru.takeLast();
        m_tileCache.remove(last);
    }
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

QImage WSIHandler::fetchTile(int level, qint64 x, qint64 y, int w, int h) {
    const TileKey key{level, x, y};
    auto it = m_tileCache.find(key);
    if (it != m_tileCache.end()) {
        touchTile(key);
        return it.value();
    }
    QImage tile = requestRegion(level, x, y, w, h);
    if (!tile.isNull()) {
        m_tileCache.insert(key, tile);
        touchTile(key);
    }
    return tile;
}

QImage WSIHandler::readRegionAtCurrentScale(qint64 x0, qint64 y0, int wView, int hView, int level, double viewScale){
    if (!isOpen() || level < 0 || level >= m_levelCount) return QImage();
    if (wView <= 0 || hView <= 0 || viewScale <= 0.0) return QImage();

    const double downsample = (level >= 0 && level < m_downsamples.size() && m_downsamples[level] > 0.0)
                                  ? m_downsamples[level]
                                  : std::pow(2.0, level);
    const QSize levelSize = m_levelDims.value(level);
    if (levelSize.width() <= 0 || levelSize.height() <= 0) return QImage();

    const double worldWidth = static_cast<double>(wView) / viewScale;
    const double worldHeight = static_cast<double>(hView) / viewScale;
    if (worldWidth <= 0.0 || worldHeight <= 0.0) return QImage();

    const double levelX = static_cast<double>(x0) / downsample;
    const double levelY = static_cast<double>(y0) / downsample;
    const double levelRight = levelX + worldWidth / downsample;
    const double levelBottom = levelY + worldHeight / downsample;

    qint64 pixelStartX = std::max<qint64>(0, static_cast<qint64>(std::floor(levelX)));
    qint64 pixelStartY = std::max<qint64>(0, static_cast<qint64>(std::floor(levelY)));
    qint64 pixelEndX = std::min<qint64>(levelSize.width(), static_cast<qint64>(std::ceil(levelRight)));
    qint64 pixelEndY = std::min<qint64>(levelSize.height(), static_cast<qint64>(std::ceil(levelBottom)));

    if (pixelEndX <= pixelStartX || pixelEndY <= pixelStartY) return QImage();

    constexpr qint64 tileSize = 512;
    qint64 tileXStart = (pixelStartX / tileSize) * tileSize;
    qint64 tileYStart = (pixelStartY / tileSize) * tileSize;
    tileXStart = std::max<qint64>(0, tileXStart);
    tileYStart = std::max<qint64>(0, tileYStart);

    qint64 tileXEnd = ((pixelEndX + tileSize - 1) / tileSize) * tileSize;
    qint64 tileYEnd = ((pixelEndY + tileSize - 1) / tileSize) * tileSize;
    tileXEnd = std::min<qint64>(levelSize.width(), tileXEnd);
    tileYEnd = std::min<qint64>(levelSize.height(), tileYEnd);

    if (tileXEnd <= tileXStart || tileYEnd <= tileYStart) return QImage();

    const int canvasWidth = static_cast<int>(tileXEnd - tileXStart);
    const int canvasHeight = static_cast<int>(tileYEnd - tileYStart);
    if (canvasWidth <= 0 || canvasHeight <= 0) return QImage();

    QImage canvas(canvasWidth, canvasHeight, QImage::Format_RGB32);
    canvas.fill(Qt::gray);

    QPainter painter(&canvas);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    for (qint64 ty = tileYStart; ty < tileYEnd; ty += tileSize) {
        const int tileH = static_cast<int>(std::min<qint64>(tileSize, levelSize.height() - ty));
        if (tileH <= 0) continue;
        for (qint64 tx = tileXStart; tx < tileXEnd; tx += tileSize) {
            const int tileW = static_cast<int>(std::min<qint64>(tileSize, levelSize.width() - tx));
            if (tileW <= 0) continue;
            QImage tile = fetchTile(level, tx, ty, tileW, tileH);
            if (tile.isNull()) continue;
            const QRectF dest(QPointF(tx - tileXStart, ty - tileYStart), QSizeF(tile.width(), tile.height()));
            painter.drawImage(dest, tile);
        }
    }
    painter.end();

    const int cropX = static_cast<int>(pixelStartX - tileXStart);
    const int cropY = static_cast<int>(pixelStartY - tileYStart);
    const int cropW = static_cast<int>(pixelEndX - pixelStartX);
    const int cropH = static_cast<int>(pixelEndY - pixelStartY);
    if (cropW <= 0 || cropH <= 0) return QImage();

    return canvas.copy(cropX, cropY, cropW, cropH);
}

void WSIHandler::setCurrentLevel(int level){
    if(m_levelCount <= 0){
        m_currentLevel = 0;
        return;
    }
    const int clamped = std::clamp(level, 0, m_levelCount - 1);
    m_currentLevel = clamped;
}

double WSIHandler::levelDownsample(int level) const {
    if (level < 0 || level >= m_downsamples.size()) {
        return 1.0;
    }
    const double down = m_downsamples[level];
    if (down > 0.0) {
        return down;
    }
    return std::pow(2.0, level);
}

uint qHash(const WSIHandler::TileKey& key, uint seed) noexcept {
    seed = ::qHash(static_cast<quint64>(key.level), seed);
    seed = ::qHash(static_cast<quint64>(key.x), seed ^ 0x9e3779b9U);
    seed = ::qHash(static_cast<quint64>(key.y), seed ^ 0x85ebca6bU);
    return seed;
}


