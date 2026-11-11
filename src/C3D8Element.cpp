#include "C3D8Element.h"
#include "MatrixMath.h"

#include <cmath>

C3D8Element::C3D8Element(MeshId id, std::vector<MeshNode> nodes, std::size_t materialIndex)
    : SolidElement3D(id, std::move(nodes), materialIndex, 8) {
    initializeGeometry(); // 初始化各积分点的Bbar, 积分因子和单元体积
}

linalg::Matrix<8, 3> C3D8Element::nodeCoordinates() const {
    linalg::Matrix<8, 3> coordinates;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 3; ++j)
            coordinates[i][j] = nodes_[i].coordinates[j];
    }
    return coordinates;
}

linalg::Matrix<3, 8> C3D8Element::computeShapeGradientsReference(const linalg::Vector<3> &naturalCoordinates) {
    const double r = naturalCoordinates[0];
    const double s = naturalCoordinates[1];
    const double t = naturalCoordinates[2];
    // 八个局部节点在参考单元中的自然坐标, 用于保留符号
    constexpr int signs[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    linalg::Matrix<3, 8> gradients{};
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

linalg::Matrix<3, 3> C3D8Element::computeJacobian(const linalg::Matrix<3, 8> &referenceGradients,
                                                  const linalg::Matrix<8, 3> &coordinates) {
    linalg::Matrix<3, 3> jacobian{};
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

linalg::Matrix<3, 8> C3D8Element::computeShapeGradientsGlobal(const linalg::Matrix<3, 8> &referenceGradients,
                                                              const linalg::Matrix<3, 3> &jacobian) {
    const auto inverseJacobian = linalg::inverse(jacobian);
    linalg::Matrix<3, 8> globalGradients{};
    for (int physical = 0; physical < 3; ++physical) {
        for (int node = 0; node < 8; ++node) {
            for (int natural = 0; natural < 3; ++natural)
                globalGradients[physical][node] += inverseJacobian[physical][natural] * referenceGradients[natural][node];
        }
    }
    return globalGradients;
}

linalg::Matrix<6, 24> C3D8Element::computeB(const linalg::Matrix<3, 8> &globalGradients) {
    linalg::Matrix<6, 24> B{};
    for (int node = 0; node < 8; ++node) {
        const int col = 3 * node;
        const double dNdx = globalGradients[0][node];
        const double dNdy = globalGradients[1][node];
        const double dNdz = globalGradients[2][node];
        // 正应变
        B[0][col] = dNdx;
        B[1][col + 1] = dNdy;
        B[2][col + 2] = dNdz;
        // 工程剪应变
        B[3][col] = dNdy;
        B[3][col + 1] = dNdx;
        B[4][col] = dNdz;
        B[4][col + 2] = dNdx;
        B[5][col + 1] = dNdz;
        B[5][col + 2] = dNdy;
    }
    return B;
}

linalg::Matrix<6, 24> C3D8Element::computeVolumetricB(const linalg::Matrix<6, 24> &B) {
    linalg::Matrix<6, 24> Bvol{};
    for (int col = 0; col < 24; ++col) {
        const double mean = (B[0][col] + B[1][col] + B[2][col]) / 3.0;
        Bvol[0][col] = mean;
        Bvol[1][col] = mean;
        Bvol[2][col] = mean;
    }
    return Bvol;
}

void C3D8Element::initializeGeometry() {
    const double g = 1.0 / std::sqrt(3.0);
    const std::array<linalg::Vector<3>, 8> gaussPoints{
        {{-g, -g, -g}, {g, -g, -g}, {g, g, -g}, {-g, g, -g}, {-g, -g, g}, {g, -g, g}, {g, g, g}, {-g, g, g}}};
    constexpr double weight = 1.0;
    const auto coordinates = nodeCoordinates();
    volume_ = 0.0;
    linalg::Matrix<6, 24> averageBvol{};
    // 01. 保存B-Bvol, 累计Bvol的体积积分
    for (std::size_t gp = 0; gp < gaussPoints.size(); ++gp) {
        const auto referenceGradients = computeShapeGradientsReference(gaussPoints[gp]);        // 对自然坐标偏导
        const auto jacobian = computeJacobian(referenceGradients, coordinates);                 // 雅可比矩阵
        const auto globalGradients = computeShapeGradientsGlobal(referenceGradients, jacobian); // 对物理坐标偏导
        const auto B = computeB(globalGradients);                                               // 组装应变矩阵B
        const auto Bvol = computeVolumetricB(B);                                                // 组装体积应变矩阵
        auto &point = geometry_[gp];
        point.integrationFactor = linalg::determinant(jacobian) * weight; // 体积积分因子
        volume_ += point.integrationFactor;                               // 单元体积
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 24; ++col) {
                point.Bbar[row][col] = B[row][col] - Bvol[row][col];
                averageBvol[row][col] += Bvol[row][col] * point.integrationFactor;
            }
        }
    }
    // 02. 体积积分除以单元体积, 得到单元平均体积项
    for (auto &row : averageBvol) {
        for (double &value : row)
            value /= volume_;
    }
    // 03. 每个积分点加上单元平均体积项
    for (auto &point : geometry_) {
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 24; ++col)
                point.Bbar[row][col] += averageBvol[row][col];
        }
    }
}