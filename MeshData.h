#pragma once
#include <array>
#include <cstdint>
#include <vector>

using MeshId = std::int64_t;

struct MeshNode {
    MeshId id;
    std::array<double, 3> coordinates;
};

enum class ElementType { Hex8 };

struct MeshElement {
    MeshId id;
    ElementType type;
    std::vector<MeshId> nodeIds;
};

struct MeshData {
    std::vector<MeshNode> nodes;
    std::vector<MeshElement> elements;
};