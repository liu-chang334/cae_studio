#pragma once

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
};

class ExplicitDynamicsSolver {
public:
    ExplicitDynamicsResult solve(const ExplicitDynamicsProblem &problem, const ExplicitDynamicsStep &step) const;
};
