
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
#include <QTransform>
#include <QHashFunctions>
#include <QtConcurrent>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr double kEpsilon = 1e-6;
}

WSIView::WSIView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    const int idealThreads = QThread::idealThreadCount();
    if (idealThreads > 0) {
        m_tileThreadPool.setMaxThreadCount(std::max(2, idealThreads));
    } else {
        m_tileThreadPool.setMaxThreadCount(4);
    }
    m_tileThreadPool.setExpiryTimeout(3000);
}

WSIView::~WSIView() {
    ++m_generation;
    cancelPendingFetches();
    m_tileThreadPool.waitForDone();
}

void WSIView::setHandler(WSIHandler* handler) {
    m_handler = handler;
}

void WSIView::setSlideInfo(const QVector<double>& downsamples, const QVector<QSize>& levelSizes) {
    ++m_generation;
    cancelPendingFetches();

    m_downsamples = downsamples;
    m_levelSizes = levelSizes;
    m_levelCount = std::min(m_downsamples.size(), m_levelSizes.size());
    m_canvasSize = m_levelCount > 0 ? m_levelSizes.front() : QSize();
    m_worldTopLeft = QPointF(0.0, 0.0);
    m_viewScale = 1.0;
    m_currentLevel = 0;
    m_hasSlide = m_levelCount > 0;
    m_pendingFitToWindow = m_hasSlide;
    m_pendingRequest = false;
    m_requestTimer.invalidate();

    m_tileCache.clear();
    m_tileLru.clear();
    m_miniMapImage = QImage();
    m_miniMapLevel = -1;
    m_miniMapDownsample = 1.0;

    emit miniMapReady(QImage(), 1.0, QSize());

    if (m_hasSlide && m_handler) {
        prepareMiniMap();
    }

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

void WSIView::centerOnWorld(const QPointF& worldCenter) {
    if (!m_hasSlide || m_viewScale <= 0.0) return;
    if (width() <= 0 || height() <= 0) return;

    const double viewWidth = static_cast<double>(width()) / m_viewScale;
    const double viewHeight = static_cast<double>(height()) / m_viewScale;
    QPointF topLeft(worldCenter.x() - viewWidth * 0.5,
                    worldCenter.y() - viewHeight * 0.5);
    m_worldTopLeft = topLeft;
    clampWorldTopLeft();
    scheduleRepaint(true);
}

void WSIView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(40, 40, 40));

    if (!m_hasSlide) {
        drawDetections(painter);
        return;
    }

    drawLowResPreview(painter);

    const QRectF viewportRect(QPointF(0, 0), QSizeF(width(), height()));
    painter.save();
    painter.setClipRect(viewportRect);

    const QRectF worldRect = currentWorldRect();
    if (!worldRect.isEmpty() && m_currentLevel >= 0 && m_currentLevel < m_levelCount) {
        const QSize levelSize = m_levelSizes.value(m_currentLevel);
        const double downsample = (m_currentLevel >= 0 && m_currentLevel < m_downsamples.size() && m_downsamples[m_currentLevel] > 0.0)
                                      ? m_downsamples[m_currentLevel]
                                      : std::pow(2.0, m_currentLevel);
        if (levelSize.width() > 0 && levelSize.height() > 0 && downsample > 0.0) {
            const double levelLeft = worldRect.left() / downsample;
            const double levelTop = worldRect.top() / downsample;
            const double levelRight = (worldRect.left() + worldRect.width()) / downsample;
            const double levelBottom = (worldRect.top() + worldRect.height()) / downsample;

            qint64 tileXStart = static_cast<qint64>(std::floor(levelLeft / static_cast<double>(m_tileSize))) * m_tileSize;
            qint64 tileYStart = static_cast<qint64>(std::floor(levelTop / static_cast<double>(m_tileSize))) * m_tileSize;
            qint64 tileXEnd = static_cast<qint64>(std::ceil(levelRight / static_cast<double>(m_tileSize))) * m_tileSize;
            qint64 tileYEnd = static_cast<qint64>(std::ceil(levelBottom / static_cast<double>(m_tileSize))) * m_tileSize;

            tileXStart = std::max<qint64>(0, tileXStart);
            tileYStart = std::max<qint64>(0, tileYStart);
            tileXEnd = std::min<qint64>(levelSize.width(), tileXEnd);
            tileYEnd = std::min<qint64>(levelSize.height(), tileYEnd);

            for (qint64 ty = tileYStart; ty < tileYEnd; ty += m_tileSize) {
                const int tileH = static_cast<int>(std::min<qint64>(m_tileSize, levelSize.height() - ty));
                if (tileH <= 0) continue;
                for (qint64 tx = tileXStart; tx < tileXEnd; tx += m_tileSize) {
                    const int tileW = static_cast<int>(std::min<qint64>(m_tileSize, levelSize.width() - tx));
                    if (tileW <= 0) continue;
                    const TileKey key{m_currentLevel, tx, ty};
                    auto it = m_tileCache.constFind(key);
                    if (it != m_tileCache.constEnd() && !it.value().isNull()) {
                        const QImage& tile = it.value();
                        touchTile(key);
                        QRectF tileWorldRect(QPointF(tx * downsample, ty * downsample),
                                             QSizeF(tile.width() * downsample, tile.height() * downsample));
                        const QRectF destRect = worldToScreen(tileWorldRect);
                        painter.drawImage(destRect, tile);
                    } else {
                        QRectF tileWorldRect(QPointF(tx * downsample, ty * downsample),
                                             QSizeF(tileW * downsample, tileH * downsample));
                        const QRectF destRect = worldToScreen(tileWorldRect);
                        painter.fillRect(destRect, QColor(60, 60, 60, 90));
                    }
                }
            }
        }
    }

    painter.restore();
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
        scheduleRepaint(true);
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
    scheduleRepaint(true);
}

