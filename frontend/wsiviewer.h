#ifndef WSIVIEWER_H
#define WSIVIEWER_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class WSIViewer : public QGraphicsView {
    Q_OBJECT
public:
    explicit WSIViewer(QWidget *parent = nullptr);
    void setWSISize(int w, int h);
    void setImage(const QPixmap &px, int x, int y);

signals:
    void viewChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QGraphicsScene scene_;
    QGraphicsPixmapItem *pixItem_;
    bool panning_ = false;
    QPoint lastPos_;
};

#endif // WSIVIEWER_H
