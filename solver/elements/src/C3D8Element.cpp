#include "C3D8Element.h"
#include "Material3D.h"
#include "Matrix.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
// 对应 C3D8 单元连接顺序的自然坐标。
constexpr int nodeSigns[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
const double gaussCoordinate = 1.0 / std::sqrt(3.0);
}  // namespace

C3D8Element::C3D8Element(std::vector<MeshNode> nodes, std::shared_ptr<const Material3D> material)
    : SolidElement3D(std::move(nodes), std::move(material), 8) {
    if (nodeCount() != 8) {
        throw std::invalid_argument("C3D8 requires exactly 8 nodes");
    }
    // 初始化各积分点的Bbar, 积分因子和单元体积
    initializeGeometry();
}

linalg::Matrix<8, 3> C3D8Element::nodeCoordinates() const {
    linalg::Matrix<8, 3> coordinates;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 3; ++j)
            coordinates[i][j] = nodes_[i].coordinates[j];
    }
    return coordinates;
}

linalg::Vector<8> C3D8Element::computeShapeFunctions(const linalg::Vector<3> &naturalCoordinates) {
    const auto &[r, s, t] = naturalCoordinates;
    linalg::Vector<8> values{};
    for (std::size_t node = 0; node < values.size(); ++node) {
        const auto &[ri, si, ti] = nodeSigns[node];
        values[node] = 0.125 * (1.0 + ri * r) * (1.0 + si * s) * (1.0 + ti * t);
    }
    return values;
}

linalg::Matrix<3, 8> C3D8Element::computeShapeGradientsReference(const linalg::Vector<3> &naturalCoordinates) {
    const double r = naturalCoordinates[0];
    const double s = naturalCoordinates[1];
    const double t = naturalCoordinates[2];
    linalg::Matrix<3, 8> gradients{};
    for (int i = 0; i < 8; ++i) {
        const double ri = nodeSigns[i][0];
        const double si = nodeSigns[i][1];
        const double ti = nodeSigns[i][2];
        gradients[0][i] = 0.125 * ri * (1.0 + si * s) * (1.0 + ti * t);  // 对r偏导
        gradients[1][i] = 0.125 * si * (1.0 + ti * t) * (1.0 + ri * r);  // 对s偏导
        gradients[2][i] = 0.125 * ti * (1.0 + ri * r) * (1.0 + si * s);  // 对t偏导
    }
    return gradients;
}

linalg::Matrix<3, 3> C3D8Element::computeJacobian(const linalg::Matrix<3, 8> &referenceGradients,
                                                  const linalg::Matrix<8, 3> &coordinates) {
    linalg::Matrix<3, 3> jacobian = linalg::multiply(referenceGradients, coordinates);
    // [ dx/dr  dy/dr  dz/dr ]
    // [ dx/ds  dy/ds  dz/ds ]
    // [ dx/dt  dy/dt  dz/dt ]
    return jacobian;
}

linalg::Matrix<3, 8> C3D8Element::computeShapeGradientsGlobal(const linalg::Matrix<3, 8> &referenceGradients,
                                                              const linalg::Matrix<3, 3> &jacobian) {
    const auto inverseJacobian = linalg::inverse_3x3(jacobian);
    linalg::Matrix<3, 8> globalGradients = linalg::multiply(inverseJacobian, referenceGradients);
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
    constexpr double weight = 1.0;
    const auto coordinates = nodeCoordinates();
    volume_ = 0.0;
    nodalVolumes_.fill(0.0);
    linalg::Matrix<6, 24> averageBvol{};
    // 01. 保存B-Bvol, 累计Bvol的体积积分
    for (std::size_t gp = 0; gp < geometryCache_.size(); ++gp) {
        const auto &[r, s, t] = nodeSigns[gp];
        const linalg::Vector<3> natural{r * gaussCoordinate, s * gaussCoordinate, t * gaussCoordinate};
        const auto referenceGradients = computeShapeGradientsReference(natural);  // 对自然坐标偏导
        const auto jacobian = computeJacobian(referenceGradients, coordinates);  // 雅可比矩阵
        const double detJ = linalg::determinant_3x3(jacobian);
        // 02. 检查雅可比矩阵是否退化
        if (!std::isfinite(detJ) || detJ <= 0.0)
            throw std::domain_error("C3D8 element , Gauss point " + std::to_string(gp + 1) + ": detJ must be finite and positive");
        const auto globalGradients = computeShapeGradientsGlobal(referenceGradients, jacobian);  // 对物理坐标偏导
        const auto B = computeB(globalGradients);  // 组装应变矩阵B
        const auto Bvol = computeVolumetricB(B);  // 组装体积应变矩阵
        auto &point = geometryCache_[gp];
        point.integrationFactor = detJ * weight;  // 体积积分因子
        volume_ += point.integrationFactor;  // 单元体积
        const auto shape = computeShapeFunctions(natural);
        for (std::size_t node = 0; node < nodalVolumes_.size(); ++node)
            nodalVolumes_[node] += shape[node] * point.integrationFactor;
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 24; ++col) {
                point.Bbar[row][col] = B[row][col] - Bvol[row][col];
                averageBvol[row][col] += Bvol[row][col] * point.integrationFactor;
            }
        }
    }
    // 03. 体积积分除以单元体积, 得到单元平均体积项
    for (auto &row : averageBvol) {
        for (double &value : row)
            value /= volume_;
    }
    // 04. 每个积分点加上单元平均体积项
    for (auto &point : geometryCache_) {
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 24; ++col)
                point.Bbar[row][col] += averageBvol[row][col];
        }
    }
}

