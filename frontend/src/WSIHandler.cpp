#include "WSIHandler.h"
#include <QDebug>

WSIHandler::WSIHandler(){
#ifdef USE_OPENSLIDE
    d = nullptr;
#endif
}

WSIHandler::~WSIHandler(){
#ifdef USE_OPENSLIDE
    // TODO: 释放 openslide 资源
#endif
}

bool WSIHandler::open(const QString& path){
#ifdef USE_OPENSLIDE
    // TODO: 调用 openslide 读取金字塔信息
    Q_UNUSED(path);
    return true;
#elif defined(DUMMY_WSI)
    bool ok = m_fullImage.load(path);
    if(!ok) qWarning() << "加载大图失败:" << path;
    return ok;
#else
    Q_UNUSED(path);
    return false;
#endif
}

bool WSIHandler::isOpen() const{
#ifdef USE_OPENSLIDE
    return true;
#elif defined(DUMMY_WSI)
    return !m_fullImage.isNull();
#else
    return false;
#endif
}

QImage WSIHandler::readRegionAtCurrentScale(qint64 x, qint64 y, int w, int h){
#ifdef USE_OPENSLIDE
    // TODO: 使用 openslide_read_region 读取指定区域，这里返回占位图
    QImage stub(w, h, QImage::Format_RGB32);
    stub.fill(Qt::white);
    return stub;
#elif defined(DUMMY_WSI)
    if(m_fullImage.isNull()) return QImage();
    QRect rect(x, y, w, h);
    QRect bounded = rect.intersected(m_fullImage.rect());
    if(bounded.isEmpty()) return QImage();
    return m_fullImage.copy(bounded);
#else
    return QImage();
#endif
}
