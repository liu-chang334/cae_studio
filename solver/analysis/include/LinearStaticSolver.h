#pragma once

#include "RawLinearSystem.h"

#include <optional>

/**
 * @brief 线性静力求解算法枚举
 */
enum class LinearStaticAlgorithm {
    Cholesky  // Cholesky分解
};

/**
 * @brief 线性静力求解约束条件处理方法枚举
 */
enum class LinearStaticConstraintMethod {
    Reduction,  // 约化法
    Penalty  // 惩罚法
};

/**
 * @brief 线性静力求解结果
 */
struct LinearStaticResult {
    linalg::VectorX displacements;  // 位移
    linalg::VectorX reactions;  // 支反力
};

/**
 * @brief 线性静力求解器
 */
class LinearStaticSolver {
public:
    /**
     * @brief 求解线性静力问题
     * @param system 施加位移边界条件前的完整线性系统及各自由度的指定位移
     * @param algorithm 线性静力求解算法
     * @param constraintMethod 约束条件处理方法
     * @return 求解结果
     */
    const LinearStaticResult &solve(const RawLinearSystem &system,
                                    LinearStaticAlgorithm algorithm = LinearStaticAlgorithm::Cholesky,
                                    LinearStaticConstraintMethod constraintMethod = LinearStaticConstraintMethod::Reduction);

    /**
     * @brief 返回求解结果的引用
     */
    const std::optional<LinearStaticResult> &result() const noexcept { return result_; }

private:
    /**
     * @brief 线性静力求解结果
     */
    std::optional<LinearStaticResult> result_;
};
