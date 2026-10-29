#include "MeshViewport.h"
#include "vtkScene.h"

#include <QApplication>
#include <QMouseEvent>

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

void MeshViewport::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        pressPosition_ = event->position().toPoint();
        clickCandidate_ = true;
    }
    QVTKOpenGLNativeWidget::mousePressEvent(event);
}

void MeshViewport::mouseMoveEvent(QMouseEvent *event) {
    if (clickCandidate_) {
        const QPoint delta = event->position().toPoint() - pressPosition_;
        if (delta.manhattanLength() >= QApplication::startDragDistance())
            clickCandidate_ = false;
    }
    QVTKOpenGLNativeWidget::mouseMoveEvent(event);
}

void MeshViewport::mouseReleaseEvent(QMouseEvent *event) {
    QVTKOpenGLNativeWidget::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
        return;
    const QPoint delta = event->position().toPoint() - pressPosition_;
    const bool isClick =
        clickCandidate_ && delta.manhattanLength() < QApplication::startDragDistance();
    clickCandidate_ = false;
    if (!isClick)
        return;

    const double scale = effectiveDevicePixelRatio();
    const int x = qRound(event->position().x() * scale);
    const int yFromTop = qRound(event->position().y() * scale);

    const int height = renderWindow()->GetSize()[1];
    const int y = height - 1 - yFromTop;
    const auto elementId = scene_->pickElement(x, y);

    if (elementId)
        emit elementPicked(*elementId);
    else
        emit selectionCleared();
}