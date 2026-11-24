#include "vtkBuilder.h"
#include "AnalysisProblemBuilder.h"
#include "LinearStaticSolver.h"

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>

vtkBuildResult vtkBuilder::build(const MeshData &mesh) const {
    vtkBuildResult result;
    // 检查网格数据是否有效
    if (mesh.nodes.empty() || mesh.elements.empty()) {
        result.error = QStringLiteral("网格节点或单元为空");
        return result;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    std::unordered_map<MeshId, vtkIdType> id2pid;
    id2pid.reserve(mesh.nodes.size());
    auto nodeIds = vtkSmartPointer<vtkTypeInt64Array>::New();
    nodeIds->SetName("NodeIds");
    nodeIds->SetNumberOfComponents(1);
    for (const auto &node : mesh.nodes) {
        if (id2pid.find(node.id) != id2pid.end()) {
            result.error = QStringLiteral("存在重复节点编号: %1").arg(static_cast<qlonglong>(node.id));
            return result;
        }
        const auto &xyz = node.coordinates;
        vtkIdType pid = points->InsertNextPoint(xyz[0], xyz[1], xyz[2]);
        nodeIds->InsertNextValue(static_cast<vtkTypeInt64>(node.id));
        id2pid.emplace(node.id, pid);
    }
    grid->SetPoints(points);
    grid->GetPointData()->AddArray(nodeIds);
    auto elementIds = vtkSmartPointer<vtkTypeInt64Array>::New();
    elementIds->SetName("ElementIds");
    elementIds->SetNumberOfComponents(1);
    std::vector<vtkIdType> conn;
    for (const auto &element : mesh.elements) {
        int vtkCellType = VTK_EMPTY_CELL;
        std::size_t expectedNodeCount = 0;
        switch (element.topology) {
        case CellTopology::HEXAHEDRON:
            vtkCellType = VTK_HEXAHEDRON;
            expectedNodeCount = 8;
            break;
        default:
            result.error = QStringLiteral("单元 %1 的拓扑类型暂不支持显示（类型值 %2）")
                               .arg(static_cast<qlonglong>(element.id))
                               .arg(static_cast<int>(element.topology));
            return result;
        }
        if (element.nodeIds.size() != expectedNodeCount) {
            result.error = QStringLiteral("单元 %1 需要 %2 个节点，实际有 %3 个")
                               .arg(static_cast<qlonglong>(element.id))
                               .arg(static_cast<qulonglong>(expectedNodeCount))
                               .arg(static_cast<qulonglong>(element.nodeIds.size()));
            return result;
        }
        conn.clear();
        for (const MeshId nodeId : element.nodeIds) {
            const auto it = id2pid.find(nodeId);
            if (it == id2pid.end()) {
                result.error = QStringLiteral("单元 %1 引用了不存在的节点 %2")
                                   .arg(static_cast<qlonglong>(element.id))
                                   .arg(static_cast<qlonglong>(nodeId));
                return result;
            }
            conn.push_back(it->second);
        }
        grid->InsertNextCell(vtkCellType, static_cast<vtkIdType>(conn.size()), conn.data());
        elementIds->InsertNextValue(static_cast<vtkTypeInt64>(element.id));  // 保留原始单元号
    }
    grid->GetCellData()->AddArray(elementIds);
    result.grid = grid;
    return result;
}

vtkBuildResult vtkBuilder::buildResults(const MeshData &mesh, const AnalysisProblemBuilder &analysis,
                                        const LinearStaticResult &solution) const {
    return buildResults(mesh, analysis, solution.displacements, solution.reactions);
}

vtkBuildResult vtkBuilder::buildResults(const MeshData &mesh, const AnalysisProblemBuilder &analysis,
                                        const std::vector<double> &displacementValues, const std::vector<double> &reactionValues) const {
    auto result = build(mesh);
    if (!result.success())
        return result;
    if (displacementValues.size() != analysis.globalDofCount() || reactionValues.size() != analysis.globalDofCount())
        return {nullptr, QStringLiteral("结果向量维度与自由度数量不一致")};
    auto displacements = vtkSmartPointer<vtkDoubleArray>::New();
    displacements->SetName("Displacements");
    displacements->SetNumberOfComponents(3);
    const std::array<const char *, 8> names = {"DisplacementMagnitude", "DisplacementX", "DisplacementY", "DisplacementZ",
                                               "ReactionMagnitude",     "ReactionX",     "ReactionY",     "ReactionZ"};
    std::array<vtkSmartPointer<vtkDoubleArray>, 8> fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        fields[i] = vtkSmartPointer<vtkDoubleArray>::New();
        fields[i]->SetName(names[i]);
        fields[i]->SetNumberOfComponents(1);
    }
    for (const auto &node : mesh.nodes) {
        const auto dof = analysis.globalDofIndex(node.id, 1);
        const double *u = displacementValues.data() + dof;
        const double *rf = reactionValues.data() + dof;
        displacements->InsertNextTypedTuple(u);
        const std::array<double, 8> values = {std::hypot(u[0], u[1], u[2]),    u[0],  u[1],  u[2],
                                              std::hypot(rf[0], rf[1], rf[2]), rf[0], rf[1], rf[2]};
        for (std::size_t i = 0; i < fields.size(); ++i)
            fields[i]->InsertNextValue(values[i]);
    }
    result.grid->GetPointData()->SetVectors(displacements);
    for (const auto &field : fields)
        result.grid->GetPointData()->AddArray(field);
    return result;
}

bool vtkBuilder::setDisplacements(vtkUnstructuredGrid *grid, const std::vector<double> &displacements) {
    if (!grid || displacements.size() != 3 * static_cast<std::size_t>(grid->GetNumberOfPoints()))
        return false;

    auto *pointData = grid->GetPointData();
    const auto count = grid->GetNumberOfPoints();
    const auto getArray = [&](const char *name, int components) {
        auto *array = vtkDoubleArray::SafeDownCast(pointData->GetArray(name));
        if (!array) {
            auto created = vtkSmartPointer<vtkDoubleArray>::New();
            created->SetName(name);
            pointData->AddArray(created);
            array = created;
        }
        array->SetNumberOfComponents(components);
        array->SetNumberOfTuples(count);
        return array;
    };
    auto *vectors = getArray("Displacements", 3);
    auto *magnitude = getArray("DisplacementMagnitude", 1);
    auto *x = getArray("DisplacementX", 1);
    auto *y = getArray("DisplacementY", 1);
    auto *z = getArray("DisplacementZ", 1);
    pointData->SetActiveVectors("Displacements");
    for (vtkIdType point = 0; point < count; ++point) {
        const double *u = displacements.data() + 3 * point;
        vectors->SetTypedTuple(point, u);
        magnitude->SetValue(point, std::hypot(u[0], u[1], u[2]));
        x->SetValue(point, u[0]);
        y->SetValue(point, u[1]);
        z->SetValue(point, u[2]);
    }
    for (auto *array : {vectors, magnitude, x, y, z})
        array->Modified();
    grid->Modified();
    return true;
}
