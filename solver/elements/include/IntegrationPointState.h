#pragma once

#include <array>

/**
 * @brief 积分点状态
 * @details 积分点状态包含应力和应变
 */
struct IntegrationPointState {
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