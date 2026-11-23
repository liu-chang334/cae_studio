#pragma once

#include <optional>
#include <string>
#include <vector>

/**
 * @brief 各向同性线弹性参数
 * @details 包含杨氏模量, 泊松比
 * */
struct IsotropicElasticData {
    double youngModulus = 0.0;
    double poissonRatio = 0.0;
};

/**
 * @brief 塑性硬化曲线上的一个数据点
 */
struct PlasticHardeningPoint {
    double yieldStress = 0.0;
    double equivalentPlasticStrain = 0.0;
};

/**
 * @brief Mises塑性各向同性硬化曲线
 */
struct MisesPlasticityData {
    std::vector<PlasticHardeningPoint> hardeningCurve;
};

struct MaterialData {
    std::string name;
    std::optional<IsotropicElasticData> elastic;
    std::optional<MisesPlasticityData> plasticity;
};