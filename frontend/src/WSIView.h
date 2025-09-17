#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QElapsedTimer>
#include <QRect>
#include <QHash>
#include <QList>
#include <QFutureWatcher>
#include <QThreadPool>

#include "DetectionResult.h"   // 唯一的 DetBox 定义

class QPainter;
class WSIHandler;

class WSIView : public QWidget {
    Q_OBJECT
public:

    struct TileKey {
        int level{0};
        qint64 x{0};
        qint64 y{0};
        bool operator==(const TileKey& other) const noexcept {
            return level == other.level && x == other.x && y == other.y;
        }
    };

    explicit WSIView(QWidget* parent = nullptr);
     ~WSIView() override;

    void setHandler(WSIHandler* handler);
    void setSlideInfo(const QVector<double>& downsamples, const QVector<QSize>& levelSizes);
    void resetView();

    bool isEmpty() const;
    void setDetections(const QVector<DetBox>& boxes);
    QImage grabViewportImage() const;

    int levelCount() const { return m_levelCount; }
    int currentLevel() const { return m_currentLevel; }
    double viewScale() const { return m_viewScale; }
    QSize slideSize() const { return m_canvasSize; }
    QRectF viewWorldRect() const;
    QPointF viewportCenterWorld() const;

public slots:
    void centerOnWorld(const QPointF& worldCenter);

signals:
    void viewportChanged();
    void levelChanged(int level);
    void miniMapReady(const QImage& image, double downsample, const QSize& level0Size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void touchTile(const TileKey& key);
    void pruneCache();
    void fitToWindow();
    void clampWorldTopLeft();
    QRectF currentWorldRect() const;
    int chooseLevel(double viewScale) const;
    void scheduleRepaint(bool force = false);
    void updateVisibleTiles(bool forceRequest);
    void requestTile(const TileKey& key, int tileW, int tileH);
    void cancelPendingFetches();
    QRectF worldToScreen(const QRectF& rect) const;
    void drawDetections(QPainter& painter);
    void drawLowResPreview(QPainter& painter);
    void prepareMiniMap();

    WSIHandler* m_handler{nullptr};
    bool m_hasSlide{false};
    QVector<double> m_downsamples;
    QVector<QSize> m_levelSizes;
    QSize m_canvasSize;

    double m_viewScale{1.0};
    double m_minScale{0.02};
    double m_maxScale{8.0};
    QPointF m_worldTopLeft{0.0, 0.0};

    int m_levelCount{0};
    int m_currentLevel{0};

    bool m_pendingFitToWindow{false};
    bool m_isPanning{false};
    QPoint m_lastMousePos;

    QVector<DetBox> m_detectionBoxes;

    QElapsedTimer m_requestTimer;
    int m_requestIntervalMs{80};
    bool m_pendingRequest{false};

    QHash<TileKey, QImage> m_tileCache;
    QHash<TileKey, QFutureWatcher<QImage>*> m_pendingFetches;
    QList<TileKey> m_tileLru;
    QThreadPool m_tileThreadPool;
    int m_tileCacheCapacity{192};
    const qint64 m_tileSize{512};
    quint64 m_generation{0};

    QImage m_miniMapImage;
    double m_miniMapDownsample{1.0};
    int m_miniMapLevel{-1};
};

uint qHash(const WSIView::TileKey& key, uint seed = 0) noexcept;
