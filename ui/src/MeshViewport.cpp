#include "MeshViewport.h"
#include "vtkScene.h"

#include <algorithm>

#include <QApplication>
#include <QMouseEvent>
#include <QVariantAnimation.h>

#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkTextProperty.h>

MeshViewport::MeshViewport(QWidget *parent) : QVTKOpenGLNativeWidget(parent), scene_(new vtkScene()) {
    auto window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    setRenderWindow(window);
    window->AddRenderer(scene_->renderer());
    // 添加坐标轴
    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    // 获取三个轴标签的文字属性
    auto *xText = axes->GetXAxisCaptionActor2D()->GetCaptionTextProperty();
    auto *yText = axes->GetYAxisCaptionActor2D()->GetCaptionTextProperty();
    auto *zText = axes->GetZAxisCaptionActor2D()->GetCaptionTextProperty();
    xText->SetColor(0.0, 0.0, 0.0);
    yText->SetColor(0.0, 0.0, 0.0);
    zText->SetColor(0.0, 0.0, 0.0);
    xText->ShadowOff();
    yText->ShadowOff();
    zText->ShadowOff();
    orientationWidget_ = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationWidget_->SetOrientationMarker(axes);
    orientationWidget_->SetInteractor(interactor());
    orientationWidget_->SetCurrentRenderer(scene_->renderer());
    // 底部留给渐变日志层，坐标标记移至右上角。
    orientationWidget_->SetViewport(0.82, 0.80, 1.00, 0.98);
    orientationWidget_->SetEnabled(1);
    orientationWidget_->InteractiveOff();
    // 导入动画配置
    importAnimation_ = new QVariantAnimation(this);
    importAnimation_->setDuration(1800);  // 动画持续时间, ms
    importAnimation_->setStartValue(0.0);  // 动画起始值
    importAnimation_->setEndValue(1.0);  // 动画结束值
    importAnimation_->setEasingCurve(QEasingCurve::InOutCubic);  // 动画曲线
    connect(importAnimation_, &QVariantAnimation::valueChanged, this,
            &MeshViewport::updateImportProgress);  // 连接导入动画进度更新信号
    // 边线传播动画，相位匀速推进并无缝循环
    edgeAnimation_ = new QVariantAnimation(this);
    edgeAnimation_->setDuration(6000);
    edgeAnimation_->setStartValue(0.0);
    edgeAnimation_->setEndValue(1.0);
    edgeAnimation_->setLoopCount(-1);  // 无限循环
    edgeAnimation_->setEasingCurve(QEasingCurve::Linear);
    connect(edgeAnimation_, &QVariantAnimation::valueChanged, this,
            &MeshViewport::updateEdgeProgress);  // 连接边线传播动画更新信号
    connect(importAnimation_, &QVariantAnimation::finished, edgeAnimation_,
            [this]() { edgeAnimation_->start(); });  // 把边线动画开始和导入结束衔接
}

MeshViewport::~MeshViewport() {
    edgeAnimation_->stop();
    importAnimation_->stop();
    orientationWidget_->SetEnabled(0);
    orientationWidget_->SetInteractor(nullptr);
    orientationWidget_ = nullptr;
    delete scene_;
}

void MeshViewport::setMesh(vtkUnstructuredGrid *grid, bool animateImport, bool resetView) {
    if (!grid)
        return;

    edgeAnimation_->stop();
    importAnimation_->stop();

    scene_->setMesh(grid, resetView);

    if (animateImport) {
        scene_->setMeshRevealProgress(0.0);
        importAnimation_->start();
        // 导入结束后，已有的 finished 连接会启动边线动画。
        renderWindow()->Render();
    } else {
        scene_->setMeshRevealProgress(1.0);
        edgeAnimation_->start();
        updateEdgeProgress(edgeAnimation_->currentValue());
    }
}

bool MeshViewport::setContour(const char *arrayName, const char *title, bool showLegend, const char *legendFooter,
                              const double *scalarRange) {
    importAnimation_->stop();
    // 完整显示网格后，恢复被清零的波浪强度。
    scene_->setMeshRevealProgress(1.0);

    const bool success = scene_->setContour(arrayName, title, showLegend, legendFooter, scalarRange);

    // 切换结果时保留正在运行的动画相位。
    if (success && edgeAnimation_->state() != QAbstractAnimation::Running)
        edgeAnimation_->start();

    if (edgeAnimation_->state() == QAbstractAnimation::Running)
        updateEdgeProgress(edgeAnimation_->currentValue());
    else
        renderWindow()->Render();

    return success;
}

void MeshViewport::setLegendBottomInset(double inset) { scene_->setLegendBottomInset(inset); }

void MeshViewport::clearContour() {
    const bool wasImporting = importAnimation_->state() == QAbstractAnimation::Running;
    importAnimation_->stop();

    scene_->clearContour();

    // 若打断了导入动画，直接接续边线动画。
    if (wasImporting && edgeAnimation_->state() != QAbstractAnimation::Running)
        edgeAnimation_->start();

    // 关闭云图也会清零波浪强度，立即恢复当前动画帧。
    if (edgeAnimation_->state() == QAbstractAnimation::Running)
        updateEdgeProgress(edgeAnimation_->currentValue());
    else
        renderWindow()->Render();
}

void MeshViewport::clear() {
    edgeAnimation_->stop();  // 停止边线传播动画
    importAnimation_->stop();  // 停止导入动画
    scene_->clear();  // 清除场景
    renderWindow()->Render();  // 刷新场景
}

void MeshViewport::resetCamera() {
    scene_->resetCamera();  // 重置相机
    renderWindow()->Render();  // 刷新场景
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
    const bool isClick = clickCandidate_ && delta.manhattanLength() < QApplication::startDragDistance();
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
    renderWindow()->Render();  // 刷新渲染, 高亮选中单元
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

void MeshViewport::updateImportProgress(const QVariant &value) {
    const double progress = value.toDouble();
    scene_->setMeshRevealProgress(progress);  // 更新导入进度
    renderWindow()->Render();  // 刷新渲染
}

void MeshViewport::updateEdgeProgress(const QVariant &value) {
    const double phase = value.toDouble();  // 获取当前相位
    double ramp = 1.0;
    if (edgeAnimation_->currentLoop() == 0)
        ramp = std::min(phase / 0.12, 1.0);  // 首轮幅度渐强
    const double strength = ramp * ramp * (3.0 - 2.0 * ramp);  // 稳定后恒为1
    scene_->setEdgeWavePhase(phase, strength);
    renderWindow()->Render();
}
