#include <cmath>
#include <stdexcept>

#include "LinearElastic.h"
#include "MaterialData.h"

linalg::Matrix<6, 6> computeIsotropicElasticMatrix(const IsotropicElasticData &material) {
    const double E = material.youngModulus;
    const double nu = material.poissonRatio;
    if (!std::isfinite(E) || E <= 0.0)
        throw std::invalid_argument("Young's modulus must be finite and positive");
    if (!std::isfinite(nu) || nu <= -1.0 || nu >= 0.5)
        throw std::invalid_argument("Poisson's ratio must be in (-1, 0.5)");
    const double G = E / (2.0 * (1.0 + nu));
    const double lambda = (2.0 * nu * G) / (1.0 - 2.0 * nu);
    linalg::Matrix<6, 6> D{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            D[row][col] = lambda;
        }
        D[row][row] += 2.0 * G;
        D[row + 3][row + 3] = G;
    }
    return D;
}

void LinearElasticMaterial::integrate(const MaterialPointState &, const linalg::Vector<6> &totalStrain, double,
                                      MaterialPointState &trial) const {
    StressStrainState fields{};
    fields.strain = totalStrain;
    for (std::size_t row = 0; row < fields.stress.size(); ++row) {
        for (std::size_t col = 0; col < totalStrain.size(); ++col)
            fields.stress[row] += elasticMatrix_[row][col] * totalStrain[col];
        if (!std::isfinite(fields.strain[row]) || !std::isfinite(fields.stress[row]))
            throw std::domain_error("Linear elastic integration produced a non-finite state");
    }
    trial.fields = fields;
    trial.internalVariables.clear();
}
