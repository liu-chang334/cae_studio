#include "MeshViewport.h"
#include "vtkScene.h"

#include <algorithm>

#include <QApplication>
#include <QMouseEvent>
#include <QVariantAnimation.h>

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

    // 导入动画
    importAnimation_ = new QVariantAnimation(this);
    importAnimation_->setDuration(1800);
    importAnimation_->setStartValue(0.0);
    importAnimation_->setEndValue(1.0);
    importAnimation_->setEasingCurve(QEasingCurve::InOutCubic);
    connect(importAnimation_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                scene_->setMeshRevealProgress(value.toDouble());
                renderWindow()->Render();
            });

    // 边线传播动画，相位匀速推进并无缝循环
    edgeAnimation_ = new QVariantAnimation(this);
    edgeAnimation_->setDuration(6000);
    edgeAnimation_->setStartValue(0.0);
    edgeAnimation_->setEndValue(1.0);
    edgeAnimation_->setLoopCount(-1);
    edgeAnimation_->setEasingCurve(QEasingCurve::Linear);
    connect(edgeAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        // 仅首次启动时用约 720 毫秒引入波纹，后续周期保持连续。
        const double phase = value.toDouble();
        const double ramp = edgeAnimation_->currentLoop() == 0
            ? std::min(phase / 0.12, 1.0) : 1.0;
        const double strength = ramp * ramp * (3.0 - 2.0 * ramp);
        scene_->setEdgeWavePhase(phase, strength);
        renderWindow()->Render();
    });
    connect(importAnimation_, &QVariantAnimation::finished, edgeAnimation_,
            [this]() { edgeAnimation_->start(); });
}

MeshViewport::~MeshViewport() {
    edgeAnimation_->stop();
    importAnimation_->stop();
    orientationWidget_->SetEnabled(0);
    orientationWidget_->SetInteractor(nullptr);
    orientationWidget_ = nullptr;
    delete scene_;
}

void MeshViewport::setMesh(vtkUnstructuredGrid *grid) {
    if (!grid)
        return;
    edgeAnimation_->stop();
    importAnimation_->stop();
    scene_->setMesh(grid);
    scene_->setMeshRevealProgress(0.0);
    renderWindow()->Render();
    importAnimation_->start();
}

void MeshViewport::clear() {
    edgeAnimation_->stop();
    importAnimation_->stop();
    scene_->clear();
    renderWindow()->Render();
}

void MeshViewport::resetCamera() {
    scene_->resetCamera();
    renderWindow()->Render();
}

void MeshViewport::mousePressEvent(QMouseEvent *event) {
    if (importAnimation_->state() == QAbstractAnimation::Running) {
        importAnimation_->stop();
        scene_->setMeshRevealProgress(1.0);
        edgeAnimation_->start();
        renderWindow()->Render();
    }
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

    if (nodePickingEnabled_) {
        const auto nodeId = scene_->pickNode(x, y);
        renderWindow()->Render();

        if (nodeId)
            emit nodePicked(*nodeId);
        else
            emit selectionCleared();
        return;
    }

    const auto elementId = scene_->pickElement(x, y);
    renderWindow()->Render(); // 刷新渲染, 高亮选中单元

    if (elementId)
        emit elementPicked(*elementId);
    else
        emit selectionCleared();
}

void MeshViewport::setNodePickingEnabled(bool enabled) {
    if (nodePickingEnabled_ == enabled)
        return;
    nodePickingEnabled_ = enabled;
    scene_->clearHighlight();
    renderWindow()->Render();
    emit selectionCleared();
}
