#include "MeshViewport.h"
#include "vtkScene.h"

#include <vtkGenericOpenGLRenderWindow.h>

MeshViewport::MeshViewport(QWidget *parent)
    : QVTKOpenGLNativeWidget(parent), scene_(new vtkScene()) {
    auto window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    setRenderWindow(window);
    window->AddRenderer(scene_->renderer());
}

MeshViewport::~MeshViewport() { delete scene_; }

void MeshViewport::setMesh(vtkUnstructuredGrid *grid) {
    if (!grid)
        return;
    scene_->setMesh(grid);
    renderWindow()->Render();
}

void MeshViewport::clearMesh() {
    scene_->clearMesh();
    renderWindow()->Render();
}

void MeshViewport::resetCamera() {
    scene_->resetCamera();
    renderWindow()->Render();
}