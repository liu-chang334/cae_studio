#include "LinearStaticSolver.h"
#include "ElementStates.h"

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

ConstraintIndices collectConstraintIndices(const RawLinearSystem &system) {
    const auto &A = system.originalCoefficientMatrix;  // 原始系数矩阵
    const auto &b = system.originalRhs;  // 原始右端项
    const auto &prescribed = system.prescribedValues;  // 约束条件
    const auto size = b.size();
    if (A.rows() != size || A.cols() != size || prescribed.size() != size)
        throw std::invalid_argument("Linear system and prescribed values must have matching dimensions");
    ConstraintIndices indices;
    for (std::size_t index = 0; index < size; ++index) {
        if (prescribed[index].has_value())
            indices.prescribedIndices.push_back(index);  // 约束条件索引
        else
            indices.freeIndices.push_back(index);  // 自由变量索引
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

linalg::VectorX solveByReduction(const RawLinearSystem &system, const ConstraintIndices &indices, LinearStaticAlgorithm algorithm) {
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    const auto &prescribed = system.prescribedValues;
    const auto freeCount = indices.freeIndices.size();
    linalg::MatrixX reducedCoefficientMatrix(freeCount, freeCount);  // 初始化约化系数矩阵
    linalg::VectorX reducedRhs(freeCount);  // 初始化约化右端项
    for (std::size_t i = 0; i < freeCount; ++i) {
        const auto row = indices.freeIndices[i];
        reducedRhs[i] = b[row];
        for (const auto col : indices.prescribedIndices)
            reducedRhs[i] -= A(row, col) * prescribed[col].value();  // 计算约化右端项
        for (std::size_t j = 0; j < freeCount; ++j)
            reducedCoefficientMatrix(i, j) = A(row, indices.freeIndices[j]);  // 计算约化系数矩阵
    }

    const auto freeValues = solveSystem(reducedCoefficientMatrix, reducedRhs, algorithm);
    linalg::VectorX displacements(b.size());
    for (const auto index : indices.prescribedIndices)
        displacements[index] = prescribed[index].value();  // 约束条件本身
    for (std::size_t i = 0; i < freeCount; ++i)
        displacements[indices.freeIndices[i]] = freeValues[i];  // 自由变量解
    return displacements;
}

linalg::VectorX solveByPenalty(const RawLinearSystem &system, const ConstraintIndices &indices, LinearStaticAlgorithm algorithm) {
    const auto &A = system.originalCoefficientMatrix;  // 原始系数矩阵
    const auto &b = system.originalRhs;  // 原始右端项
    if (indices.prescribedIndices.empty())
        return solveSystem(A, b, algorithm);
    double diagonalScale = 0.0;
    for (std::size_t index = 0; index < A.rows(); ++index)
        diagonalScale = std::max(diagonalScale, std::abs(A(index, index)));  // 计算原始系统的对角线量级
    // 按原始系统的对角线量级选取罚参数, 零矩阵使用单位量级
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

const LinearStaticResult &LinearStaticSolver::solve(const RawLinearSystem &system, LinearStaticAlgorithm algorithm,
                                                    LinearStaticConstraintMethod constraintMethod) {
    // 01. 求解线性方程
    result_.reset();
    const auto indices = collectConstraintIndices(system);
    linalg::VectorX displacements;
    switch (constraintMethod) {
    case LinearStaticConstraintMethod::Reduction:  // 约化法
        displacements = solveByReduction(system, indices, algorithm);
        break;
    case LinearStaticConstraintMethod::Penalty:  // 罚函数法
        displacements = solveByPenalty(system, indices, algorithm);
        break;
    default:
        throw std::invalid_argument("Unsupported linear static constraint method");
    }
    // 02. 计算反应力
    const auto &A = system.originalCoefficientMatrix;
    const auto &b = system.originalRhs;
    LinearStaticResult result{std::move(displacements), linalg::VectorX(b.size(), 0.0)};
    for (const auto row : indices.prescribedIndices) {
        double reaction = -b[row];
        for (std::size_t col = 0; col < b.size(); ++col)
            reaction += A(row, col) * result.displacements[col];
        result.reactions[row] = reaction;
    }
    // 03. 更新材料状态
    ElementStates states(
        system.elementDefinitions,
        b.size());  // TODO: 目前ElementStates是局部的, 如果扩展到历史依赖的材料, 是否需要让一个全局对象持有ElementStates
    if (!states.empty()) {
        states.updateTrial(result.displacements, 0.0);  // 更新试算单元状态
        states.commit();  // 提交单元状态
        result.elasticFrame = states.snapshot(0.0);  // 保存弹性框架, 对于弹性全量求解, 无需时间步
    }
    result_ = std::move(result);
    return *result_;
}
