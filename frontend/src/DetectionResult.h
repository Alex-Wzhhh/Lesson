#pragma once
#include <QVector>
#include <QString>
#include <QRectF>

struct DetBox {
    QRectF rect;
    QString label;
    double score;
};

class DetectionResult {
public:
    void clear();
    int count() const;
    const QVector<DetBox>& boxes() const;
    void setBoxes(const QVector<DetBox>& boxes);
    bool saveToJson(const QString& path) const;
    bool loadFromJson(const QString& path);

private:
    QVector<DetBox> m_boxes;
};
