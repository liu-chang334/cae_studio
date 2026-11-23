#include "vtkScene.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkCellType.h>
#include <vtkCoordinate.h>
#include <vtkDataSetMapper.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkDoubleArray.h>
#include <vtkExtractCells.h>
#include <vtkExtractEdges.h>
#include <vtkLookupTable.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPointData.h>
#include <vtkPointPicker.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkShaderProperty.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTubeFilter.h>
#include <vtkTypeInt64Array.h>
#include <vtkUniforms.h>
#include <vtkUnstructuredGrid.h>

vtkScene::vtkScene() : renderer_(vtkSmartPointer<vtkRenderer>::New()) {
    renderer_->GradientBackgroundOn();  // 渐变背景
    renderer_->SetBackground(0.95, 0.975, 0.98);  // 下方柔和冷白，与日志渐变衔接
    renderer_->SetBackground2(1.0, 1.0, 1.0);  // 上方白色
    // renderer_->SetBackground(0.043, 0.071, 0.125);
    // renderer_->SetBackground2(0.106, 0.169, 0.251);
}

vtkRenderer *vtkScene::renderer() const { return renderer_.GetPointer(); }

void vtkScene::setMesh(vtkUnstructuredGrid *grid, bool resetView) {
    if (!grid)
        return;

    vtkSmartPointer<vtkUnstructuredGrid> incoming = grid;
    clear();
    grid_ = incoming;
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(grid_);
    mapper->ScalarVisibilityOff();
    meshActor_ = vtkSmartPointer<vtkActor>::New();
    meshActor_->SetMapper(mapper);
    meshActor_->GetProperty()->EdgeVisibilityOff();
    renderer_->AddActor(meshActor_);
    buildMeshEdges(true);
    setMeshRevealProgress(1.0);
    if (resetView)
        resetCamera();
    else
        renderer_->ResetCameraClippingRange();
}

