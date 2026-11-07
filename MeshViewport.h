#pragma once

#include "MeshData.h"

#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class QWidget;
class QVariantAnimation;
class vtkScene;
class vtkUnstructuredGrid;
class vtkOrientationMarkerWidget;

class MeshViewport : public QVTKOpenGLNativeWidget {
    Q_OBJECT
public:
    explicit MeshViewport(QWidget *parent = nullptr);
    ~MeshViewport() override;

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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint pressPosition_;

    /// @brief 导入动画
    QVariantAnimation *importAnimation_ = nullptr;

    /// @brief 边线呼吸动画
    QVariantAnimation *edgeAnimation_ = nullptr;

    bool clickCandidate_ = false;
    bool nodePickingEnabled_ = false;

    vtkScene *scene_ = nullptr;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget_ = nullptr;
};