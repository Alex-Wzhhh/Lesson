#include "wsiviewer.h"

#include <QWheelEvent>
#include <QMouseEvent>

WSIViewer::WSIViewer(QWidget *parent)
    : QGraphicsView(parent),
    pixItem_(new QGraphicsPixmapItem) {
    setScene(&scene_);
    scene_.addItem(pixItem_);
    setDragMode(NoDrag);
    setTransformationAnchor(AnchorUnderMouse);
}

void WSIViewer::setWSISize(int w, int h) {
    scene_.setSceneRect(0, 0, w, h);
}

void WSIViewer::setImage(const QPixmap &px, int x, int y) {
    pixItem_->setPixmap(px);
    pixItem_->setPos(x, y);
}

void WSIViewer::wheelEvent(QWheelEvent *event) {
    const double factor = 1.15;
    if (event->angleDelta().y() > 0) {
        scale(factor, factor);
    } else {
        scale(1.0 / factor, 1.0 / factor);
    }
    emit viewChanged();
}

void WSIViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        panning_ = true;
        lastPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(event);
}

void WSIViewer::mouseMoveEvent(QMouseEvent *event) {
    if (panning_) {
        QPointF delta = mapToScene(lastPos_) - mapToScene(event->pos());
        translate(delta.x(), delta.y());
        lastPos_ = event->pos();
        emit viewChanged();
    }
    QGraphicsView::mouseMoveEvent(event);
}

void WSIViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(event);
}
