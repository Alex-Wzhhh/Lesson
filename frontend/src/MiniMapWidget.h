#pragma once

#include <QWidget>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QPointF>

class QMouseEvent;
class QPaintEvent;
class QEvent;

class MiniMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MiniMapWidget(QWidget* parent = nullptr);

    void setMiniMapImage(const QImage& image, double downsample, const QSize& level0Size);
    void setViewWorldRect(const QRectF& rect);

signals:
    void requestCenterOn(const QPointF& worldCenter);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF imageDisplayRect() const;
    QRectF viewRectInDisplay() const;
    QPointF displayPosToWorld(const QPointF& pos) const;

    QImage m_image;
    double m_downsample{1.0};
    QSize m_slideSize;
    QRectF m_viewWorldRect;
    bool m_dragging{false};
};