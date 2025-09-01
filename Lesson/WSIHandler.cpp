#ifndef WSIHANDLER_CPP
#define WSIHANDLER_CPP

#endif // WSIHANDLER_CPP

#include "WSIHandler.h"
#include <QtGlobal>
#include <QByteArray>
#include <QDebug>

// 将 (level, x, y, size) 压成 64-bit key（演示用）
static inline qulonglong packKey(int level, int x, int y, int size) {
    return ( (qulonglong)(unsigned)level << 48 )
    | ( (qulonglong)(unsigned)(x & 0xFFFF) << 32 )
        | ( (qulonglong)(unsigned)(y & 0xFFFF) << 16 )
        | ( (qulonglong)(unsigned)(size & 0xFFFF) );
}

WSIHandler::WSIHandler() = default;

WSIHandler::~WSIHandler() {
    close();
}

bool WSIHandler::open(const QString& path) {
    QMutexLocker locker(&m_mutex);
    close();

    QByteArray p = QFile::encodeName(path);
    m_slide = openslide_open(p.constData());
    if (!m_slide) {
        setError(QStringLiteral("无法打开 WSI：") + path);
        return false;
    }
    if (checkError()) {
        close();
        return false;
    }
    m_lastError.clear();
    return true;
}

void WSIHandler::close() {
    if (m_slide) {
        openslide_close(m_slide);
        m_slide = nullptr;
    }
    m_cache.clear();
}

int WSIHandler::levelCount() const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0;
    int32_t levels = openslide_get_level_count(m_slide);
    return levels < 0 ? 0 : levels;
}

QSize WSIHandler::levelSize(int level) const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return {};
    int64_t w=0, h=0;
    openslide_get_level_dimensions(m_slide, level, &w, &h);
    return QSize((int)w, (int)h);
}

double WSIHandler::downsample(int level) const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0.0;
    double ds = openslide_get_level_downsample(m_slide, level);
    return ds;
}

int WSIHandler::bestLevelForScale(double scale) const {
    // scale：显示像素 / 原始像素（举例：1/8 ≈ 0.125）
    // OpenSlide 使用 downsample（原始/当前），所以传入 1/scale
    if (scale <= 0) return 0;
    double ds = 1.0 / scale;
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0;
    return openslide_get_best_level_for_downsample(m_slide, ds);
}

QImage WSIHandler::readRegion(qint64 x, qint64 y, int level, int w, int h) {
    QMutexLocker locker(&m_mutex);
    if (!m_slide || w<=0 || h<=0) return {};

    // OpenSlide 返回的是 32-bit 预乘 ARGB（本机端序）
    const size_t N = (size_t)w * (size_t)h;
    uint32_t* buf = new (std::nothrow) uint32_t[N];
    if (!buf) {
        setError(QStringLiteral("内存分配失败：") + QString::number(N));
        return {};
    }

    openslide_read_region(m_slide, buf, x, y, level, w, h);
    if (checkError()) {
        delete[] buf;
        return {};
    }

    // 利用 QImage 包装，再拷贝一份拥有独立内存
    QImage img((uchar*)buf, w, h, w*4, QImage::Format_ARGB32_Premultiplied);
    QImage out = img.copy(); // 深拷贝
    delete[] buf;
    return out;
}

QImage WSIHandler::readTile(int level, int tileX, int tileY, int tileSize) {
    if (tileSize <= 0) return {};
    const qulonglong key = packKey(level, tileX, tileY, tileSize);

    // 命中缓存
    if (QImage* cached = m_cache.object(key)) {
        return *cached;
    }

    // 将该 level 的瓦片索引转回 level-0 坐标
    const double ds = downsample(level); // 原始/当前
    const qint64 x0 = (qint64)(tileX * (qint64)tileSize * ds);
    const qint64 y0 = (qint64)(tileY * (qint64)tileSize * ds);

    QImage img = readRegion(x0, y0, level, tileSize, tileSize);
    if (!img.isNull()) {
        m_cache.insert(key, new QImage(img));
    }
    return img;
}

double WSIHandler::toDouble(const char* s) {
    if (!s) return 0.0;
    bool ok = false;
    QString t = QString::fromUtf8(s);
    double v = t.toDouble(&ok);
    return ok ? v : 0.0;
}

double WSIHandler::mppX() const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0.0;
    return toDouble(openslide_get_property_value(m_slide, "openslide.mpp-x"));
}

double WSIHandler::mppY() const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0.0;
    return toDouble(openslide_get_property_value(m_slide, "openslide.mpp-y"));
}

double WSIHandler::objectivePower() const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return 0.0;
    return toDouble(openslide_get_property_value(m_slide, "openslide.objective-power"));
}

QString WSIHandler::vendor() const {
    QMutexLocker locker(&m_mutex);
    if (!m_slide) return {};
    const char* v = openslide_get_property_value(m_slide, "openslide.vendor");
    return v ? QString::fromUtf8(v) : QString{};
}

void WSIHandler::setError(const QString& err) const {
    m_lastError = err;
    qWarning() << "[WSIHandler]" << err;
}

bool WSIHandler::checkError() const {
    if (!m_slide) return true;
    const char* err = openslide_get_error(m_slide);
    if (err) {
        setError(QString::fromUtf8(err));
        return true;
    }
    return false;
}
