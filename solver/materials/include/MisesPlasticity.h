#pragma once
#include "LinearElastic.h"

// 小应变 J2 塑性，分段线性各向同性硬化；末段之后保持屈服应力不变。
// 内变量为 6 个工程塑性应变分量及累计等效塑性应变。
class MisesPlasticityMaterial : public Material3D {
public:
    MisesPlasticityMaterial(const IsotropicElasticData &elastic, const MisesPlasticityData &plastic,
                            std::optional<double> density = std::nullopt);
    const linalg::Matrix<6, 6> &initialElasticMatrix() const override { return elastic_; }

    /// @brief 更新单积分点的试算状态
    /// @param committed 已提交状态(作为起点)
    /// @param strain 应变(全量)
    /// @param dt 时间增量(暂不起作用)
    /// @param trial 试算状态
    void integrate(const MaterialPointState &committed, const linalg::Vector<6> &strain, double dt,
                   MaterialPointState &trial) const override;

private:
    std::pair<double, double> hardening(double equivalentStrain) const;
    linalg::Matrix<6, 6> elastic_;
    MisesPlasticityData plastic_;
    double shear_;
};
