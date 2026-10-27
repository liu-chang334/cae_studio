#pragma once

#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>

class QWidget;
class vtkScene;
class vtkUnstructuredGrid;
class vtkOrientationMarkerWidget;

class MeshViewport : public QVTKOpenGLNativeWidget {
public:
    explicit MeshViewport(QWidget *parent = nullptr);
    ~MeshViewport() override;

    void setMesh(vtkUnstructuredGrid *grid);
    void clear();
    void resetCamera();

private:
    vtkScene *scene_ = nullptr;
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationWidget_ = nullptr;
};