#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using MeshId = std::int64_t; // 源网格或源节点id

/**
 * @brief 网格节点
 * @details 网格节点包含节点ID和3个坐标值
 */
struct MeshNode {
    MeshId id;
    std::array<double, 3> coordinates;
};

enum class CellTopology { Unknown, HEXAHEDRON };
enum class ElementFormulation { Unspecified, C3D8, C3D8R };

/**
 * @brief 网格单元
 * @details 网格单元包含单元ID、单元类型和拓扑节点ID
 * @note 目前单元类型仅支持Hex8
 */
struct MeshElement {
    MeshId id;
    std::vector<MeshId> nodeIds;
    CellTopology topology = CellTopology::Unknown;                    // 单元拓扑类型
    ElementFormulation formulation = ElementFormulation::Unspecified; // 单元分析类型
};

/**
 * @brief 网格数据
 * @details 网格数据包含所有节点、单元和节点集
 */
struct MeshData {
    std::vector<MeshNode> nodes;
    std::vector<MeshElement> elements;
    std::map<std::string, std::vector<MeshId>> nodeSets;
};