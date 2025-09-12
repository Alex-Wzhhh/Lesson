#pragma once
#include <QUrl>
#include <QJsonObject>

class HttpClient {
public:
    static QJsonObject postJsonSync(const QUrl& base, const QString& path, const QJsonObject& payload, int timeoutMs=15000);
};
