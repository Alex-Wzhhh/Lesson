#pragma once
#include <QString>
#include <QImage>
#include <QUrl>
#include <QVector>
#include <QSize>
#include <QHash>
#include <QList>

class WSIHandler {
public:
    explicit WSIHandler(const QUrl& backendBase = QUrl("http://127.0.0.1:5001"));
    ~WSIHandler();

    bool open(const QString& path);
    bool isOpen() const;

    int levelCount() const { return m_levelCount; }
    QVector<double> levelDownsamples() const { return m_downsamples; }
    QVector<QSize> levelSizes() const { return m_levelDims; }
    QSize levelSize(int level) const;

    QImage requestRegion(int level, qint64 x, qint64 y, int w, int h);
    QImage readRegionAtCurrentScale(qint64 x0, qint64 y0, int wView, int hView, int level, double viewScale);
    double levelDownsample(int level) const;
    int slideId() const { return m_slideId; }
    int currentLevel() const { return m_currentLevel; }
    void setCurrentLevel(int level);

    struct TileKey {
        int level{0};
        qint64 x{0};
        qint64 y{0};
        bool operator==(const TileKey& other) const noexcept {
            return level == other.level && x == other.x && y == other.y;
        }
    };

private:


    void touchTile(const TileKey& key);
    QImage fetchTile(int level, qint64 x, qint64 y, int w, int h);
    void resetCache();

    QUrl m_base;
    int m_slideId{-1};
    int m_levelCount{0};
    QVector<QSize> m_levelDims;
    int m_currentLevel{0};
    QVector<double> m_downsamples;

    QHash<TileKey, QImage> m_tileCache;
    QList<TileKey> m_lru;
    int m_cacheCapacity{256};

    uint qHash(const WSIHandler::TileKey& key, uint seed = 0) noexcept;
};