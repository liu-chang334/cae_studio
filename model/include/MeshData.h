#pragma once
#include <array>
#include <cstdint>
#include <vector>

using MeshId = std::int64_t;  // 源网格或源节点id

enum class CellTopology {
    Unknown,
    HEXAHEDRON,  // 六面体
    QUADRATIC_TETRAHEDRON  // 二次四面体
};
enum class ElementFormulation {
    Unspecified,
    C3D8,  // C3D8全积分单元
    C3D8R,  // C3D8R减缩积分单元
    C3D10  // C3D10全积分单元
};

/**
 * @brief 网格节点
 * @details 网格节点包含节点ID和3个坐标值
 */
struct MeshNode {
    MeshId id;
    std::array<double, 3> coordinates;
};

/**
 * @brief 网格单元
 * @details 网格单元包含单元ID、单元类型和拓扑节点ID
 * @note 目前单元类型仅支持Hex8
 */
struct MeshElement {
    MeshId id;
    std::vector<MeshId> nodeIds;
    CellTopology topology = CellTopology::Unknown;  // 单元拓扑类型
    ElementFormulation formulation = ElementFormulation::Unspecified;  // 单元分析类型
};

/**
 * @brief 网格数据
 * @details 网格数据只包含节点和单元
 */
struct MeshData {
    std::vector<MeshNode> nodes;
    std::vector<MeshElement> elements;
};