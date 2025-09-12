#include "DetectionResult.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

void DetectionResult::clear(){ m_boxes.clear(); }
int DetectionResult::count() const { return m_boxes.size(); }
const QVector<DetBox>& DetectionResult::boxes() const { return m_boxes; }
void DetectionResult::setBoxes(const QVector<DetBox>& boxes){ m_boxes = boxes; }

bool DetectionResult::saveToJson(const QString& path) const{
    QJsonObject root;
    QJsonArray arr;
    for(const auto& b : m_boxes){
        QJsonObject o;
        o["x"]=b.rect.x(); o["y"]=b.rect.y();
        o["w"]=b.rect.width(); o["h"]=b.rect.height();
        o["label"]=b.label; o["score"]=b.score;
        arr.append(o);
    }
    root["image_size"] = QJsonArray{0,0}; // 可按需填充
    root["boxes"] = arr;
    QJsonDocument doc(root);
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly)) return false;
    f.write(doc.toJson());
    return true;
}

bool DetectionResult::loadFromJson(const QString& path){
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return false;
    auto data = f.readAll();
    auto doc = QJsonDocument::fromJson(data);
    if(!doc.isObject()) return false;
    auto root = doc.object();
    auto arr = root["boxes"].toArray();
    m_boxes.clear();
    for(const auto& it : arr){
        auto o = it.toObject();
        DetBox b;
        b.rect = QRectF(o["x"].toDouble(), o["y"].toDouble(),
                        o["w"].toDouble(), o["h"].toDouble());
        b.label = o["label"].toString();
        b.score = o["score"].toDouble();
        m_boxes.push_back(b);
    }
    return true;
}
