#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QElapsedTimer>
#include <QRect>
#include "DetectionResult.h"   // 唯一的 DetBox 定义

class WSIHandler;

class WSIView : public QWidget {
    Q_OBJECT
public:
    explicit WSIView(QWidget* parent = nullptr);

    void setHandler(WSIHandler* handler);
    void setSlideInfo(const QVector<double>& downsamples, const QVector<QSize>& levelSizes);
    void resetView();

    bool isEmpty() const;
    void setDetections(const QVector<DetBox>& boxes);
    QImage grabViewportImage() const;

    int levelCount() const { return m_levelCount; }
    int currentLevel() const { return m_currentLevel; }
    double viewScale() const { return m_viewScale; }
    QRectF viewWorldRect() const;
    QPointF viewportCenterWorld() const;


signals:
    void viewportChanged();
    void levelChanged(int level);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void fitToWindow();
    void clampWorldTopLeft();
    QRectF currentWorldRect() const;
    int chooseLevel(double viewScale) const;
    void scheduleRepaint(bool force = false);
    void fetchRegion();
    QRectF worldToScreen(const QRectF& rect) const;
    void drawDetections(QPainter& painter);
    void drawMiniMap(QPainter& painter);
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
    QImage m_currentImage;
    QPointF m_currentImageWorldTopLeft{0.0, 0.0};
    int m_currentImageLevel{0};

    bool m_pendingFitToWindow{false};
    bool m_isPanning{false};
    QPoint m_lastMousePos;

    QVector<DetBox> m_detectionBoxes;

    QElapsedTimer m_requestTimer;
    int m_requestIntervalMs{80};
    bool m_pendingRequest{false};

    QImage m_miniMapImage;
    double m_miniMapDownsample{1.0};
    int m_miniMapLevel{-1};
};
