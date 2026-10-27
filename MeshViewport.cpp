#include "MeshViewport.h"
#include "vtkScene.h"

#include <vtkAxesActor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkOrientationMarkerWidget.h>

MeshViewport::MeshViewport(QWidget *parent)
    : QVTKOpenGLNativeWidget(parent), scene_(new vtkScene()) {
    auto window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    setRenderWindow(window);
    window->AddRenderer(scene_->renderer());

    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    orientationWidget_ = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationWidget_->SetOrientationMarker(axes);
    orientationWidget_->SetInteractor(interactor());
    orientationWidget_->SetCurrentRenderer(scene_->renderer());
    orientationWidget_->SetViewport(0.80, 0.00, 1.00, 0.20);
    orientationWidget_->SetEnabled(1);
    orientationWidget_->InteractiveOff();
}

MeshViewport::~MeshViewport() {
    orientationWidget_->SetEnabled(0);
    orientationWidget_->SetInteractor(nullptr);
    orientationWidget_ = nullptr;
    delete scene_;
}

void MeshViewport::setMesh(vtkUnstructuredGrid *grid) {
    if (!grid)
        return;
    scene_->setMesh(grid);
    renderWindow()->Render();
}

void MeshViewport::clear() {
    scene_->clear();
    renderWindow()->Render();
}

void MeshViewport::resetCamera() {
    scene_->resetCamera();
    renderWindow()->Render();
}