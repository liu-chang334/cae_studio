#pragma once

#include "DisplacementFrame.h"
#include "ExplicitDynamicsProblem.h"

struct ExplicitDynamicsStep;

struct ExplicitDynamicsResult {
    linalg::VectorX displacements;
    linalg::VectorX velocities;
    linalg::VectorX accelerations;
    linalg::VectorX reactions;
    double time = 0.0;
    double kineticEnergy = 0.0;
    double strainEnergy = 0.0;
    std::vector<DisplacementFrame> frames;
};

class ExplicitDynamicsSolver {
public:
    // 需要帧输出时，保存初始状态和各输出时刻之后的第一个完整积分步。
    ExplicitDynamicsResult solve(const ExplicitDynamicsProblem &problem, const ExplicitDynamicsStep &step,
                                 std::size_t outputIntervals = 0) const;
};
