#pragma once

#include <array>
#include <vector>

/**
 * @brief 积分点或节点上的应力应变状态
 * @details 全局坐标系 Voigt 顺序为 11, 22, 33, 12, 13, 23；剪应变使用工程剪应变。
 */
struct StressStrainState {
    /**
     * @brief 应力
     * @details 初始化为0
     */
    std::array<double, 6> stress{};

    /**
     * @brief 应变
     * @details 初始化为0
     */
    std::array<double, 6> strain{};
};

using IntegrationPointState = StressStrainState;

struct MaterialPointState {
    IntegrationPointState fields;
    std::vector<double> internalVariables;  // 由材料模型定义
};

struct ElementState {
    explicit ElementState(std::size_t count) : committed(count), trial(count) {}

    // 下一次本构试算以 committed 为起点，并重新填充 trial。
    void commit() noexcept { committed.swap(trial); }
    void revert() { trial = committed; }

    std::vector<MaterialPointState> committed;
    std::vector<MaterialPointState> trial;
};
