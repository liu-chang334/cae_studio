#pragma once

#include <array>
#include <vector>

/**
 * @brief 单积分点或单节点应力应变状态
 * @details 全局坐标系voigt约定为11, 22, 33, 12, 13, 23, 剪应变使用工程剪应变约定
 */
struct StressStrainState {
    /// @brief 应力
    std::array<double, 6> stress{};

    /// @brief 应变
    std::array<double, 6> strain{};
};

/// @brief 单积分点应力应变状态
using IntegrationPointState = StressStrainState;

/// @brief 单积分点材料状态
struct MaterialPointState {
    IntegrationPointState fields;
    std::vector<double> internalVariables;  // 与材料本构模型相关的内变量
};

/// @brief 单个单元内所有积分点的材料状态
struct ElementState {
    explicit ElementState(std::size_t count) : committed(count), trial(count) {}

    /// @brief 提交试算状态
    void commit() noexcept { committed.swap(trial); }
    /// @brief 回滚试算状态
    void revert() { trial = committed; }

    /// @brief 已提交状态数组
    std::vector<MaterialPointState> committed;
    /// @brief 试算状态数组
    std::vector<MaterialPointState> trial;
};
