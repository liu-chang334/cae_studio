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