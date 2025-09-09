#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnProcess_clicked();
    void onReplyFinished(QNetworkReply* reply);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *nam_;
    QString lastOutputPath_;
};

#endif // MAINWINDOW_H
