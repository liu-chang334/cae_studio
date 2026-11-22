<!-- omit from toc -->
# VTK_Notes
- [1. 渲染三维网格](#1-渲染三维网格)
- [2. 基于三维网格渲染标量场](#2-基于三维网格渲染标量场)

## 1. 渲染三维网格

```cpp
auto points = vtkSmartPointer<vtkPoints>::New();
points->SetDataTypeToDouble();
points->InsertNextPoint(x1, y1, z1);
points->InsertNextPoint(x2, y2, z2);
// 继续添加点...
auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
grid->SetPoints(points);
grid->InsertNextCell(vtkCellType, 8, cell_1_Ids);
grid->InsertNextCell(vtkCellType, 8, cell_2_Ids);
// 继续添加单元...

auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
mapper->SetInputData(grid);
auto actor = vtkSmartPointer<vtkActor>::New();
actor->SetMapper(mapper);
auto renderer = vtkSmartPointer<vtkRenderer>::New();
renderer->AddActor(actor);
auto window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
window->AddRenderer(renderer);
window->Render();
```

## 2. 基于三维网格渲染标量场

```cpp
auto field = vtkSmartPointer<vtkDoubleArray>::New();
field->SetNumberOfComponents(1);
field->SetName("Displacement");
field->InsertNextValue(u);
grid->GetPointData()->AddArray(field);

auto *array = grid->GetPointData()->GetArray("Displacement");
double range[2];
array->GetRange(range);
const double minValue = range[0];
const double maxValue = range[1];
const double midValue = (minValue + maxValue) / 2.0;

auto colors = vtkSmartPointer<vtkDiscretizableColorTransferFunction>::New();
colors->SetColorSpaceToLab();
colors->AddColor(0.00, 0.25, 0.44, 0.70);  // 柔和蓝
colors->AddColor(0.20, 0.47, 0.66, 0.82);  // 中蓝
colors->AddColor(0.40, 0.79, 0.87, 0.93);  // 浅蓝
colors->AddRGBPoint(midValue, 0.96, 0.96, 0.95);
colors->AddColor(0.60, 0.96, 0.84, 0.79);  // 浅红
colors->AddColor(0.80, 0.87, 0.59, 0.51);  // 中红
colors->AddColor(1.00, 0.72, 0.32, 0.31);  // 柔和红
constexpr int colorLevels = 13;
colors->SetNumberOfValues(colorLevels);
colors->Build();

mapper->SetLookupTable(colors);
mapper->SelectColorArray("Displacement");
```