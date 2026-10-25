#pragma once
#include <vtkSmartPointer.h>

class vtkActor;
class vtkRenderer;
class vtkUnstructuredGrid;

class vtkScene {
public:
    vtkScene();
    ~vtkScene() = default;

    vtkRenderer *renderer() const;

    void setMesh(vtkUnstructuredGrid *grid);
    void clearMesh();
    void resetCamera();

private:
    vtkSmartPointer<vtkRenderer> renderer_;
    vtkSmartPointer<vtkActor> meshActor_;
    vtkSmartPointer<vtkUnstructuredGrid> grid_;
};