bool vtkScene::setContour(const char *arrayName, const char *title, bool showLegend, const char *legendFooter) {
    if (!grid_ || !meshActor_ || !arrayName)
        return false;
    auto *mapper = vtkDataSetMapper::SafeDownCast(meshActor_->GetMapper());
    auto *array = grid_->GetPointData()->GetArray(arrayName);
    if (!mapper || !array || array->GetNumberOfComponents() != 1 || array->GetNumberOfTuples() != grid_->GetNumberOfPoints() ||
        array->GetNumberOfTuples() == 0) {
        return false;
    }
    // 1. 获取当前选中结果的范围
    double range[2];
    array->GetRange(range);
    if (!std::isfinite(range[0]) || !std::isfinite(range[1]) || range[0] > range[1])
        return false;
    // 常量场：适当扩展范围，避免颜色控制点全部重合
    if (range[0] == range[1]) {
        const double padding = std::max(std::abs(range[0]) * 1.0e-6, 1.0e-12);
        range[0] -= padding;
        range[1] += padding;
    }
    constexpr int colorLevels = 13;
    const double minValue = range[0];
    const double maxValue = range[1];
    const double span = maxValue - minValue;
    const double midValue = minValue + span * 0.5;
    // 2. 保留原来的蓝 -> 白 -> 红配色
    auto colors = vtkSmartPointer<vtkDiscretizableColorTransferFunction>::New();
    colors->SetColorSpaceToLab();
    const double colorSaturation = 1.55;
    auto addColor = [&](double t, double r, double g, double b) {
        const double peak = std::max({r, g, b});
        auto saturate = [&](double channel) { return std::clamp(peak - (peak - channel) * colorSaturation, 0.0, 1.0); };
        colors->AddRGBPoint(minValue + t * span, saturate(r), saturate(g), saturate(b));
    };
    addColor(0.00, 0.25, 0.44, 0.70);  // 柔和蓝
    addColor(0.20, 0.47, 0.66, 0.82);  // 中蓝
    addColor(0.40, 0.79, 0.87, 0.93);  // 浅蓝
    // 柔白中心保持不变
    colors->AddRGBPoint(midValue, 0.96, 0.96, 0.95);
    addColor(0.60, 0.96, 0.84, 0.79);  // 浅红
    addColor(0.80, 0.87, 0.59, 0.51);  // 中红
    addColor(1.00, 0.72, 0.32, 0.31);  // 柔和红
    colors->DiscretizeOn();
    colors->SetNumberOfValues(colorLevels);
    colors->Build();
    // 3. 使用当前选中的结果数组着色
    mapper->SetLookupTable(colors);
    mapper->UseLookupTableScalarRangeOn();
    mapper->SetScalarModeToUsePointFieldData();
    // 由参数决定数组，不再固定为总位移
    mapper->SelectColorArray(arrayName);
    mapper->InterpolateScalarsBeforeMappingOn();
    mapper->ScalarVisibilityOn();
    meshActor_->GetProperty()->LightingOff();
    // 4. 色标只创建一次，切换结果时复用
    if (!scalarBar_) {
        scalarBar_ = vtkSmartPointer<vtkScalarBarActor>::New();
        scalarBar_->SetOrientationToVertical();
        scalarBar_->SetBarRatio(0.25);
        scalarBar_->SetMaximumNumberOfColors(colorLevels);
        scalarBar_->SetNumberOfLabels(12);
        scalarBar_->UnconstrainedFontSizeOn();
        // 标题：Times New Roman 粗斜体
        auto *titleText = scalarBar_->GetTitleTextProperty();
        titleText->SetFontFamily(VTK_FONT_FILE);
        titleText->SetFontFile("C:/Windows/Fonts/timesbi.ttf");
        titleText->BoldOn();
        titleText->ItalicOn();
        titleText->SetFontSize(16);
        titleText->SetColor(0.0, 0.0, 0.0);
        titleText->ShadowOff();
        // 数值标签：Times New Roman 粗斜体
        auto *labelText = scalarBar_->GetLabelTextProperty();
        labelText->SetFontFamily(VTK_FONT_FILE);
        labelText->SetFontFile("C:/Windows/Fonts/timesbi.ttf");
        labelText->BoldOn();
        labelText->ItalicOn();
        labelText->SetFontSize(16);
        labelText->SetColor(0.0, 0.0, 0.0);
        labelText->ShadowOff();
        renderer_->AddActor2D(scalarBar_);

        legendFooter_ = vtkSmartPointer<vtkTextActor>::New();
        legendFooter_->SetTextScaleModeToNone();
        legendFooter_->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        auto *footerText = legendFooter_->GetTextProperty();
        footerText->ShallowCopy(labelText);
        footerText->SetJustificationToCentered();
        footerText->SetVerticalJustificationToTop();
        renderer_->AddActor2D(legendFooter_);
    }
    // 5. 每次切换结果都更新标题、颜色映射与可见性
    scalarBar_->SetTitle(title ? title : arrayName);
    scalarBar_->SetLookupTable(mapper->GetLookupTable());
    scalarBar_->SetVisibility(showLegend);
    legendFooter_->SetInput(legendFooter ? legendFooter : "");
    legendFooter_->SetVisibility(showLegend && legendFooter && *legendFooter);
    updateLegendPosition();

    return true;
}

void vtkScene::setLegendBottomInset(double inset) {
    legendBottomInset_ = inset;
    if (scalarBar_)
        updateLegendPosition();
}

void vtkScene::updateLegendPosition() {
    constexpr double defaultLeft = 0.87;
    constexpr double width = 0.10;
    constexpr double rightPadding = 0.03;
    constexpr double top = 0.75;
    constexpr double defaultBottom = 0.20;
    constexpr double footerSpace = 0.07;
    constexpr double footerGap = 0.012;
    const bool showFooter = legendFooter_->GetVisibility();
    const double bottom = showFooter ? std::max(defaultBottom, legendBottomInset_ + footerSpace) : defaultBottom;
    double center = defaultLeft + width * 0.5;
    const int viewportWidth = renderer_->GetSize()[0];
    if (showFooter && viewportWidth > 0) {
        double textSize[2];
        legendFooter_->GetSize(renderer_, textSize);
        const double halfTextWidth = textSize[0] / (2.0 * viewportWidth);
        center = std::min(center, std::max(width * 0.5, 1.0 - rightPadding - halfTextWidth));
    }
    scalarBar_->SetPosition(center - width * 0.5, bottom);
    scalarBar_->SetWidth(width);
    scalarBar_->SetHeight(top - bottom);
    legendFooter_->SetPosition(center, bottom - footerGap);
}

