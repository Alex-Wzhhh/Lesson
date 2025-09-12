#include "WSIView.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>

WSIView::WSIView(QWidget* parent): QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setDragMode(QGraphicsView::NoDrag);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}

void WSIView::setImage(const QImage& img){
    m_scene->clear();
    m_rects.clear();
    m_boxes.clear();
    if(!img.isNull()){
        m_pix = m_scene->addPixmap(QPixmap::fromImage(img));
        m_scene->setSceneRect(m_pix->boundingRect());
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    } else {
        m_pix = nullptr;
    }
    emit viewportChanged();
}

bool WSIView::isEmpty() const { return m_pix == nullptr; }

void WSIView::setDetections(const QVector<DetBox>& boxes){
    m_boxes = boxes;
    updateOverlay();
}

void WSIView::updateOverlay(){
    for(auto* r : m_rects) { m_scene->removeItem(r); delete r; }
    m_rects.clear();
    for(const auto& b : m_boxes){
        auto* rect = m_scene->addRect(b.rect, QPen(Qt::red, 2));
        rect->setToolTip(QString("%1 (%.2f)").arg(b.label).arg(b.score));
        m_rects.push_back(rect);
    }
}

QImage WSIView::grabViewportImage() const{
    if(!m_pix) return QImage();
    const QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
    QImage img(viewRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    const_cast<WSIView*>(this)->render(
        &p,
        QRectF(QPointF(0,0), QSizeF(img.size())),
        viewRect.toRect()
    );
    return img;
}

void WSIView::wheelEvent(QWheelEvent* e){
    constexpr double scaleFactor = 1.15;
    if(e->angleDelta().y() > 0) scale(scaleFactor, scaleFactor);
    else scale(1.0/scaleFactor, 1.0/scaleFactor);
    emit viewportChanged();
}

void WSIView::mousePressEvent(QMouseEvent* e){
    if(e->button() == Qt::LeftButton){
        m_panning = true;
        m_lastPos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QGraphicsView::mousePressEvent(e);
}

void WSIView::mouseMoveEvent(QMouseEvent* e){
    if(m_panning){
        const QPoint delta = e->pos() - m_lastPos;
        m_lastPos = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        emit viewportChanged();
    }
    QGraphicsView::mouseMoveEvent(e);
}

void WSIView::mouseReleaseEvent(QMouseEvent* e){
    if(e->button() == Qt::LeftButton){
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void WSIView::resizeEvent(QResizeEvent* e){
    QGraphicsView::resizeEvent(e);
    emit viewportChanged();
}
