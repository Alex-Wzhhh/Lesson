#ifndef IMAGELABEL_H
#define IMAGELABEL_H

#include <QLabel>
#include <QRubberBand>
#include <QMouseEvent>

class ImageLabel : public QLabel {
    Q_OBJECT
public:
    explicit ImageLabel(QWidget *parent = nullptr);

signals:
    void regionSelected(const QRect &rect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint origin_;
    QRubberBand *rubberBand_;
};

#endif // IMAGELABEL_H
frontend/main.cpp
    +1
    -4

#include <QApplication>
#include <QMainWindow> // 或你的 MainWindow 头

#include "mainwindow.h"


    int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QMainWindow w; // 或你的 MainWindow w;
    MainWindow w;
    w.show();
    return a.exec();
}