void vtkScene::clearContour() {
    if (scalarBar_)
        scalarBar_->SetVisibility(false);
    if (legendFooter_)
        legendFooter_->SetVisibility(false);
    if (!meshActor_)
        return;
    auto *mapper = vtkDataSetMapper::SafeDownCast(meshActor_->GetMapper());
    if (mapper)
        mapper->ScalarVisibilityOff();
    meshActor_->GetProperty()->LightingOn();
    setMeshRevealProgress(1.0);
}

void vtkScene::clear() {
    // 1. 清除高亮
    clearHighlight();
    // 2. 移除渲染器中的所有对象
    if (meshActor_)
        renderer_->RemoveActor(meshActor_);
    if (edgeActor_)
        renderer_->RemoveActor(edgeActor_);
    if (edgeWaveActor_)
        renderer_->RemoveActor(edgeWaveActor_);
    if (scalarBar_) {
        renderer_->RemoveActor2D(scalarBar_);
        scalarBar_ = nullptr;
    }
    if (legendFooter_) {
        renderer_->RemoveActor2D(legendFooter_);
        legendFooter_ = nullptr;
    }
    // 3. 重置持有的所有指针
    meshActor_ = nullptr;
    grid_ = nullptr;
    edgeActor_ = nullptr;
    edgeWaveActor_ = nullptr;
}

void vtkScene::resetCamera() { renderer_->ResetCamera(); }

std::optional<MeshId> vtkScene::pickElement(int x, int y) {
    // 1. 清除原有高亮对象
    clearHighlight();
    // 2. 检查网格是否存在
    if (!grid_ || !meshActor_)
        return std::nullopt;
    // 3. 选择单元
    auto picker = vtkSmartPointer<vtkCellPicker>::New();
    picker->AddPickList(meshActor_);  // 添加场景对象到选择列表
    picker->PickFromListOn();  // 仅从列表中选择
    if (!picker->Pick(x, y, 0.0, renderer_))  // 选择单元
        return std::nullopt;  // 未选中单元
    const vtkIdType cellId = picker->GetCellId();  // 选中的vtk内部索引
    auto *elementIds = vtkTypeInt64Array::SafeDownCast(grid_->GetCellData()->GetArray("ElementIds"));  // 获取单元原始id数组
    if (!elementIds || cellId < 0 || cellId >= elementIds->GetNumberOfValues())
        return std::nullopt;
    // 4. 高亮选中的单元
    highlightCell(cellId);
    return static_cast<MeshId>(elementIds->GetValue(cellId));  // 返回选中单元的原始编号
}

std::optional<MeshId> vtkScene::pickNode(int x, int y) {
    // 1. 清除原有高亮对象
    clearHighlight();
    // 2. 检查网格是否存在
    if (!grid_ || !meshActor_)
        return std::nullopt;
    // 3. 选择节点
    auto picker = vtkSmartPointer<vtkPointPicker>::New();
    picker->AddPickList(meshActor_);
    picker->PickFromListOn();
    if (!picker->Pick(x, y, 0.0, renderer_))
        return std::nullopt;
    const vtkIdType pointId = picker->GetPointId();
    auto *nodeIds = vtkTypeInt64Array::SafeDownCast(grid_->GetPointData()->GetArray("NodeIds"));
    if (!nodeIds || pointId < 0 || pointId >= nodeIds->GetNumberOfValues())
        return std::nullopt;
    // 4. 高亮选中的节点
    highlightNode(pointId);
    return static_cast<MeshId>(nodeIds->GetValue(pointId));
}

void vtkScene::clearHighlight() {
    if (highlightActor_) {
        renderer_->RemoveActor(highlightActor_);  // 移除渲染器中的高亮对象
        highlightActor_ = nullptr;  // 将持有的高亮对象设为nullptr
    }
}

