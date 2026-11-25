#include "C3D8Element.h"
#include "LinearElastic.h"
#include "MaterialData.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
class HistoryMaterial : public Material3D {
public:
    const linalg::Matrix<6, 6> &initialElasticMatrix() const override { return matrix_; }

    void integrate(const MaterialPointState &committed, const linalg::Vector<6> &strain, double timeIncrement,
                   MaterialPointState &trial) const override {
        const double previous = committed.internalVariables.empty() ? 0.0 : committed.internalVariables.front();
        trial.fields.strain = strain;
        trial.internalVariables.assign(1, previous + timeIncrement);
    }

private:
    linalg::Matrix<6, 6> matrix_{};
};
}  // namespace

int main() {
    // 01. 拓扑节点
    std::vector<MeshNode> nodes{{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {1, 1, 0}}, {4, {0, 1, 0}},
                                {5, {0, 0, 1}}, {6, {1, 0, 1}}, {7, {1, 1, 1}}, {8, {0, 1, 1}}};
    // 02. 单元初始化, 材料id暂时不起作用
    const auto material = std::make_shared<const LinearElasticMaterial>(IsotropicElasticData{1000.0, 0.25});
    const C3D8Element element(nodes, material);
    // 03. 计算弹性刚度矩阵
    const auto ke = element.computeElasticStiffness();
    // 04. 验证体积和单元刚度矩阵
    assert(std::abs(element.volume() - 1.0) < 1e-12);
    assert(std::abs(ke(0, 0) - 189.8148148148) < 1e-9);
    // 05. 验证单元刚度矩阵是否对称
    for (std::size_t row = 0; row < ke.rows(); ++row) {
        for (std::size_t col = 0; col < ke.cols(); ++col)
            assert(std::abs(ke(row, col) - ke(col, row)) < 1e-9);
    }

    // 在高斯点采样完整三线性场，外推应恢复每个角点的解析值。
    const auto field = [](double r, double s, double t) {
        return 1.0 + 2.0 * r + 3.0 * s - 4.0 * t + 5.0 * r * s - 6.0 * r * t + 7.0 * s * t + 8.0 * r * s * t;
    };
    const double g = 1.0 / std::sqrt(3.0);
    std::array<IntegrationPointState, 8> samples{};
    for (std::size_t gp = 0; gp < samples.size(); ++gp) {
        const auto &[x, y, z] = nodes[gp].coordinates;
        const double value = field(g * (2.0 * x - 1.0), g * (2.0 * y - 1.0), g * (2.0 * z - 1.0));
        for (std::size_t component = 0; component < samples[gp].strain.size(); ++component) {
            samples[gp].strain[component] = value + static_cast<double>(component);
            samples[gp].stress[component] = -3.0 * value + static_cast<double>(component);
        }
    }
    const auto extrapolated = C3D8Element::extrapolateToNodes(samples);
    for (std::size_t node = 0; node < extrapolated.size(); ++node) {
        const auto &[x, y, z] = nodes[node].coordinates;
        const double expected = field(2.0 * x - 1.0, 2.0 * y - 1.0, 2.0 * z - 1.0);
        for (std::size_t component = 0; component < extrapolated[node].strain.size(); ++component) {
            const double offset = static_cast<double>(component);
            assert(std::abs(extrapolated[node].strain[component] - (expected + offset)) < 1e-12);
            assert(std::abs(extrapolated[node].stress[component] - (-3.0 * expected + offset)) < 1e-12);
        }
    }

    // 畸变单元映射 y = (1 + x) * eta，雅可比权重随 x 变化。
    auto distortedNodes = nodes;
    linalg::VectorX displacements(24);
    for (std::size_t i = 0; i < distortedNodes.size(); ++i) {
        auto &[x, y, z] = distortedNodes[i].coordinates;
        y *= 1.0 + x;
        displacements[3 * i + 2] = x * y;
    }
    // 积分点恢复应与求解器的 B̄ 刚度给出相同的单元应变能。
    const C3D8Element distorted(distortedNodes, material);
    for (std::size_t i = 0; i < distortedNodes.size(); ++i)
        displacements[3 * i] = distortedNodes[i].coordinates[0] * distortedNodes[i].coordinates[1];
    auto state = distorted.createState();
    distorted.updateTrialState(displacements, 0.25, state);
    assert(state.committed.front().fields.stress[0] == 0.0);
    const auto trialStress = state.trial.front().fields.stress;
    assert(std::abs(trialStress[0]) > 1e-12);
    state.revert();
    assert(state.trial.front().fields.stress == state.committed.front().fields.stress);
    distorted.updateTrialState(displacements, 0.25, state);
    state.commit();
    assert(state.committed.front().fields.stress == trialStress);
    const auto distortedStiffness = distorted.computeElasticStiffness();
    double stiffnessWork = 0.0;
    for (std::size_t i = 0; i < displacements.size(); ++i) {
        for (std::size_t j = 0; j < displacements.size(); ++j)
            stiffnessWork += displacements[i] * distortedStiffness(i, j) * displacements[j];
    }
    double stressWork = 0.0;
    for (std::size_t gp = 0; gp < state.committed.size(); ++gp) {
        const double x = 0.5 * (1.0 + (gp % 4 == 1 || gp % 4 == 2 ? g : -g));
        const double factor = (1.0 + x) / 8.0;
        const auto &fields = state.committed[gp].fields;
        for (std::size_t component = 0; component < fields.strain.size(); ++component)
            stressWork += factor * fields.strain[component] * fields.stress[component];
    }
    assert(std::abs(stressWork - stiffnessWork) < 1e-9);

    const C3D8Element historyElement(nodes, std::make_shared<const HistoryMaterial>());
    auto history = historyElement.createState();
    historyElement.updateTrialState(displacements, 0.25, history);
    historyElement.updateTrialState(displacements, 0.25, history);
    assert(history.trial.front().internalVariables.front() == 0.25);
    history.commit();
    historyElement.updateTrialState(displacements, 0.5, history);
    assert(history.trial.front().internalVariables.front() == 0.75);
    history.revert();
    assert(history.trial.front().internalVariables.front() == 0.25);

    // 06. 保持节点顺序, 把单元沿x方向镜像
    for (auto &node : nodes)
        node.coordinates[0] = 1.0 - node.coordinates[0];
    // 07. 验证镜像后的单元是否被拒绝, 预期被拒绝, 因为没有按约定排列节点
    bool rejected = false;
    try {
        const C3D8Element inverted(nodes, material);
    } catch (const std::domain_error &) {
        rejected = true;
    }
    assert(rejected);
    std::cout << "C3D8 smoke test passed\n";
}
