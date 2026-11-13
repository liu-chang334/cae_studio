#include "C3D8Element.h"
#include "LinearElastic.h"
#include "MaterialData.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    // 01. 拓扑节点
    std::vector<MeshNode> nodes{{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {1, 1, 0}}, {4, {0, 1, 0}},
                                {5, {0, 0, 1}}, {6, {1, 0, 1}}, {7, {1, 1, 1}}, {8, {0, 1, 1}}};
    // 02. 单元初始化, 材料id暂时不起作用
    const auto material = std::make_shared<const LinearElasticMaterial>(IsotropicElasticData{1000.0, 0.25});
    const C3D8Element element(1, nodes, material);
    // 03. 计算弹性刚度矩阵
    const auto ke = element.computeElasticStiffness();
    // 04. 验证体积和单元刚度矩阵
    assert(std::abs(element.volume() - 1.0) < 1e-12);
    assert(std::abs(ke[0][0] - 189.8148148148) < 1e-9);
    // 05. 验证单元刚度矩阵是否对称
    for (std::size_t row = 0; row < ke.size(); ++row) {
        for (std::size_t col = 0; col < ke[row].size(); ++col)
            assert(std::abs(ke[row][col] - ke[col][row]) < 1e-9);
    }

    // 06. 保持节点顺序, 把单元沿x方向镜像
    for (auto &node : nodes)
        node.coordinates[0] = 1.0 - node.coordinates[0];
    // 07. 验证镜像后的单元是否被拒绝, 预期被拒绝, 因为没有按约定排列节点
    bool rejected = false;
    try {
        const C3D8Element inverted(2, nodes, material);
    } catch (const std::domain_error &) {
        rejected = true;
    }
    assert(rejected);
    std::cout << "C3D8 smoke test passed\n";
}