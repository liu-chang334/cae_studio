#include "vtkBuilder.h"

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
    for (const auto &node : mesh.nodes) {
        if (id2pid.find(node.id) != id2pid.end()) {
            result.error =
                QStringLiteral("存在重复节点编号: %1").arg(static_cast<qlonglong>(node.id));
            return result;
        }
        const auto &xyz = node.coordinates;
        vtkIdType pid = points->InsertNextPoint(xyz[0], xyz[1], xyz[2]);
        id2pid.emplace(node.id, pid);
    }
    grid->SetPoints(points);
    for (const auto &element : mesh.elements) {
        if (element.type != ElementType::Hex8 || element.nodeIds.size() != 8) {
            result.error = QStringLiteral("单元 %1 的类型或节点数量无效")
                               .arg(static_cast<qlonglong>(element.id));
            return result;
        }
        vtkIdType conn[8];
        for (int i = 0; i < 8; ++i) {
            auto it = id2pid.find(element.nodeIds[i]);
            if (it == id2pid.end()) {
                result.error = QStringLiteral("单元 %1 引用了不存在的节点 %2")
                                   .arg(static_cast<qlonglong>(element.id))
                                   .arg(static_cast<qlonglong>(element.nodeIds[i]));
                return result;
            }
            conn[i] = it->second;
        }
        grid->InsertNextCell(VTK_HEXAHEDRON, 8, conn);
    }
    result.grid = grid;
    return result;
}