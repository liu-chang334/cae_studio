#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using MeshId = std::int64_t;

/**
 * @brief 网格节点
 * @details 网格节点包含节点ID和3个坐标值
 */
struct MeshNode {
    MeshId id;
    std::array<double, 3> coordinates;
};

enum class ElementType { Hex8 };

/**
 * @brief 网格单元
 * @details 网格单元包含单元ID、单元类型和拓扑节点ID
 * @note 目前单元类型仅支持Hex8
 */
struct MeshElement {
    MeshId id;
    ElementType type;
    std::vector<MeshId> nodeIds;
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