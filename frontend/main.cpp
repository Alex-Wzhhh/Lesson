#include <QApplication>
#include <QMainWindow> // 或你的 MainWindow 头

#include "mainwindow.h"


int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QMainWindow w; // 或你的 MainWindow w;
    w.show();
    return a.exec();
}
