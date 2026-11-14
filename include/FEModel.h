#pragma once

#include "BoundaryData.h"
#include "LoadData.h"
#include "MaterialData.h"
#include "MeshData.h"

#include <string>
#include <unordered_map>
#include <vector>

struct FEModel {
    MeshData mesh;
    std::vector<MaterialData> materials;
    std::vector<NodeSetLoad> loads;
    std::vector<NodeSetBoundary> boundaries;

    std::unordered_map<MeshId, std::string> materialNameByElementId; // inp原始单元号->材料名称
};