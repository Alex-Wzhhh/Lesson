#include "InferenceClient.h"
#include "HttpClient.h"
#include <QBuffer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

QVector<DetBox> InferenceClient::analyzeViewport(const QImage& img, const ViewportMeta& meta){
    QVector<DetBox> boxes;
    if(img.isNull()) return boxes;

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    QString b64 = bytes.toBase64();

    QJsonObject payload;
    payload["image_b64"] = b64;
    if (meta.slideId > 0) {
        payload["slide_id"] = meta.slideId;
    }
    payload["level"] = meta.level;
    payload["origin_x"] = meta.originX;
    payload["origin_y"] = meta.originY;

    QJsonObject resp = HttpClient::postJsonSync(m_base, "/analyze_viewport", payload);
    auto arr = resp["boxes"].toArray();
    for(const auto& it : arr){
        auto o = it.toObject();
        DetBox b;
        b.rect = QRectF(o["x"].toDouble(), o["y"].toDouble(), o["w"].toDouble(), o["h"].toDouble());
        b.label = o["label"].toString();
        b.score = o["score"].toDouble();
        boxes.push_back(b);
    }
    return boxes;
}