void vtkScene::highlightCell(vtkIdType cellId) {
    // 1. 装填提取单元对象
    auto extract = vtkSmartPointer<vtkExtractCells>::New();
    extract->SetInputData(grid_);  // 装入原始完整网格
    extract->AddCellIds(&cellId, 1);  // 添加提取对象id
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputConnection(extract->GetOutputPort());  // 将extract提取结果交给mapper
    mapper->ScalarVisibilityOff();  // 不显示标量场
    // 2. 设置深度偏移, 防止深度冲突
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();  // 启用Polygon-Offset模式
    mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0.0, -4.0);  // 设置相对深度偏移
    // 3. 装填场景对象
    highlightActor_ = vtkSmartPointer<vtkActor>::New();
    highlightActor_->SetMapper(mapper);
    highlightActor_->GetProperty()->SetColor(1.0, 0.0, 0.0);  // rgb(255, 0, 0)
    highlightActor_->PickableOff();
    renderer_->AddActor(highlightActor_);
}

void vtkScene::highlightNode(vtkIdType pointId) {
    // 1. 拿坐标
    double xyz[3];
    grid_->GetPoint(pointId, xyz);
    // 2. 在相同位置创建VTK_VERTEX对象充当marker
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    const vtkIdType markerPointId = points->InsertNextPoint(xyz);
    auto marker = vtkSmartPointer<vtkUnstructuredGrid>::New();
    marker->SetPoints(points);
    marker->InsertNextCell(VTK_VERTEX, 1, &markerPointId);
    // 3. 装填映射器
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(marker);
    mapper->ScalarVisibilityOff();
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    mapper->SetRelativeCoincidentTopologyPointOffsetParameter(-4.0);
    // 4. 装填场景对象
    highlightActor_ = vtkSmartPointer<vtkActor>::New();
    highlightActor_->SetMapper(mapper);
    highlightActor_->GetProperty()->SetColor(1.0, 0.0, 0.0);  // rgb(255, 0, 0)
    highlightActor_->GetProperty()->SetPointSize(10.0);  // 点大小
    highlightActor_->PickableOff();
    renderer_->AddActor(highlightActor_);
}

void vtkScene::setMeshRevealProgress(double progress) {
    if (!meshActor_)
        return;
    const double t = std::clamp(progress, 0.0, 1.0);
    double startColor[3] = {255 / 255.0, 255 / 255.0, 255.0 / 255.0};  // rgb(255, 255, 255) 起始颜色
    double surfColor[3] = {17 / 255.0, 128 / 255.0, 134 / 255.0};  // rgb(17, 128, 134)网格目标颜色
    double edgeColor[3] = {0.0 / 255.0, 0.0 / 255.0, 0.0 / 255.0};  // rgb(0, 0, 0)边线目标颜色
    double surface[3];
    double edge[3];
    // 1. 计算当前进度下的表面颜色和边线颜色
    for (int i = 0; i < 3; ++i) {
        surface[i] = startColor[i] + t * (surfColor[i] - startColor[i]);
        edge[i] = startColor[i] + t * (edgeColor[i] - startColor[i]);
    }
    // 2. 设置网格属性
    meshActor_->GetProperty()->SetOpacity(1.0);  // 不透明
    meshActor_->GetProperty()->SetColor(surface);  // 表面颜色
    meshActor_->GetProperty()->SetEdgeColor(edge);  // 边线颜色
    meshActor_->GetProperty()->SetAmbient(1.0 - 0.60 * t);  // 环境光系数
    meshActor_->GetProperty()->SetDiffuse(0.60 * t);  // 漫反射系数
    meshActor_->GetProperty()->SetSpecular(0.12 * t);  // 镜面反射系数
    meshActor_->GetProperty()->SetSpecularPower(24.0);  // 镜面高光集中程度
    // 3. 导入与常驻动画共用边线actor, 保持线宽和深度偏移一致
    if (edgeActor_) {
        edgeActor_->SetVisibility(true);
        edgeActor_->GetProperty()->SetColor(edge);
        edgeWaveActor_->SetVisibility(t >= 1.0);  // 导入完成后才显示波纹
        edgeWaveActor_->GetShaderProperty()->GetVertexCustomUniforms()->SetUniformf("waveStrength", 0.0f);  // 把波纹强度归零
    }
}

