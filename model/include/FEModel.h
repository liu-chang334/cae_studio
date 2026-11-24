#pragma once

#include "BoundaryData.h"
#include "LoadData.h"
#include "MaterialData.h"
#include "MeshData.h"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class AnalysisType { Static, ExplicitDynamics };

struct ExplicitDynamicsStep {
    double duration = 0.0;
    std::optional<double> timeIncrement;
};

/**
 * @brief 有限元模型数据
 * @details 包含网格数据, 集合信息, 材料数据, 载荷数据, 边界条件数据, 材料单元分配表
 */
struct FEModel {
    MeshData mesh;

    std::map<std::string, std::vector<MeshId>> nodeSets;
    std::map<std::string, std::vector<MeshId>> elemSets;

    std::vector<MaterialData> materials;
    std::vector<NodalLoad> loads;
    std::vector<NodalBoundary> boundaries;

    std::unordered_map<MeshId, std::string> materialNameByElementId; // inp原始单元号->材料名称
    AnalysisType analysisType = AnalysisType::Static;
    std::optional<ExplicitDynamicsStep> explicitDynamicsStep;
    std::unordered_map<std::string, SmoothStepAmplitude> smoothStepAmplitudes;
};
