#pragma once

#include "ElasticResultIO.h"
#include "FEModel.h"
#include "vtkBuilder.h"

#include <QByteArray>
#include <QMainWindow>
#include <QString>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

class QAction;
class QCheckBox;
class QComboBox;
class QDialog;
class QLabel;
class QPlainTextEdit;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;
class FloatingToolPalette;
class SoftLogPanel;
class MeshViewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onOpenFile();
    void onSolve();
    void showResultOptions();
    void updateResultDisplay();

private:
    enum class Workspace { Preprocess, Solve, Postprocess };
    static constexpr int displacementComponentCount_ = 4;

    void setupLogWidget();
    void appendLog(const QString &message);
    void setupCentral();
    void setupResultPopup();
    void setupToolPalettes();
    void layoutOverlays();
    void showElementInfo(MeshId elementId);
    void showNodeInfo(MeshId nodeId);
    bool switchWorkspace(Workspace workspace);
    void updateActionStates();
    bool loadSavedResults();
    bool showResultFrame(int index);
    void updateResultComponents();

    QPlainTextEdit *logWidget_ = nullptr;
    SoftLogPanel *logPanel_ = nullptr;
    MeshViewport *viewport_ = nullptr;
    FloatingToolPalette *selectionPalette_ = nullptr;
    FloatingToolPalette *analysisPalette_ = nullptr;

    QDialog *resultDialog_ = nullptr;
    QComboBox *componentCombo_ = nullptr;
    QCheckBox *contourCheck_ = nullptr;
    QCheckBox *legendCheck_ = nullptr;
    QWidget *frameControls_ = nullptr;
    QSlider *frameSlider_ = nullptr;
    QSpinBox *frameNumber_ = nullptr;
    QLabel *frameCountLabel_ = nullptr;
    QLabel *frameTimeLabel_ = nullptr;
    QToolButton *playFramesButton_ = nullptr;
    QTimer *frameTimer_ = nullptr;

    QAction *openAction_ = nullptr;
    QAction *solveAction_ = nullptr;
    QAction *viewResultsAction_ = nullptr;
    QAction *pickNodeAction_ = nullptr;
    QAction *pickElementAction_ = nullptr;

    Workspace workspace_ = Workspace::Preprocess;
    bool solving_ = false;
    bool meshOnlyPreview_ = false;
    bool palettesPlaced_ = false;
    QString workingDirectory_;
    QString inputFilePath_;
    QString resultFilePath_;
    QByteArray inputFingerprint_;
    QByteArray displacementFingerprint_;
    FEModel currentModel_;
    vtkSmartPointer<vtkUnstructuredGrid> modelGrid_;
    vtkSmartPointer<vtkUnstructuredGrid> resultGrid_;
    std::vector<DisplacementFrame> resultFrames_;
    ElasticResultIO::NodalHistory nodalFrames_;
};
