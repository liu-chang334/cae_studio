#pragma once

#include <string>

/**
 * @brief 作用于节点集的指定位移边界条件
 * @details 包含节点集名称, 方向, 指定位移值
 */
struct NodeSetBoundary {
    std::string nodeSetName;
    int degreeOfFreedom = 0;
    double displacement = 0.0;
};
