#pragma once

#include "SolidElement3D.h"

#include <optional>

// 单元定义可以在求解之间共享，积分点状态由每次求解单独持有。
struct AnalysisElement {
    std::shared_ptr<const SolidElement3D> element;
    std::vector<std::size_t> globalDofs;
    std::optional<MeshId> sourceId;
};
