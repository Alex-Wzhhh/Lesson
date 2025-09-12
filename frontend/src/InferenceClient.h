#pragma once
#include <QUrl>
#include <QImage>
#include <QVector>
#include "DetectionResult.h"

class InferenceClient {
public:
    explicit InferenceClient(const QUrl& base): m_base(base) {}
    QVector<DetBox> analyzeViewport(const QImage& img);

private:
    QUrl m_base;
};
