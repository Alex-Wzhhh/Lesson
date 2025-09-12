#pragma once
#include <QString>
#include <QImage>
#include <QUrl>
#include <QVector>
#include <QSize>

class WSIHandler {
public:
    explicit WSIHandler(const QUrl& backendBase = QUrl("http://127.0.0.1:5001"));
    ~WSIHandler();

    bool open(const QString& path);
    bool isOpen() const;

    QImage readRegionAtCurrentScale(qint64 x, qint64 y, int w, int h);

    int levelCount() const { return m_levelCount; }
    QSize levelSize(int level) const;

private:
    QUrl m_base;
    int m_slideId{-1};
    int m_levelCount{0};
    QVector<QSize> m_levelDims;

    QImage requestRegion(int level, qint64 x, qint64 y, int w, int h);
};
