#pragma once

#include "SolidElement3D.h"

#include <optional>

// 单元定义可以在求解之间共享, 积分点状态由每次求解单独持有
struct AnalysisElement {
    std::shared_ptr<const SolidElement3D> element;  // 该单元对应的算法类型
    std::vector<std::size_t> globalDofs;  // 该单元对应的全局自由度索引
    std::optional<MeshId> sourceId;  // 该单元的源网格ID
};
