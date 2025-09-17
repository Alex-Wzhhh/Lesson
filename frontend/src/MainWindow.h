#pragma once
#include <QMainWindow>
#include <QPointer>
#include <memory>

#include "WSIHandler.h"
#include "WSIView.h"
#include "InferenceClient.h"
#include "DetectionResult.h"

class MiniMapWidget;
class QDockWidget;

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
    void handleLevelChanged(int level);

private:
    void updateDetectionDetails();
    void updateHeatmapVisualization();

    Ui::MainWindow* ui{nullptr};
    std::unique_ptr<WSIHandler> m_handler;
    std::unique_ptr<InferenceClient> m_infer;
    QPointer<WSIView> m_view;
    MiniMapWidget* m_miniMap{nullptr};
    QDockWidget* m_miniMapDock{nullptr};
    DetectionResult m_result;
    int m_currentLevel{0};
};

