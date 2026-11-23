#pragma once

#include "ConstrainedLinearSystem.h"

#include <optional>

enum class LinearStaticAlgorithm { Cholesky };
enum class LinearStaticConstraintMethod { Reduction, Penalty };

struct LinearStaticResult {
    linalg::VectorX displacements;
    linalg::VectorX reactions;
};

class LinearStaticSolver {
public:
    const LinearStaticResult &solve(const ConstrainedLinearSystem &system,
                                    LinearStaticAlgorithm algorithm = LinearStaticAlgorithm::Cholesky,
                                    LinearStaticConstraintMethod constraintMethod = LinearStaticConstraintMethod::Reduction);

    const std::optional<LinearStaticResult> &result() const noexcept { return result_; }

private:
    std::optional<LinearStaticResult> result_;
};
