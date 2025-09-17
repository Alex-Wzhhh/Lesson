#include "WSIView.h"
#include "WSIHandler.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QPen>
#include <QBrush>
#include <QFontMetricsF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr double kEpsilon = 1e-6;
}

WSIView::WSIView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void WSIView::setHandler(WSIHandler* handler) {
    m_handler = handler;
}

void WSIView::setSlideInfo(const QVector<double>& downsamples, const QVector<QSize>& levelSizes) {
    m_downsamples = downsamples;
    m_levelSizes = levelSizes;
    m_levelCount = std::min(m_downsamples.size(), m_levelSizes.size());
    m_canvasSize = m_levelCount > 0 ? m_levelSizes.front() : QSize();
    m_worldTopLeft = QPointF(0.0, 0.0);
    m_viewScale = 1.0;
    m_currentLevel = 0;
    m_currentImage = QImage();
    m_currentImageLevel = 0;
    m_currentImageWorldTopLeft = QPointF(0.0, 0.0);
    m_hasSlide = m_levelCount > 0;
    m_pendingFitToWindow = m_hasSlide;
    m_pendingRequest = false;
    m_requestTimer.invalidate();
    update();
    if (m_hasSlide && width() > 0 && height() > 0) {
        fitToWindow();
        m_pendingFitToWindow = false;
    }
}


void WSIView::resetView() {
    if (!m_hasSlide) return;
    fitToWindow();
}

bool WSIView::isEmpty() const {
    return !m_hasSlide;
}

void WSIView::setDetections(const QVector<DetBox>& boxes) {
    m_detectionBoxes = boxes;
    update();
}

QImage WSIView::grabViewportImage() const {
    if (width() <= 0 || height() <= 0) return QImage();
    QImage img(size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter painter(&img);
    const_cast<WSIView*>(this)->render(&painter);
    return img;
}

QRectF WSIView::viewWorldRect() const {
    return currentWorldRect();
}


QPointF WSIView::viewportCenterWorld() const {
    const QRectF rect = currentWorldRect();
    return rect.center();
}

void WSIView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(40, 40, 40));

    if (!m_currentImage.isNull()) {
        const double downsample = (m_currentImageLevel >= 0 && m_currentImageLevel < m_downsamples.size() && m_downsamples[m_currentImageLevel] > 0.0)
        ? m_downsamples[m_currentImageLevel]
        : 1.0;
        const QPointF topLeft = (m_currentImageWorldTopLeft - m_worldTopLeft) * m_viewScale;
        const QSizeF size(m_currentImage.width() * downsample * m_viewScale,
                          m_currentImage.height() * downsample * m_viewScale);
        painter.drawImage(QRectF(topLeft, size), m_currentImage);
    }

    drawDetections(painter);
}

void WSIView::wheelEvent(QWheelEvent* event) {
    if (!m_hasSlide) {
        QWidget::wheelEvent(event);
        return;
    }

    const QPoint angle = event->angleDelta();
    if (angle.isNull()) {
        event->ignore();
        return;
    }

    double factorExponent = static_cast<double>(angle.y());
    if (event->modifiers() & Qt::ControlModifier) {
        factorExponent *= 1.5;
    }
    const double factor = std::pow(1.0015, factorExponent);

    const QPointF cursorPos = event->position();
    const QPointF worldBefore = cursorPos / m_viewScale + m_worldTopLeft;

    m_viewScale = std::clamp(m_viewScale * factor, m_minScale, m_maxScale);
    const int newLevel = chooseLevel(m_viewScale);
    if (newLevel != m_currentLevel) {
        m_currentLevel = newLevel;
        emit levelChanged(m_currentLevel);
    }

    m_worldTopLeft = worldBefore - (cursorPos / m_viewScale);
    clampWorldTopLeft();

    scheduleRepaint(true);
    event->accept();
}

void WSIView::mousePressEvent(QMouseEvent* event) {
    if (!m_hasSlide) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void WSIView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_worldTopLeft -= QPointF(delta) / m_viewScale;
        clampWorldTopLeft();
        scheduleRepaint();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void WSIView::mouseReleaseEvent(QMouseEvent* event) {
     if (m_isPanning && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton)) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void WSIView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!m_hasSlide) return;

    if (m_pendingFitToWindow) {
        fitToWindow();
        m_pendingFitToWindow = false;
        return;
    }

    clampWorldTopLeft();
    scheduleRepaint();
}

void WSIView::fitToWindow() {
    if (!m_hasSlide || m_canvasSize.isEmpty() || width() <= 0 || height() <= 0) {
        return;
    }
    const double sx = static_cast<double>(width()) / static_cast<double>(m_canvasSize.width());
    const double sy = static_cast<double>(height()) / static_cast<double>(m_canvasSize.height());
    double newScale = std::min(sx, sy);
    newScale = std::clamp(newScale, m_minScale, m_maxScale);
    m_viewScale = newScale;
    m_worldTopLeft = QPointF(0.0, 0.0);
    const int newLevel = chooseLevel(m_viewScale);
    if (newLevel != m_currentLevel) {
        m_currentLevel = newLevel;
        emit levelChanged(m_currentLevel);
    }
    scheduleRepaint(true);
}

