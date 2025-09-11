#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include "imagelabel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnLoadWSI_clicked();
    void onRegionSelected(const QRect &rect);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *nam_;
    QString currentWSIPath_;
    double scaleX_ = 1.0;
    double scaleY_ = 1.0;
};

#endif // MAINWINDOW_H
