#pragma once

#include "FEModel.h"

#include <QMainWindow>
#include <vtkSmartPointer.h>

class MeshViewport;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QTableWidget;
class QToolBar;
class QStackedWidget;
class QWidget;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QAction;

class vtkUnstructuredGrid;

/**
 * @brief 程序主窗口
 */
class MainWindow : public QMainWindow {
    /**
     * @brief 定义宏,使signal/slot机制生效
     */
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow() override = default;

private:
    enum class Workspace { Preprocess = 0, Solve = 1, Postprocess = 2 };

private slots:
    /**
     * @brief 打开文件
     */
    void onOpenFile();

    /**
     * @brief 求解
     */
    void onSolve();

    /**
     * @brief 更新后处理
     */
    void updateResultDisplay();

private:
    void setupModelPanel();
    void setupInfoTable();
    void setupLogWidget();
    void setupWorkspacePanels();
    void setupCentral();
    void setupToolBar();
    void updateModelTree();
    void showElementInfo(MeshId elementId);
    void showNodeInfo(MeshId nodeId);
    void switchWorkspace(Workspace workspace);
    void updateActionStates();

private:
    QTreeWidget *modelTree_ = nullptr;
    QTreeWidgetItem *nodeItem_ = nullptr;
    QTreeWidgetItem *elemItem_ = nullptr;
    QTreeWidgetItem *nsetItem_ = nullptr;
    QTreeWidgetItem *esetItem_ = nullptr;
    QTreeWidgetItem *materialItem_ = nullptr;
    QTreeWidgetItem *boundaryItem_ = nullptr;
    QTreeWidgetItem *loadItem_ = nullptr;

    QPlainTextEdit *logWidget_ = nullptr;
    QTableWidget *infoTable_ = nullptr;
    MeshViewport *viewport_ = nullptr;

    QToolBar *toolBar_ = nullptr;
    QToolBar *workspaceToolBar_ = nullptr;
    QTabBar *workspaceTabs_ = nullptr;

    QStackedWidget *navigationStack_ = nullptr;
    QWidget *solvePanel_ = nullptr;
    QWidget *resultPanel_ = nullptr;

    QLabel *solveStatusLabel_ = nullptr;
    QPushButton *solveButton_ = nullptr;
    QPushButton *viewResultsButton_ = nullptr;

    QComboBox *fieldCombo_ = nullptr;
    QComboBox *componentCombo_ = nullptr;
    QCheckBox *contourCheck_ = nullptr;
    QCheckBox *legendCheck_ = nullptr;

    QAction *openAction_ = nullptr;
    QAction *solveAction_ = nullptr;
    QAction *viewResultsAction_ = nullptr;
    QAction *pickNodeAction_ = nullptr;
    QAction *pickElementAction_ = nullptr;

    Workspace workspace_ = Workspace::Preprocess;
    bool solving_ = false;

    FEModel currentModel_;
    vtkSmartPointer<vtkUnstructuredGrid> modelGrid_;
    vtkSmartPointer<vtkUnstructuredGrid> resultGrid_;
};
