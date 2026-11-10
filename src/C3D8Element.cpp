#include "C3D8Element.h"

C3D8Element::Matrix<8, 3> C3D8Element::nodeCoordinates() const {
    Matrix<8, 3> coordinates;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 3; ++j)
            coordinates[i][j] = nodes_[i].coordinates[j];
    }
    return coordinates;
}

C3D8Element::Matrix<3, 8> C3D8Element::computeShapeGradientsReference(const std::array<double, 3> &naturalCoordinates) {
    const double r = naturalCoordinates[0];
    const double s = naturalCoordinates[1];
    const double t = naturalCoordinates[2];
    // 八个局部节点在参考单元中的自然坐标, 用于保留符号
    constexpr int signs[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    Matrix<3, 8> gradients{};
    for (int i = 0; i < 8; ++i) {
        const double ri = signs[i][0];
        const double si = signs[i][1];
        const double ti = signs[i][2];
        gradients[0][i] = 0.125 * ri * (1.0 + si * s) * (1.0 + ti * t); // 对r偏导
        gradients[1][i] = 0.125 * si * (1.0 + ti * t) * (1.0 + ri * r); // 对s偏导
        gradients[2][i] = 0.125 * ti * (1.0 + ri * r) * (1.0 + si * s); // 对t偏导
    }
    return gradients;
}

C3D8Element::Matrix<3, 3> C3D8Element::computeJacobian(const Matrix<3, 8> &referenceGradients, const Matrix<8, 3> &coordinates) {
    Matrix<3, 3> jacobian{};
    for (int i = 0; i < 3; ++i) {     // 第i行是三个物理坐标依次对第i个自然坐标的偏导
        for (int j = 0; j < 3; ++j) { // 第j列是第j个物理坐标依次对三个自然坐标的偏导
            for (int a = 0; a < 8; ++a)
                jacobian[i][j] += referenceGradients[i][a] * coordinates[a][j];
        }
    }
    // [ dx/dr  dy/dr  dz/dr ]
    // [ dx/ds  dy/ds  dz/ds ]
    // [ dx/dt  dy/dt  dz/dt ]
    return jacobian;
}