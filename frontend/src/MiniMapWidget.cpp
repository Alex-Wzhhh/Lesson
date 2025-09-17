#include "MiniMapWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QEvent>
#include <QPen>
#include <QBrush>
#include <QSizeF>

#include <algorithm>

MiniMapWidget::MiniMapWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(140, 140);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::NoFocus);
}

void MiniMapWidget::setMiniMapImage(const QImage& image, double downsample, const QSize& level0Size) {
    m_image = image;
    m_downsample = downsample > 0.0 ? downsample : 1.0;
    m_slideSize = level0Size;
    m_dragging = false;
    if (m_image.isNull()) {
        m_viewWorldRect = QRectF();
    }
    update();
}

void MiniMapWidget::setViewWorldRect(const QRectF& rect) {
    if (m_viewWorldRect == rect) {
        return;
    }
    m_viewWorldRect = rect;
    update();
}

void MiniMapWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_image.isNull()) {
        painter.setPen(QPen(Qt::gray, 1.0));
        painter.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter, tr("暂无缩略图"));
        return;
    }

    const QRectF displayRect = imageDisplayRect();
    if (displayRect.isEmpty()) {
        painter.setPen(QPen(Qt::gray, 1.0));
        painter.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignCenter, tr("缩略图不可用"));
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 120));
    painter.drawRoundedRect(displayRect.adjusted(-4.0, -4.0, 4.0, 4.0), 6.0, 6.0);

    painter.setBrush(Qt::NoBrush);
    painter.drawImage(displayRect, m_image);
    painter.setPen(QPen(QColor(220, 220, 220), 1.0));
    painter.drawRect(displayRect);

    const QRectF viewRect = viewRectInDisplay();
    if (!viewRect.isEmpty()) {
        painter.setPen(QPen(QColor(0, 200, 255), 2.0));
        painter.setBrush(QColor(0, 200, 255, 40));
        painter.drawRect(viewRect);
    }
}

void MiniMapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        const QRectF displayRect = imageDisplayRect();
        if (displayRect.contains(event->pos())) {
            m_dragging = true;
            emit requestCenterOn(displayPosToWorld(event->pos()));
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void MiniMapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        emit requestCenterOn(displayPosToWorld(event->pos()));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void MiniMapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        emit requestCenterOn(displayPosToWorld(event->pos()));
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MiniMapWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_dragging = false;
}

QRectF MiniMapWidget::imageDisplayRect() const {
    if (m_image.isNull()) return QRectF();

    constexpr double margin = 10.0;
    QRectF area = rect().adjusted(margin, margin, -margin, -margin);
    if (area.width() <= 0.0 || area.height() <= 0.0) return QRectF();

    const double scale = std::min({area.width() / static_cast<double>(m_image.width()),
                                   area.height() / static_cast<double>(m_image.height()),
                                   1.0});
    const double displayWidth = m_image.width() * scale;
    const double displayHeight = m_image.height() * scale;

    const double left = rect().center().x() - displayWidth * 0.5;
    const double top = rect().center().y() - displayHeight * 0.5;
    return QRectF(left, top, displayWidth, displayHeight);
}

QRectF MiniMapWidget::viewRectInDisplay() const {
    if (m_image.isNull() || m_downsample <= 0.0 || m_viewWorldRect.isEmpty()) {
        return QRectF();
    }
    const QRectF displayRect = imageDisplayRect();
    if (displayRect.isEmpty()) return QRectF();

    QRectF world = m_viewWorldRect;
    if (!m_slideSize.isEmpty()) {
        const QRectF bounds(QPointF(0.0, 0.0), QSizeF(m_slideSize));
        world = world.intersected(bounds);
    }
    if (world.isEmpty()) return QRectF();

    const double scaleX = displayRect.width() / static_cast<double>(m_image.width());
    const double scaleY = displayRect.height() / static_cast<double>(m_image.height());

    const QRectF imageRect(world.left() / m_downsample,
                           world.top() / m_downsample,
                           world.width() / m_downsample,
                           world.height() / m_downsample);

    QRectF displayRectView(displayRect.left() + imageRect.left() * scaleX,
                           displayRect.top() + imageRect.top() * scaleY,
                           imageRect.width() * scaleX,
                           imageRect.height() * scaleY);
    return displayRectView;
}

QPointF MiniMapWidget::displayPosToWorld(const QPointF& pos) const {
    if (m_image.isNull() || m_downsample <= 0.0) {
        return QPointF();
    }
    const QRectF displayRect = imageDisplayRect();
    if (displayRect.isEmpty()) {
        return QPointF();
    }

    QPointF clamped = pos;
    clamped.setX(std::clamp(clamped.x(), displayRect.left(), displayRect.right()));
    clamped.setY(std::clamp(clamped.y(), displayRect.top(), displayRect.bottom()));

    if (displayRect.width() <= 0.0 || displayRect.height() <= 0.0) {
        return QPointF();
    }

    const double relX = (clamped.x() - displayRect.left()) / displayRect.width();
    const double relY = (clamped.y() - displayRect.top()) / displayRect.height();

    double worldX = relX * static_cast<double>(m_image.width()) * m_downsample;
    double worldY = relY * static_cast<double>(m_image.height()) * m_downsample;

    if (!m_slideSize.isEmpty()) {
        worldX = std::clamp(worldX, 0.0, static_cast<double>(m_slideSize.width()));
        worldY = std::clamp(worldY, 0.0, static_cast<double>(m_slideSize.height()));
    }

    return QPointF(worldX, worldY);
}