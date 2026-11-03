#include "vtkScene.h"

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkDataSetMapper.h>
#include <vtkExtractCells.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnstructuredGrid.h>

vtkScene::vtkScene() : renderer_(vtkSmartPointer<vtkRenderer>::New()) {
    renderer_->GradientBackgroundOn();
    renderer_->SetBackground(0.8, 0.8, 0.8);
    renderer_->SetBackground2(1.0, 1.0, 1.0);
}

vtkRenderer *vtkScene::renderer() const { return renderer_.GetPointer(); }

void vtkScene::setMesh(vtkUnstructuredGrid *grid) {
    if (!grid)
        return;
    // 先准备新对象, 再替换旧对象
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.8, 0.8, 0.9);
    actor->GetProperty()->EdgeVisibilityOn();
    actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.3);
    actor->GetProperty()->SetLineWidth(1.5);
    clear();
    grid_ = grid;
    meshActor_ = actor;
    renderer_->AddActor(meshActor_);
    resetCamera();
}

void vtkScene::clear() {
    clearHighlight();

    if (meshActor_)
        renderer_->RemoveActor(meshActor_);
    meshActor_ = nullptr;
    grid_ = nullptr;
}

void vtkScene::resetCamera() { renderer_->ResetCamera(); }

std::optional<MeshId> vtkScene::pickElement(int x, int y) {
    clearHighlight();

    if (!grid_ || !meshActor_)
        return std::nullopt;

    auto picker = vtkSmartPointer<vtkCellPicker>::New();
    picker->PickFromListOn();
    picker->AddPickList(meshActor_);

    if (!picker->Pick(x, y, 0.0, renderer_))
        return std::nullopt;

    const vtkIdType cellId = picker->GetCellId(); // vtk内部单元索引
    auto *elementIds =
        vtkTypeInt64Array::SafeDownCast(grid_->GetCellData()->GetArray("ElementIds"));
    if (!elementIds || cellId < 0 || cellId >= elementIds->GetNumberOfValues())
        return std::nullopt;

    highlightCell(cellId);
    return static_cast<MeshId>(elementIds->GetValue(cellId)); // 原始单元编号
}

void vtkScene::clearHighlight() {
    if (highlightActor_) {
        renderer_->RemoveActor(highlightActor_);
        highlightActor_ = nullptr;
    }
}

void vtkScene::highlightCell(vtkIdType cellId) {
    // 从当前vtk网格中提取一个单元
    auto extract = vtkSmartPointer<vtkExtractCells>::New();
    extract->SetInputData(grid_);
    extract->AddCellIds(&cellId, 1);
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputConnection(extract->GetOutputPort());
    mapper->ScalarVisibilityOff();

    // 给重叠表面设置深度偏移, 减少闪烁
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0.0, -4.0);

    highlightActor_ = vtkSmartPointer<vtkActor>::New();
    highlightActor_->SetMapper(mapper);
    highlightActor_->GetProperty()->SetColor(1.0, 0.05, 0.0);
    highlightActor_->PickableOff();
    renderer_->AddActor(highlightActor_);
}