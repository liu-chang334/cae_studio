#pragma once

#include <QFileDialog>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

class MeshViewport;

/**
 * @brief 程序主窗口
 */
class MainWindow : public QMainWindow {
    ///@brief 定义宏,使signal/slot机制生效
    Q_OBJECT

public:
    /// @brief 构造函数
    /// @param parent 父窗口指针
    explicit MainWindow(QWidget *parent = nullptr);

    /// @brief 析构函数
    ~MainWindow() = default;

private slots:
    /// @brief 打开文件
    void onOpenFile();

    /// @brief 切换线框模式
    /// @param checked true 表示启用,false 表示关闭
    // void onToggleWireframe(bool checked);

    /// @brief 切换缩放模式
    /// @param checked true 表示启用,false 表示关闭
    // void onToggleZoom(bool checked);

private:
    /// @brief 初始化工具栏
    void setupToolBar();

    /// @brief 初始化中心区域
    void setupCentral();

    /// @brief 初始化模型面板
    void setupModelPanel();

    /// @brief 初始化信息表格
    void setupInfoTable();

    /// @brief 初始化日志窗口
    void setupLogWidget();

    /// @brief 初始化悬停选择
    // void setupHoverPicking();

    /// @brief 初始化HUD栏
    // void setupHudBar();

    /// @brief 更新信息表格
    // void updateInfoTable();

    /// @brief 加载 ABAQUS inp 文件
    /// @param path 文件路径
    /// @return true 表示加载成功,false 表示加载失败
    bool loadAbaqusInp(const QString &path);

    /// @brief 显示网格
    void showGrid();

private:
    /// @brief 网格
    vtkSmartPointer<vtkUnstructuredGrid> grid_ = nullptr;

    /// @brief 模型树
    QTreeWidget *modelTree_ = nullptr;

    /// @brief vtk渲染窗口
    QVTKOpenGLNativeWidget *vtkWidget_ = nullptr;

    /// @brief 日志窗口
    QPlainTextEdit *logWidget_ = nullptr;

    /// @brief 信息表格
    QTableWidget *infoTable_ = nullptr;

    /// @brief 工具条
    QToolBar *toolBar_ = nullptr;

    /// @brief 网格视口
    MeshViewport *viewport_ = nullptr;
};
