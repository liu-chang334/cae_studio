#pragma once

#include "MeshData.h"

#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class QWidget;
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

signals:
    /**
     * @brief 选中一个单元
     * @param elementId 原始单元编号
     */
    void elementPicked(MeshId elementId);

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
    bool clickCandidate_ = false;

    vtkScene *scene_ = nullptr;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget_ = nullptr;
};