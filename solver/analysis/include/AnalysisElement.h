#pragma once

#include "SolidElement3D.h"

#include <optional>

/// @brief 封装单元算法,全局自由度索引,源id, 供求解器使用
struct AnalysisElement {
    std::shared_ptr<const SolidElement3D> element;  // 该单元对应的算法类型
    std::vector<std::size_t> globalDofs;  // 该单元对应的全局自由度索引
    std::optional<MeshId> sourceId;  // 该单元的源网格ID
};
