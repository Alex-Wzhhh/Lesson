#ifndef WSIHANDLER_H
#define WSIHANDLER_H

#endif // WSIHANDLER_H

#pragma once
#include <QString>
#include <QImage>
#include <QSize>
#include <QMutex>
#include <QCache>

// C API
#include <openslide/openslide.h>

class WSIHandler final {
public:
    WSIHandler();
    ~WSIHandler();

    bool open(const QString& path);
    void close();
    bool isOpen() const { return m_slide != nullptr; }

    // 基本信息
    int     levelCount() const;
    QSize   levelSize(int level) const;
    double  downsample(int level) const;
    int     bestLevelForScale(double scale) const; // scale=显示像素:原始像素，类似 1/8 -> 0.125

    // 读图（x,y 为 level-0 坐标）
    QImage  readRegion(qint64 x, qint64 y, int level, int w, int h);
    // 按瓦片读取（tileX/tileY 为该 level 的瓦片索引）
    QImage  readTile(int level, int tileX, int tileY, int tileSize = 512);

    // 元数据
    double  mppX() const;
    double  mppY() const;
    double  objectivePower() const;
    QString vendor() const;

    QString lastError() const { return m_lastError; }

private:
    void    setError(const QString& err) const;
    bool    checkError() const;

    static  double toDouble(const char* s);

private:
    openslide_t*                m_slide {nullptr};
    mutable QString             m_lastError;
    mutable QMutex              m_mutex;

    // 简易缓存（可按需替换为更完善的 LRU）
    struct TileKey {
        int level;
        int x, y, size;
        bool operator==(const TileKey& o) const {
            return level==o.level && x==o.x && y==o.y && size==o.size;
        }
    };
    struct TileKeyHash {
        size_t operator()(const TileKey& k) const {
            // 简易 hash
            return ( (size_t)k.level<<48 ) ^ ((size_t)k.x<<32) ^ ((size_t)k.y<<16) ^ (size_t)k.size;
        }
    };
    // QCache 需要指针 key，演示用 qulonglong 组合；生产可自写 LRU
    QCache<qulonglong, QImage> m_cache { 256 }; // 256 "cost"（块数），按需调整
};
