#pragma once

#include <QVTKOpenGLNativeWidget.h>
#include <vtkUnstructuredGrid.h>

class vtkScene;

class MeshViewport : public QVTKOpenGLNativeWidget {
public:
    explicit MeshViewport(QWidget *parent = nullptr);
    ~MeshViewport() override;

    void setMesh(vtkUnstructuredGrid *grid);
    void clearMesh();
    void resetCamera();

private:
    vtkScene *scene_ = nullptr;
};