#pragma once

#include "IntegrationPointState.h"

using NodalStressStrainValues = std::array<double, 12>;

struct ElasticResultFrame {
    double time = 0.0;
    std::vector<NodalStressStrainValues> nodes;
    std::vector<std::vector<IntegrationPointState>> integrationPoints;  // 按网格单元顺序排列
};
