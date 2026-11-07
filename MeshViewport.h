#pragma once

#include "MeshData.h"

#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class QWidget;
class QVariantAnimation;
class vtkScene;
class vtkUnstructuredGrid;
class vtkOrientationMarkerWidget;

/**
 * @brief vtk视口
 * @details 显示网格, 并提供交互功能
 */
class MeshViewport : public QVTKOpenGLNativeWidget {
    Q_OBJECT
public:
    explicit MeshViewport(QWidget *parent = nullptr);
    ~MeshViewport() override;

    /**
     * @brief 添加网格
     * @param grid 网格数据
     * @details 显示网格, 并开始导入动画
     */
    void setMesh(vtkUnstructuredGrid *grid);
    void clear();
    void resetCamera();

    void setNodePickingEnabled(bool enabled);

signals:
    /**
     * @brief 选中一个单元
     * @param elementId 原始单元编号
     */
    void elementPicked(MeshId elementId);

    /**
     * @brief 选中一个节点
     * @param nodeId 原始节点编号
     */
    void nodePicked(MeshId nodeId);

    /**
     * @brief 点击空白, 取消选择
     */
    void selectionCleared();

private:
    /**
     * @brief 更新导入进度
     * @param value 导入进度值
     */
    void updateImportProgress(const QVariant &value);

    /**
     * @brief 更新边线传播进度
     * @param value 边线传播进度值
     */
    void updateEdgeProgress(const QVariant &value);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint pressPosition_;

    /// @brief 导入动画
    QVariantAnimation *importAnimation_ = nullptr;

    /// @brief 边线传播动画
    QVariantAnimation *edgeAnimation_ = nullptr;

    bool clickCandidate_ = false;
    bool nodePickingEnabled_ = false;

    vtkScene *scene_ = nullptr;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget_ = nullptr;
};