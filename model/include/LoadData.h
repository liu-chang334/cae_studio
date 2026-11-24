#pragma once

#include "NodeTarget.h"

#include <string>
#include <vector>

struct AmplitudePoint {
    double time;
    double value;
};

using SmoothStepAmplitude = std::vector<AmplitudePoint>;

/**
 * @brief 作用于节点或节点集的集中载荷
 */
struct NodalLoad {
    NodeTarget target;
    int degreeOfFreedom = 0;
    double magnitude = 0.0;
    std::string amplitudeName;
};
