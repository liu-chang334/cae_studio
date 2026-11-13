#include "vtkBuilder.h"

#include <vtkCellData.h>
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
        elementIds->InsertNextValue(static_cast<vtkTypeInt64>(element.id)); // 保留原始单元号
    }
    grid->GetCellData()->AddArray(elementIds);
    result.grid = grid;
    return result;
}