void WSIView::touchTile(const TileKey& key) {
    const int idx = m_tileLru.indexOf(key);
    if (idx >= 0) {
        m_tileLru.removeAt(idx);
    }
    m_tileLru.prepend(key);
    pruneCache();
}

void WSIView::pruneCache() {
    while (m_tileLru.size() > m_tileCacheCapacity) {
        const TileKey last = m_tileLru.takeLast();
        m_tileCache.remove(last);
    }
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
    if (!m_hasSlide) {
        update();
        emit viewportChanged();
        return;
    }

    if (force || !m_requestTimer.isValid() || m_requestTimer.elapsed() >= m_requestIntervalMs) {
        m_pendingRequest = false;
        if (m_handler) {
            updateVisibleTiles(true);
        }
        if (m_requestTimer.isValid()) {
            m_requestTimer.restart();
        } else {
            m_requestTimer.start();
        }
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

void WSIView::updateVisibleTiles(bool forceRequest) {
    if (!forceRequest) {
        return;
    }
    if (!m_handler || !m_hasSlide || width() <= 0 || height() <= 0) return;
    if (m_currentLevel < 0 || m_currentLevel >= m_levelCount) return;

    QRectF worldRect = currentWorldRect();
    if (worldRect.isEmpty()) return;

    const double marginX = worldRect.width() * 0.2;
    const double marginY = worldRect.height() * 0.2;
    worldRect.adjust(-marginX, -marginY, marginX, marginY);
    const QRectF slideRect(QPointF(0.0, 0.0), QSizeF(m_canvasSize));
    worldRect = worldRect.intersected(slideRect);
    if (worldRect.isEmpty()) return;

    const double downsample = (m_currentLevel >= 0 && m_currentLevel < m_downsamples.size() && m_downsamples[m_currentLevel] > 0.0)
                                  ? m_downsamples[m_currentLevel]
                                  : std::pow(2.0, m_currentLevel);
    if (downsample <= 0.0) return;

    const QSize levelSize = m_levelSizes.value(m_currentLevel);
    if (levelSize.width() <= 0 || levelSize.height() <= 0) return;

    const double levelLeft = worldRect.left() / downsample;
    const double levelTop = worldRect.top() / downsample;
    const double levelRight = (worldRect.left() + worldRect.width()) / downsample;
    const double levelBottom = (worldRect.top() + worldRect.height()) / downsample;

    qint64 tileXStart = static_cast<qint64>(std::floor(levelLeft / static_cast<double>(m_tileSize))) * m_tileSize;
    qint64 tileYStart = static_cast<qint64>(std::floor(levelTop / static_cast<double>(m_tileSize))) * m_tileSize;
    qint64 tileXEnd = static_cast<qint64>(std::ceil(levelRight / static_cast<double>(m_tileSize))) * m_tileSize;
    qint64 tileYEnd = static_cast<qint64>(std::ceil(levelBottom / static_cast<double>(m_tileSize))) * m_tileSize;

    tileXStart = std::max<qint64>(0, tileXStart);
    tileYStart = std::max<qint64>(0, tileYStart);
    tileXEnd = std::min<qint64>(levelSize.width(), tileXEnd);
    tileYEnd = std::min<qint64>(levelSize.height(), tileYEnd);

    for (qint64 ty = tileYStart; ty < tileYEnd; ty += m_tileSize) {
        const int tileH = static_cast<int>(std::min<qint64>(m_tileSize, levelSize.height() - ty));
        if (tileH <= 0) continue;
        for (qint64 tx = tileXStart; tx < tileXEnd; tx += m_tileSize) {
            const int tileW = static_cast<int>(std::min<qint64>(m_tileSize, levelSize.width() - tx));
            if (tileW <= 0) continue;
            const TileKey key{m_currentLevel, tx, ty};
            if (m_tileCache.contains(key)) {
                continue;
            }
            if (m_pendingFetches.contains(key)) {
                continue;
            }
            requestTile(key, tileW, tileH);
        }
    }
}

void WSIView::requestTile(const TileKey& key, int tileW, int tileH) {
    if (!m_handler || tileW <= 0 || tileH <= 0) return;
    if (m_pendingFetches.contains(key)) return;

    auto* watcher = new QFutureWatcher<QImage>(this);
    const quint64 generation = m_generation;
    auto future = QtConcurrent::run(&m_tileThreadPool, [handler = m_handler, key, tileW, tileH]() -> QImage {
        if (!handler) return QImage();
        return handler->requestRegion(key.level, key.x, key.y, tileW, tileH);
    });
    m_pendingFetches.insert(key, watcher);
    QObject::connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key, generation]() {
        QImage tile = watcher->future().result();
        m_pendingFetches.remove(key);
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        if (!tile.isNull()) {
            m_tileCache.insert(key, tile);
            touchTile(key);
            update();
        }
    });
    watcher->setFuture(future);
}

