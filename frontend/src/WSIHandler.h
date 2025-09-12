#pragma once
#include <QString>
#include <QImage>

class WSIHandler {
public:
    WSIHandler();
    ~WSIHandler();
    bool open(const QString& path);
    bool isOpen() const;
    // 根据当前缩放读取一块区域（简化：演示中忽略真实多级金字塔映射）
    QImage readRegionAtCurrentScale(qint64 x, qint64 y, int w, int h);

private:
#ifdef USE_OPENSLIDE
    struct Impl;
    Impl* d;
#elif defined(DUMMY_WSI)
    QImage m_fullImage; // 用普通大图代替
#endif
};
