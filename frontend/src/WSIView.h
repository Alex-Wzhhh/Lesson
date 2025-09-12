#pragma once
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QVector>

struct DetBox {
    QRectF rect;
    QString label;
    double score;
};

class WSIView : public QGraphicsView {
    Q_OBJECT
public:
    explicit WSIView(QWidget* parent=nullptr);
    void setImage(const QImage& img);
    bool isEmpty() const;
    void setDetections(const QVector<DetBox>& boxes);
    QImage grabViewportImage() const;

signals:
    void viewportChanged();

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateOverlay();

    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pix{nullptr};
    QList<QGraphicsRectItem*> m_rects;
    QPoint m_lastPos;
    bool m_panning{false};
    QVector<DetBox> m_boxes;
};
