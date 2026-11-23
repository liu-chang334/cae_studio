#include "LinearStaticSolver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct ConstraintIndices {
    std::vector<std::size_t> freeIndices;
    std::vector<std::size_t> prescribedIndices;
};

ConstraintIndices collectConstraintIndices(const ConstrainedLinearSystem &system) {
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    const auto &prescribed = system.prescribedValues;
    const auto size = b.size();
    if (A.rows() != size || A.cols() != size || prescribed.size() != size)
        throw std::invalid_argument("Constrained linear system dimensions do not match");
    ConstraintIndices indices;
    for (std::size_t index = 0; index < size; ++index) {
        if (prescribed[index].has_value())
            indices.prescribedIndices.push_back(index);
        else
            indices.freeIndices.push_back(index);
    }
    return indices;
}

linalg::VectorX solveSystem(const linalg::MatrixX &coefficientMatrix, const linalg::VectorX &rhs, LinearStaticAlgorithm algorithm) {
    switch (algorithm) {
    case LinearStaticAlgorithm::Cholesky:
        return linalg::solveCholesky(coefficientMatrix, rhs);
    }
    throw std::invalid_argument("Unsupported linear static algorithm");
}

linalg::VectorX solveByReduction(const ConstrainedLinearSystem &system, const ConstraintIndices &indices,
                                 LinearStaticAlgorithm algorithm) {
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    const auto &prescribed = system.prescribedValues;
    const auto freeCount = indices.freeIndices.size();
    linalg::MatrixX reducedCoefficientMatrix(freeCount, freeCount);
    linalg::VectorX reducedRhs(freeCount);
    for (std::size_t i = 0; i < freeCount; ++i) {
        const auto row = indices.freeIndices[i];
        reducedRhs[i] = b[row];
        for (const auto col : indices.prescribedIndices)
            reducedRhs[i] -= A(row, col) * prescribed[col].value();
        for (std::size_t j = 0; j < freeCount; ++j)
            reducedCoefficientMatrix(i, j) = A(row, indices.freeIndices[j]);
    }

    const auto freeValues = solveSystem(reducedCoefficientMatrix, reducedRhs, algorithm);
    linalg::VectorX displacements(b.size());
    for (const auto index : indices.prescribedIndices)
        displacements[index] = prescribed[index].value();
    for (std::size_t i = 0; i < freeCount; ++i)
        displacements[indices.freeIndices[i]] = freeValues[i];
    return displacements;
}

linalg::VectorX solveByPenalty(const ConstrainedLinearSystem &system, const ConstraintIndices &indices,
                               LinearStaticAlgorithm algorithm) {
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    if (indices.prescribedIndices.empty())
        return solveSystem(A, b, algorithm);

    double diagonalScale = 0.0;
    for (std::size_t index = 0; index < A.rows(); ++index)
        diagonalScale = std::max(diagonalScale, std::abs(A(index, index)));

    // 按原始系统的对角线量级选取罚参数,零矩阵使用单位量级
    constexpr double penaltyMultiplier = 1e8;
    const double penalty = penaltyMultiplier * (diagonalScale == 0.0 ? 1.0 : diagonalScale);
    if (!std::isfinite(penalty))
        throw std::overflow_error("Penalty coefficient is too large");

    auto penalizedCoefficientMatrix = A;
    auto penalizedRhs = b;
    for (const auto index : indices.prescribedIndices) {
        penalizedCoefficientMatrix(index, index) += penalty;
        penalizedRhs[index] += penalty * system.prescribedValues[index].value();
        if (!std::isfinite(penalizedCoefficientMatrix(index, index)) || !std::isfinite(penalizedRhs[index]))
            throw std::overflow_error("Penalized linear system contains a non-finite value");
    }
    return solveSystem(penalizedCoefficientMatrix, penalizedRhs, algorithm);
}

}  // namespace

const LinearStaticResult &LinearStaticSolver::solve(const ConstrainedLinearSystem &system, LinearStaticAlgorithm algorithm,
                                                    LinearStaticConstraintMethod constraintMethod) {
    result_.reset();
    const auto indices = collectConstraintIndices(system);
    linalg::VectorX displacements;
    switch (constraintMethod) {
    case LinearStaticConstraintMethod::Reduction:
        displacements = solveByReduction(system, indices, algorithm);
        break;
    case LinearStaticConstraintMethod::Penalty:
        displacements = solveByPenalty(system, indices, algorithm);
        break;
    default:
        throw std::invalid_argument("Unsupported linear static constraint method");
    }
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    LinearStaticResult result{std::move(displacements), linalg::VectorX(b.size(), 0.0)};
    for (const auto row : indices.prescribedIndices) {
        double reaction = -b[row];
        for (std::size_t col = 0; col < b.size(); ++col)
            reaction += A(row, col) * result.displacements[col];
        result.reactions[row] = reaction;
    }
    result_ = std::move(result);
    return *result_;
}
