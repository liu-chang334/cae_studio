#include "MisesPlasticity.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

MisesPlasticityMaterial::MisesPlasticityMaterial(const IsotropicElasticData &elastic, const MisesPlasticityData &plastic,
                                                 std::optional<double> density)
    : Material3D(density), elastic_(computeIsotropicElasticMatrix(elastic)), plastic_(plastic),
      shear_(elastic.youngModulus / (2.0 * (1.0 + elastic.poissonRatio))) {
    if (plastic_.hardeningCurve.empty() || plastic_.hardeningCurve.front().equivalentPlasticStrain != 0.0)
        throw std::invalid_argument("Plastic hardening curve must start at zero plastic strain");
    double previousStrain = -1.0, previousStress = 0.0;
    for (const auto &point : plastic_.hardeningCurve) {
        if (!std::isfinite(point.yieldStress) || !std::isfinite(point.equivalentPlasticStrain) || point.yieldStress <= 0.0 ||
            point.yieldStress < previousStress || point.equivalentPlasticStrain <= previousStrain)
            throw std::invalid_argument("Plastic curve requires positive nondecreasing stress and increasing plastic strain");
        previousStrain = point.equivalentPlasticStrain;
        previousStress = point.yieldStress;
    }
}

std::pair<double, double> MisesPlasticityMaterial::hardening(double p) const {
    const auto &curve = plastic_.hardeningCurve;
    for (std::size_t i = 1; i < curve.size(); ++i) {
        if (p < curve[i].equivalentPlasticStrain) {
            const auto &a = curve[i - 1];
            const auto &b = curve[i];
            const double H = (b.yieldStress - a.yieldStress) / (b.equivalentPlasticStrain - a.equivalentPlasticStrain);
            return {a.yieldStress + H * (p - a.equivalentPlasticStrain), H};
        }
    }
    return {curve.back().yieldStress, 0.0};
}

void MisesPlasticityMaterial::integrate(const MaterialPointState &committed, const linalg::Vector<6> &strain, double dt,
                                        MaterialPointState &trial) const {
    if (!std::isfinite(dt) || dt < 0.0 || !std::all_of(strain.begin(), strain.end(), [](double v) { return std::isfinite(v); }))
        throw std::invalid_argument("Invalid plastic integration strain or time increment");
    if (!committed.internalVariables.empty() && committed.internalVariables.size() != 7)
        throw std::invalid_argument("J2 material requires seven internal variables");
    auto history = committed.internalVariables;  // 从已提交历史状态出发
    if (history.empty())
        history.assign(7, 0.0);
    for (double v : history)
        if (!std::isfinite(v))
            throw std::invalid_argument("Non-finite plastic history");
    if (history[6] < 0.0)
        throw std::invalid_argument("Negative equivalent plastic strain");
    MaterialPointState next;
    next.fields.strain = strain;
    next.internalVariables = history;
    next.tangent = elastic_;
    auto &stress = next.fields.stress;
    // 01. 计算弹性试算应力
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j)
            stress[i] += elastic_[i][j] * (strain[j] - history[j]);  // 需要先计算应变增量, 再计算试算应力
    const double mean = (stress[0] + stress[1] + stress[2]) / 3.0;  // 试应力的体应力部分
    auto deviator = stress;
    for (std::size_t i = 0; i < 3; ++i)
        deviator[i] -= mean;  // 试应力的偏量部分
    double norm = 0.0;
    for (std::size_t i = 0; i < 6; ++i)
        norm += (i < 3 ? 1.0 : 2.0) * deviator[i] * deviator[i];
    const double q = std::sqrt(1.5 * norm);  // 等效试应力
    if (!std::isfinite(q))
        throw std::overflow_error("Non-finite J2 trial stress");
    const double yield = hardening(history[6]).first;  // 当前屈服应力
    if (q > yield) {  // 塑性发展
        double lo = 0.0, hi = (q - yield) / (3.0 * shear_);
        for (int iteration = 0; iteration < 80; ++iteration) {
            const double mid = 0.5 * (lo + hi);
            if (q - 3.0 * shear_ * mid > hardening(history[6] + mid).first)
                lo = mid;
            else
                hi = mid;
        }
        const double dp = 0.5 * (lo + hi);
        const double H = hardening(history[6] + dp).second;
        const double a = 1.0 - 3.0 * shear_ * dp / q;
        const double bulk = (elastic_[0][0] + 2.0 * elastic_[0][1]) / 3.0;
        const double coefficient = 9.0 * shear_ * shear_ * (1.0 / (3.0 * shear_ + H) - dp / q);
        for (std::size_t i = 0; i < 6; ++i) {
            stress[i] = a * deviator[i] + (i < 3 ? mean : 0.0);  // 修正应力
            next.internalVariables[i] += (i < 3 ? 1.5 : 3.0) * dp * deviator[i] / q;  // 修正应变
            for (std::size_t j = 0; j < 6; ++j) {
                const double volumetric = i < 3 && j < 3 ? bulk : 0.0;
                const double projector = i == j ? (i < 3 ? 1.0 : 0.5) : 0.0;
                const double deviatoric = projector - (i < 3 && j < 3 ? 1.0 / 3.0 : 0.0);
                next.tangent[i][j] = volumetric + 2.0 * shear_ * a * deviatoric -
                                     coefficient * (deviator[i] / q) * (deviator[j] / q);  // 计算弹塑性矩阵
            }
        }
        next.internalVariables[6] += dp;  // 累积等效塑性应变
    }
    trial = std::move(next);  // 更新试算状态
}