void WSIView::clampWorldTopLeft() {
    if (!m_hasSlide || m_viewScale <= 0.0) return;
    const double viewWidth = static_cast<double>(width()) / m_viewScale;
    const double viewHeight = static_cast<double>(height()) / m_viewScale;
    const double maxX = std::max(0.0, static_cast<double>(m_canvasSize.width()) - viewWidth);
    const double maxY = std::max(0.0, static_cast<double>(m_canvasSize.height()) - viewHeight);
    m_worldTopLeft.setX(std::clamp(m_worldTopLeft.x(), 0.0, maxX));
    m_worldTopLeft.setY(std::clamp(m_worldTopLeft.y(), 0.0, maxY));
}

QRectF WSIView::currentWorldRect() const {
    if (!m_hasSlide || m_viewScale <= 0.0) return QRectF();
    const QSizeF size(static_cast<double>(width()) / m_viewScale,
                      static_cast<double>(height()) / m_viewScale);
    return QRectF(m_worldTopLeft, size);
}

int WSIView::chooseLevel(double viewScale) const {
    if (m_downsamples.isEmpty()) return 0;
    const double targetDownsample = 1.0 / viewScale;
    double bestDiff = std::numeric_limits<double>::max();
    double bestDownsample = m_downsamples.front() > 0.0 ? m_downsamples.front() : 1.0;
    int bestIndex = 0;
    for (int i = 0; i < m_downsamples.size(); ++i) {
        const double d = m_downsamples[i] > 0.0 ? m_downsamples[i] : std::pow(2.0, i);
        const double diff = std::abs(std::log2(d) - std::log2(targetDownsample));
        if (diff < bestDiff - kEpsilon) {
            bestDiff = diff;
            bestIndex = i;
            bestDownsample = d;
        } else if (std::abs(diff - bestDiff) <= kEpsilon) {
            if (targetDownsample < bestDownsample && d < bestDownsample) {
                bestIndex = i;
                bestDownsample = d;
            } else if (targetDownsample >= bestDownsample && d > bestDownsample) {
                bestIndex = i;
                bestDownsample = d;
            }
        }
    }
    return bestIndex;
}

void WSIView::scheduleRepaint(bool force) {
    if (!m_hasSlide || !m_handler) {
        update();
        emit viewportChanged();
        return;
    }

    bool fetched = false;
    if (force || !m_requestTimer.isValid() || m_requestTimer.elapsed() >= m_requestIntervalMs) {
        m_pendingRequest = false;
        fetchRegion();
        if (m_requestTimer.isValid()) {
            m_requestTimer.restart();
        } else {
            m_requestTimer.start();
        }
        fetched = true;
    } else if (!m_pendingRequest) {
        m_pendingRequest = true;
        const int delay = m_requestIntervalMs - static_cast<int>(m_requestTimer.elapsed());
        QTimer::singleShot(std::max(0, delay), this, [this]() {
            m_pendingRequest = false;
            scheduleRepaint(true);
        });
    }

    update();
    emit viewportChanged();
}

void WSIView::fetchRegion() {
    if (!m_handler || !m_hasSlide || width() <= 0 || height() <= 0) return;

    const QRectF worldRect = currentWorldRect();
    const qint64 x0 = static_cast<qint64>(std::floor(worldRect.left()));
    const qint64 y0 = static_cast<qint64>(std::floor(worldRect.top()));
    QImage img = m_handler->readRegionAtCurrentScale(x0, y0, width(), height(), m_currentLevel, m_viewScale);
    if (!img.isNull()) {
        m_currentImage = img;
        const double downsample = (m_currentLevel >= 0 && m_currentLevel < m_downsamples.size() && m_downsamples[m_currentLevel] > 0.0)
                                      ? m_downsamples[m_currentLevel]
                                      : 1.0;
        const qint64 px = std::max<qint64>(0, static_cast<qint64>(std::floor(static_cast<double>(x0) / downsample)));
        const qint64 py = std::max<qint64>(0, static_cast<qint64>(std::floor(static_cast<double>(y0) / downsample)));
        m_currentImageWorldTopLeft = QPointF(px * downsample, py * downsample);
        m_currentImageLevel = m_currentLevel;
    }
}

QRectF WSIView::worldToScreen(const QRectF& rect) const {
    const QPointF topLeft = (rect.topLeft() - m_worldTopLeft) * m_viewScale;
    const QSizeF size(rect.size().width() * m_viewScale, rect.size().height() * m_viewScale);
    return QRectF(topLeft, size);
}

void WSIView::drawDetections(QPainter& painter) {
    if (m_detectionBoxes.isEmpty()) return;

    painter.save();
    QPen pen(Qt::red);
    pen.setWidthF(1.5);
    painter.setPen(pen);
    const QRectF viewportRect(QPointF(0, 0), QSizeF(width(), height()));

    for (const auto& box : m_detectionBoxes) {
        const QRectF screenRect = worldToScreen(box.rect);
        if (!screenRect.intersects(viewportRect)) continue;
        painter.drawRect(screenRect);
        if (!box.label.isEmpty()) {
            const QString text = QStringLiteral("%1 (%.2f)").arg(box.label).arg(box.score);
            QFontMetricsF metrics(painter.font());
            const QSizeF textSize(metrics.horizontalAdvance(text) + 6.0, metrics.height() + 4.0);
            QPointF textPos = screenRect.topLeft() - QPointF(0.0, textSize.height() + 2.0);
            if (textPos.y() < 0.0) {
                textPos.setY(screenRect.bottom() + 2.0);
            }
            QRectF textRect(textPos, textSize);
            painter.fillRect(textRect, QColor(0, 0, 0, 180));
            painter.setPen(Qt::white);
            painter.drawText(textRect.adjusted(3.0, 0.0, -3.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, text);
            painter.setPen(pen);
        }
    }
    painter.restore();
}
