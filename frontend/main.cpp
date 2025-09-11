#include <QApplication>

#include "mainwindow.h"


int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w; // 或你的 MainWindow w;
    w.show();
    return a.exec();
}