void vtkScene::buildMeshEdges(bool showImmediately) {
    // 1. 提取所有边线
    auto extract = vtkSmartPointer<vtkExtractEdges>::New();
    extract->SetInputData(grid_);
    extract->Update();
    auto *edges = extract->GetOutput();
    const vtkIdType count = edges->GetNumberOfPoints();
    if (count == 0 || edges->GetNumberOfLines() == 0)
        return;
    // 2. 构建邻接表
    using Neighbor = std::pair<vtkIdType, double>;  // 相邻点id和距离
    std::vector<std::vector<Neighbor>> adjacency(static_cast<std::size_t>(count));  // 邻接表
    auto *lines = edges->GetLines();  // 获取线单元集合
    vtkIdType size = 0;
    const vtkIdType *ids = nullptr;
    lines->InitTraversal();
    while (lines->GetNextCell(size, ids)) {  // 遍历所有线单元
        for (vtkIdType i = 1; i < size; ++i) {  // 遍历线单元中的所有点
            double a[3], b[3];
            edges->GetPoint(ids[i - 1], a);  // 根据点编号读取两个端点坐标
            edges->GetPoint(ids[i], b);
            const double length = std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);  // 计算两个端点之间的距离
            adjacency[ids[i - 1]].emplace_back(ids[i], length);  // 记录a->b
            adjacency[ids[i]].emplace_back(ids[i - 1], length);  // 记录b->a
        }
    }
    // 3. 计算每个点到所在部件波源的最短边线路径距离
    std::vector<double> distance(static_cast<std::size_t>(count), std::numeric_limits<double>::infinity());
    using Visit = std::pair<double, vtkIdType>;  // 到波源的距离, 点编号
    std::priority_queue<Visit, std::vector<Visit>, std::greater<Visit>> pending;
    for (vtkIdType source = 0; source < count; ++source) {  // 每个部件选自己第一个点作为波源
        if (std::isfinite(distance[source]))
            continue;
        distance[source] = 0.0;
        pending.emplace(0.0, source);
        while (!pending.empty()) {
            const auto [currentDistance, point] = pending.top();
            pending.pop();
            if (currentDistance > distance[point])
                continue;
            for (const auto &[next, length] : adjacency[point]) {
                const double candidate = currentDistance + length;
                if (candidate < distance[next]) {
                    distance[next] = candidate;
                    pending.emplace(candidate, next);
                }
            }
        }
    }
    // 4. 创建普通边线actor
    const double maxDistance = *std::max_element(distance.begin(), distance.end());
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(edges);  // 输入原始边线数据
    mapper->ScalarVisibilityOff();
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();
    mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0.0, -1.0);
    edgeActor_ = vtkSmartPointer<vtkActor>::New();
    edgeActor_->SetMapper(mapper);
    edgeActor_->GetProperty()->SetColor(0.0, 0.0, 0.0);  // rgb(0,0,0)
    edgeActor_->GetProperty()->SetLineWidth(1.5);
    edgeActor_->GetProperty()->LightingOff();
    edgeActor_->PickableOff();  // 禁止此装饰性边线参与拾取
    edgeActor_->SetVisibility(showImmediately);
    renderer_->AddActor(edgeActor_);
    // 5. 创建波纹actor
    const int segments = static_cast<int>(std::clamp<vtkIdType>(200000 / edges->GetNumberOfLines(), 2, 24));
    auto wavePoints = vtkSmartPointer<vtkPoints>::New();
    wavePoints->SetDataTypeToDouble();
    auto waveLines = vtkSmartPointer<vtkCellArray>::New();
    auto waveDistances = vtkSmartPointer<vtkDoubleArray>::New();
    waveDistances->SetName("EdgeWaveDistance");
    auto waveCenters = vtkSmartPointer<vtkDoubleArray>::New();
    waveCenters->SetName("EdgeWaveCenter");
    waveCenters->SetNumberOfComponents(3);
    lines->InitTraversal();
    while (lines->GetNextCell(size, ids)) {
        for (vtkIdType i = 1; i < size; ++i) {
            double a[3], b[3];
            edges->GetPoint(ids[i - 1], a);
            edges->GetPoint(ids[i], b);
            const double length = std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
            if (length <= 0.0)
                continue;
            waveLines->InsertNextCell(segments + 1);
            for (int step = 0; step <= segments; ++step) {
                const double t = static_cast<double>(step) / segments;
                const vtkIdType point =
                    wavePoints->InsertNextPoint(a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2]));
                waveLines->InsertCellPoint(point);
                const double travel = std::min(distance[ids[i - 1]] + t * length, distance[ids[i]] + (1.0 - t) * length);
                waveDistances->InsertNextValue(maxDistance > 0.0 ? travel / maxDistance : 0.0);
                waveCenters->InsertNextTuple3(a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2]));
            }
        }
    }
    double bounds[6];
    grid_->GetBounds(bounds);
    const double diagonal = std::hypot(bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4]);
    auto wave = vtkSmartPointer<vtkPolyData>::New();
    wave->SetPoints(wavePoints);
    wave->SetLines(waveLines);
    wave->GetPointData()->AddArray(waveDistances);
    wave->GetPointData()->AddArray(waveCenters);
    auto tube = vtkSmartPointer<vtkTubeFilter>::New();
    tube->SetInputData(wave);
    tube->SetRadius(diagonal * 0.00165);  // 波纹线条本身的粗细
    tube->SetVaryRadiusToVaryRadiusOff();
    tube->SetNumberOfSides(8);
    tube->CappingOn();
    auto waveMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    tube->Update();
    waveMapper->SetInputData(tube->GetOutput());
    waveMapper->ScalarVisibilityOff();
    waveMapper->SetVBOShiftScaleMethod(vtkPolyDataMapper::DISABLE_SHIFT_SCALE);
    waveMapper->MapDataArrayToVertexAttribute("waveCenterMC", "EdgeWaveCenter", vtkDataObject::FIELD_ASSOCIATION_POINTS, -1);
    waveMapper->MapDataArrayToVertexAttribute("waveDistanceMC", "EdgeWaveDistance", vtkDataObject::FIELD_ASSOCIATION_POINTS, -1);
    edgeWaveActor_ = vtkSmartPointer<vtkActor>::New();
    edgeWaveActor_->SetMapper(waveMapper);
    edgeWaveActor_->GetProperty()->SetColor(0.0, 0.0, 0.0);
    edgeWaveActor_->GetProperty()->LightingOff();
    auto *shader = edgeWaveActor_->GetShaderProperty();
    shader->GetVertexCustomUniforms()->SetUniformf("wavePhase", 0.0f);
    shader->GetVertexCustomUniforms()->SetUniformf("waveStrength", 0.0f);
    shader->AddVertexShaderReplacement("//VTK::PositionVC::Dec", true,
                                       "//VTK::PositionVC::Dec\n"
                                       "in vec3 waveCenterMC;\n"
                                       "in float waveDistanceMC;\n",
                                       false);
    shader->AddVertexShaderReplacement("//VTK::PositionVC::Impl", true,
                                       "float crest = 0.5 + 0.5 * cos(6.28318530718 * (2.0 * waveDistanceMC - wavePhase));\n"
                                       "float sharpness = 0.5; // 越小波峰越宽\n"
                                       "float radiusScale = waveStrength * (0.003 + 0.997 * pow(crest, sharpness));\n"
                                       "vec4 waveVertexMC = vec4(waveCenterMC + (vertexMC.xyz - waveCenterMC) * radiusScale, "
                                       "1.0);\n"
                                       "#define vertexMC waveVertexMC\n"
                                       "//VTK::PositionVC::Impl\n"
                                       "#undef vertexMC\n",
                                       false);
    edgeWaveActor_->PickableOff();  // 禁止此装饰性波纹参与拾取
    edgeWaveActor_->SetVisibility(showImmediately);
    renderer_->AddActor(edgeWaveActor_);
}

void vtkScene::setEdgeWavePhase(double phase, double strength) {
    if (!edgeWaveActor_)
        return;
    edgeWaveActor_->GetShaderProperty()->GetVertexCustomUniforms()->SetUniformf("wavePhase", static_cast<float>(phase));
    edgeWaveActor_->GetShaderProperty()->GetVertexCustomUniforms()->SetUniformf("waveStrength",
                                                                                static_cast<float>(std::clamp(strength, 0.0, 1.0)));
}