void WSIView::cancelPendingFetches() {
    for (auto watcher : std::as_const(m_pendingFetches)) {
        if (!watcher) continue;
        watcher->cancel();
        watcher->waitForFinished();
        watcher->deleteLater();
    }
    m_pendingFetches.clear();
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

void WSIView::drawLowResPreview(QPainter& painter) {
    if (m_miniMapImage.isNull() || m_miniMapDownsample <= 0.0) return;

    painter.save();
    painter.setClipRect(QRectF(QPointF(0, 0), QSizeF(width(), height())));

    QTransform transform;
    transform.translate(-m_worldTopLeft.x(), -m_worldTopLeft.y());
    transform.scale(m_viewScale, m_viewScale);
    painter.setWorldTransform(transform);

    const QRectF worldRect(QPointF(0.0, 0.0),
                           QSizeF(m_miniMapImage.width() * m_miniMapDownsample,
                                  m_miniMapImage.height() * m_miniMapDownsample));
    painter.drawImage(worldRect, m_miniMapImage);

    painter.restore();
}

void WSIView::prepareMiniMap() {
    m_miniMapImage = QImage();
    m_miniMapLevel = -1;
    m_miniMapDownsample = 1.0;

    if (!m_handler || !m_hasSlide || m_levelCount <= 0) {
        emit miniMapReady(m_miniMapImage, m_miniMapDownsample, m_canvasSize);
        return;
    }

    constexpr int kMaxDimension = 1024;
    int level = m_levelCount - 1;
    QSize levelSize;
    for (; level >= 0; --level) {
        levelSize = m_handler->levelSize(level);
        if (levelSize.width() <= 0 || levelSize.height() <= 0) {
            continue;
        }
        if (levelSize.width() <= kMaxDimension && levelSize.height() <= kMaxDimension) {
            break;
        }
        if (level == 0) {
            break;
        }
    }

    if (levelSize.width() <= 0 || levelSize.height() <= 0) {
        emit miniMapReady(m_miniMapImage, m_miniMapDownsample, m_canvasSize);
        return;
    }

    QImage mini = m_handler->requestRegion(level, 0, 0, levelSize.width(), levelSize.height());
    if (mini.isNull()) {
        emit miniMapReady(m_miniMapImage, m_miniMapDownsample, m_canvasSize);
        return;
    }

    m_miniMapImage = mini;
    m_miniMapLevel = level;
    m_miniMapDownsample = m_handler->levelDownsample(level);
    if (m_miniMapDownsample <= 0.0) {
        m_miniMapDownsample = std::pow(2.0, level);
    }
    emit miniMapReady(m_miniMapImage, m_miniMapDownsample, m_canvasSize);
}
uint qHash(const WSIView::TileKey& key, uint seed) noexcept {
    seed = ::qHash(static_cast<quint64>(key.level), seed);
    seed = ::qHash(static_cast<quint64>(key.x), seed ^ 0x9e3779b9U);
    seed = ::qHash(static_cast<quint64>(key.y), seed ^ 0x85ebca6bU);
    return seed;
}