linalg::MatrixX C3D8Element::computeElasticStiffness() const {
    const auto n = dofCount();  // 获取自由度数量
    linalg::MatrixX ke(n, n);  // 初始化单刚容器
    const auto D = material_->initialElasticMatrix();  // TODO: 固定获取初始弹性刚度矩阵, 未来可以扩展到弹塑性
    for (const auto &point : geometryCache_) {
        const auto &Bbar = point.Bbar;
        // (6×6) × (6×24) → 6×24
        const auto DB = linalg::multiply(D, Bbar);  // 6×24
        const auto pointStiffness = linalg::multiply(linalg::transpose(Bbar), DB);  // 24×24
        for (std::size_t row = 0; row < ke.rows(); ++row) {
            for (std::size_t col = 0; col < ke.cols(); ++col) {
                ke(row, col) += pointStiffness[row][col] * point.integrationFactor;  // 高斯积分ke
            }
        }
    }
    return ke;
}

linalg::VectorX C3D8Element::computeLumpedMass() const {
    const auto density = material_->density();
    if (!density || !std::isfinite(*density) || *density <= 0.0)
        throw std::invalid_argument("C3D8 requires finite positive density for explicit dynamics");

    linalg::VectorX mass(dofCount());
    for (std::size_t node = 0; node < nodalVolumes_.size(); ++node) {
        const double nodalMass = *density * nodalVolumes_[node];
        for (std::size_t direction = 0; direction < 3; ++direction)
            mass[3 * node + direction] = nodalMass;
    }
    return mass;
}

void C3D8Element::updateTrialState(const linalg::VectorX &elementDisplacements, double timeIncrement, ElementState &state) const {
    if (elementDisplacements.size() != dofCount() || state.committed.size() != geometryCache_.size() ||
        state.trial.size() != geometryCache_.size())
        throw std::invalid_argument("C3D8 requires 24 displacement components and 8 material points");

    for (std::size_t gp = 0; gp < geometryCache_.size(); ++gp) {
        const auto &Bbar = geometryCache_[gp].Bbar;  // TODO: 目前固定取修正Bbar, 未来可扩展为用户可选的标准B或修正Bbar
        linalg::Vector<6> strain{};  // 单积分点应变状态容器
        for (std::size_t row = 0; row < strain.size(); ++row) {
            for (std::size_t col = 0; col < elementDisplacements.size(); ++col)
                strain[row] += Bbar[row][col] * elementDisplacements[col];  // strain=B*displacement
        }
        material_->integrate(state.committed[gp], strain, timeIncrement, state.trial[gp]);  // 更新当前积分点试算材料状态
    }
}

std::array<StressStrainState, 8> C3D8Element::extrapolateToNodes(const std::array<IntegrationPointState, 8> &states) {
    // 在高斯点构成的参考立方体内，各高斯点坐标缩放为 ±1，原单元节点坐标缩放为 ±sqrt(3)。
    static const auto weights = [] {
        linalg::Matrix<8, 8> result{};
        for (std::size_t node = 0; node < result.size(); ++node) {
            const auto &[r, s, t] = nodeSigns[node];
            result[node] = computeShapeFunctions({r / gaussCoordinate, s / gaussCoordinate, t / gaussCoordinate});
        }
        return result;
    }();

    std::array<StressStrainState, 8> nodalStates{};
    for (std::size_t node = 0; node < nodalStates.size(); ++node) {
        auto &state = nodalStates[node];
        for (std::size_t gp = 0; gp < states.size(); ++gp) {
            const double weight = weights[node][gp];
            for (std::size_t component = 0; component < state.strain.size(); ++component) {
                state.strain[component] += weight * states[gp].strain[component];
                state.stress[component] += weight * states[gp].stress[component];
            }
        }
        const auto finite = [](double value) { return std::isfinite(value); };
        if (!std::all_of(state.strain.begin(), state.strain.end(), finite) ||
            !std::all_of(state.stress.begin(), state.stress.end(), finite))
            throw std::domain_error("C3D8 extrapolated result contains a non-finite component");
    }
    return nodalStates;
}

std::vector<StressStrainState> C3D8Element::extrapolateNodalStates(const std::vector<IntegrationPointState> &points) const {
    std::array<IntegrationPointState, 8> states{};
    if (points.size() != states.size())
        throw std::invalid_argument("C3D8 extrapolation requires 8 integration points");
    std::copy(points.begin(), points.end(), states.begin());
    const auto nodalStates = extrapolateToNodes(states);
    return {nodalStates.begin(), nodalStates.end()};
}
