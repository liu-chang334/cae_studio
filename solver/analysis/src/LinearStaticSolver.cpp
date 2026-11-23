#include "LinearStaticSolver.h"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct ReducedLinearSystem {
    linalg::MatrixX reducedCoefficientMatrix;
    linalg::VectorX reducedRhs;
    std::vector<std::size_t> freeIndices;
    std::vector<std::size_t> prescribedIndices;
};

ReducedLinearSystem reduceSystem(const ConstrainedLinearSystem &system) {
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    const auto &prescribed = system.prescribedValues;
    const auto size = b.size();
    if (A.rows() != size || A.cols() != size || prescribed.size() != size)
        throw std::invalid_argument("Constrained linear system dimensions do not match");
    std::vector<std::size_t> freeIndices;
    std::vector<std::size_t> prescribedIndices;
    for (std::size_t index = 0; index < size; ++index) {
        if (prescribed[index].has_value())
            prescribedIndices.push_back(index);
        else
            freeIndices.push_back(index);
    }
    const auto freeCount = freeIndices.size();
    linalg::MatrixX reducedCoefficientMatrix(freeCount, freeCount);
    linalg::VectorX reducedRhs(freeCount);
    for (std::size_t i = 0; i < freeCount; ++i) {
        const auto row = freeIndices[i];
        reducedRhs[i] = b[row];
        for (const auto col : prescribedIndices)
            reducedRhs[i] -= A(row, col) * prescribed[col].value();
        for (std::size_t j = 0; j < freeCount; ++j)
            reducedCoefficientMatrix(i, j) = A(row, freeIndices[j]);
    }
    return {std::move(reducedCoefficientMatrix), std::move(reducedRhs), std::move(freeIndices), std::move(prescribedIndices)};
}

}  // namespace

const LinearStaticResult &LinearStaticSolver::solve(const ConstrainedLinearSystem &system, LinearStaticAlgorithm algorithm) {
    result_.reset();
    const auto reduced = reduceSystem(system);
    linalg::VectorX freeValues;
    switch (algorithm) {
    case LinearStaticAlgorithm::Cholesky:
        freeValues = linalg::solveCholesky(reduced.reducedCoefficientMatrix, reduced.reducedRhs);
        break;
    default:
        throw std::invalid_argument("Unsupported linear static algorithm");
    }
    const auto size = system.originalRhs.size();
    LinearStaticResult result{linalg::VectorX(size, 0.0), linalg::VectorX(size, 0.0)};
    for (const auto index : reduced.prescribedIndices)
        result.displacements[index] = system.prescribedValues[index].value();
    for (std::size_t i = 0; i < reduced.freeIndices.size(); ++i)
        result.displacements[reduced.freeIndices[i]] = freeValues[i];
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    for (const auto row : reduced.prescribedIndices) {
        double reaction = -b[row];
        for (std::size_t col = 0; col < size; ++col)
            reaction += A(row, col) * result.displacements[col];
        result.reactions[row] = reaction;
    }
    result_ = std::move(result);
    return *result_;
}