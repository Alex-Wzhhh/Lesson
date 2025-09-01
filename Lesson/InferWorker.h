#ifndef INFERWORKER_H
#define INFERWORKER_H

#endif // INFERWORKER_H

// src/inference/InferWorker.h
#pragma once
#include <QObject>
#include <QImage>
#include "InferenceEngine.h"

class InferWorker : public QObject {
    Q_OBJECT
public:
    explicit InferWorker(QObject* parent=nullptr) : QObject(parent) {}

public slots:
    void initEngine(const QString& modelPath) {
        IEOptions opt;
        opt.input_width = 224; opt.input_height = 224;
        opt.keep_ratio = false; opt.try_directml = true;
        std::string err;
        if (!engine_.load(modelPath.toStdWString(), opt, &err)) {
            emit initialized(false, QString::fromStdString(err));
        } else {
            emit initialized(true, "");
        }
    }

    void classify(const QImage& img) {
        // QImage -> cv::Mat(BGR)
        QImage rgb = img.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                    const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine());
        cv::Mat bgr; cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);

        auto topk = engine_.inferClassify(bgr);
        // 转信号数据
        QStringList lines;
        for (auto& p : topk) {
            lines << QString("#%1: %2").arg(p.class_id).arg(p.prob, 0, 'f', 4);
        }
        emit classified(lines);
    }

signals:
    void initialized(bool ok, const QString& msg);
    void classified(const QStringList& topk);

private:
    InferenceEngine engine_;
};
