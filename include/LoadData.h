#pragma once

#include <string>

/**
 * @brief 节点集载荷结构体
 * @details 用于存储节点集载荷的信息，包括节点集名称、自由度和载荷大小
 */
struct NodeSetLoad {
    std::string nodeSetName;
    int degreeOfFreedom = 0;
    double magnitude = 0.0;
};