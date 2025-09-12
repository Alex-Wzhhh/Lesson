#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QTreeWidgetItem>
#include <QVector>
#include "wsiviewer.h"

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
    void onReplyFinished(QNetworkReply *reply);
    void onLevelChanged(int level);
    void requestRegion();
    void onLevelChanged(int level);
    void onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *nam_;
    QString currentWSIPath_;
    int wsiWidth_ = 0;
    int wsiHeight_ = 0;
    int currentLevel_ = 0;
    int lastX_ = 0;
    int lastY_ = 0;
    QVector<double> levelDownsamples_;
};

#endif // MAINWINDOW_H
