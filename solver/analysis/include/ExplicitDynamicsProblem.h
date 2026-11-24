#pragma once

#include "LoadData.h"
#include "Matrix.h"

#include <cstddef>
#include <optional>
#include <vector>

struct ExplicitElement {
    std::vector<std::size_t> globalDofs;
    linalg::MatrixX elasticStiffness;
};

struct ExplicitNodalLoad {
    SmoothStepAmplitude amplitude;
    linalg::VectorX magnitudes;
};

struct ExplicitDynamicsProblem {
    linalg::VectorX lumpedMass;
    linalg::VectorX nodalForces;
    std::vector<std::optional<double>> prescribedDisplacements;
    std::vector<ExplicitElement> elements;
    std::vector<ExplicitNodalLoad> timeDependentLoads;
};
