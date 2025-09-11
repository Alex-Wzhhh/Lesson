#include "imagelabel.h"

ImageLabel::ImageLabel(QWidget *parent)
    : QLabel(parent), rubberBand_(nullptr) {
    setMouseTracking(true);
}

void ImageLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        origin_ = event->pos();
        if (!rubberBand_) {
            rubberBand_ = new QRubberBand(QRubberBand::Rectangle, this);
        }
        rubberBand_->setGeometry(QRect(origin_, QSize()));
        rubberBand_->show();
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event) {
    if (rubberBand_) {
        rubberBand_->setGeometry(QRect(origin_, event->pos()).normalized());
    }
    QLabel::mouseMoveEvent(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (rubberBand_) {
        rubberBand_->hide();
        emit regionSelected(rubberBand_->geometry());
    }
    QLabel::mouseReleaseEvent(event);
}