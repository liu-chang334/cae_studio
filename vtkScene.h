#pragma once
#include <vtkSmartPointer.h>

class vtkActor;
class vtkRenderer;
class vtkUnstructuredGrid;

/**
 * @brief vtk场景类
 * @details 管理vtk场景, 场景对象, 渲染器, 相机
 */
class vtkScene {
public:
    vtkScene();
    ~vtkScene() = default;

    vtkRenderer *renderer() const;

    void setMesh(vtkUnstructuredGrid *grid);
    void clear();
    void resetCamera();

private:
    vtkSmartPointer<vtkRenderer> renderer_;
    vtkSmartPointer<vtkActor> meshActor_;
    vtkSmartPointer<vtkUnstructuredGrid> grid_;
};
