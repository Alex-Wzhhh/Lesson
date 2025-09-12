#pragma once
#include <QMainWindow>
#include <QPointer>
#include "WSIHandler.h"
#include "WSIView.h"
#include "InferenceClient.h"
#include "DetectionResult.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void openWSI();
    void runInferenceOnViewport();
    void saveResults();
    void loadResults();
    void updateStatus();

private:
    Ui::MainWindow* ui;
    std::unique_ptr<WSIHandler> m_handler;
    std::unique_ptr<InferenceClient> m_infer;
    QPointer<WSIView> m_view;
    DetectionResult m_result;
};
