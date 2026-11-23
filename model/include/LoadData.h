#pragma once

#include "NodeTarget.h"

/**
 * @brief 作用于节点或节点集的集中载荷
 */
struct NodalLoad {
    NodeTarget target;
    int degreeOfFreedom = 0;
    double magnitude = 0.0;
};
