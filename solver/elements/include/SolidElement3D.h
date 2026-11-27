#pragma once

#include "ElementState.h"
#include "Matrix.h"
#include "MeshData.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

class Material3D;

/**
 * @brief 实体单元基类
 */
class SolidElement3D {
public:
    virtual ~SolidElement3D() = default;

    /**
     * @brief 获取单元拓扑节点
     * @return 单元拓扑节点
     */
    const std::vector<MeshNode> &nodes() const { return nodes_; }

    /**
     * @brief 获取单元拓扑节点数量
     * @return 单元拓扑节点数量
     */
    std::size_t nodeCount() const { return nodes_.size(); }

    /**
     * @brief 获取单元自由度数量
     * @return 单元自由度数量
     */
    std::size_t dofCount() const { return 3 * nodes_.size(); }

    /**
     * @brief 获取积分点数量
     * @return 积分点数量
     */
    std::size_t integrationPointCount() const { return integrationPointCount_; }
    ElementState createState() const { return ElementState(integrationPointCount_); }

    virtual linalg::MatrixX computeElasticStiffness() const = 0;
    virtual void updateTrialState(const linalg::VectorX &elementDisplacements, double timeIncrement, ElementState &state) const = 0;
    virtual std::vector<StressStrainState> extrapolateNodalStates(const std::vector<IntegrationPointState> &points) const = 0;
    virtual linalg::VectorX computeLumpedMass() const { throw std::invalid_argument("Element does not support lumped mass"); }

protected:
    /**
     * @brief 由派生类调用, 初始化公共数据
     * @param nodes 按单元连接顺序排列的节点
     * @param material 单元材料
     * @param integrationPointCount 积分点数量
     */
    SolidElement3D(std::vector<MeshNode> nodes, std::shared_ptr<const Material3D> material, std::size_t integrationPointCount)
        : nodes_(std::move(nodes)), material_(std::move(material)), integrationPointCount_(integrationPointCount) {
        if (!material_)
            throw std::invalid_argument("Solid element requires a material");
    }

    std::vector<MeshNode> nodes_;
    std::shared_ptr<const Material3D> material_;
    std::size_t integrationPointCount_;
};
