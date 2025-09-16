#pragma once
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QVector>
#include <QList>
#include "DetectionResult.h"   // 唯一的 DetBox 定义

class QGraphicsScene;

class WSIView : public QGraphicsView {
    Q_OBJECT
public:
    explicit WSIView(QWidget* parent=nullptr);
    void setImage(const QImage& img);
    bool isEmpty() const;
    void setDetections(const QVector<DetBox>& boxes);
    QImage grabViewportImage() const;
    void setLevelCount(int count);
    int levelCount() const { return m_levelCount; }
    int currentLevel() const { return m_currentLevel; }
    void setCurrentLevel(int level);

signals:
    void viewportChanged();
    void levelChanged(int level);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void updateOverlay();

    QGraphicsScene* m_scene{nullptr};
    QGraphicsPixmapItem* m_pix{nullptr};
    QList<QGraphicsRectItem*> m_rects;
    QPoint m_lastPos;
    bool m_panning{false};
    QVector<DetBox> m_boxes;
    int m_levelCount{0};
    int m_currentLevel{0};
};
