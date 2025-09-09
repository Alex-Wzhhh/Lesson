#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_MainWindow {
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QLabel *imageLabel;
    QTextEdit *resultTextEdit;
    QPushButton *btnProcess;

    void setupUi(QMainWindow *MainWindow) {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        imageLabel = new QLabel(centralwidget);
        imageLabel->setObjectName(QString::fromUtf8("imageLabel"));
        verticalLayout->addWidget(imageLabel);
        resultTextEdit = new QTextEdit(centralwidget);
        resultTextEdit->setObjectName(QString::fromUtf8("resultTextEdit"));
        verticalLayout->addWidget(resultTextEdit);
        btnProcess = new QPushButton(centralwidget);
        btnProcess->setObjectName(QString::fromUtf8("btnProcess"));
        verticalLayout->addWidget(btnProcess);
        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);
        QMetaObject::connectSlotsByName(MainWindow);
    }

    void retranslateUi(QMainWindow *MainWindow) {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        btnProcess->setText(QCoreApplication::translate("MainWindow", "Process", nullptr));
    }
};

namespace Ui {
class MainWindow : public Ui_MainWindow {};
}

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
