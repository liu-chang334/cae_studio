#pragma once

#include "NodeTarget.h"

/**
 * @brief 作用于节点或节点集的指定位移边界条件
 */
struct NodalBoundary {
    NodeTarget target;
    int degreeOfFreedom = 0;
    double displacement = 0.0;
};
