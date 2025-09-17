#pragma once
#include <QUrl>
#include <QImage>
#include <QVector>
#include "DetectionResult.h"

struct ViewportMeta {
    int slideId{-1};
    int level{0};
    double originX{0.0};
    double originY{0.0};
};

class InferenceClient {
public:
    explicit InferenceClient(const QUrl& base): m_base(base) {}
    QVector<DetBox> analyzeViewport(const QImage& img, const ViewportMeta& meta = {});

private:
    QUrl m_base;
};
