#pragma once

#include "IntegrationPointState.h"
#include "MeshData.h"

#include <utility>
#include <vector>

class SolidElement3D {
public:
    virtual ~SolidElement3D() = default;

    /**
     * @brief 获取单元id
     * @return 单元id
     */
    MeshId id() const { return id_; }

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
     * @brief 获取材料数组中的0-based索引
     * @return 材料数组中的0-based索引
     */
    std::size_t materialIndex() const { return materialIndex_; }

    /**
     * @brief 获取积分点数量
     * @return 积分点数量
     */
    std::size_t integrationPointCount() const { return integrationPointStates_.size(); }

    /**
     * @brief 获取积分点状态
     * @return 积分点状态
     */
    const std::vector<IntegrationPointState> &integrationPointStates() const { return integrationPointStates_; }

protected:
    /**
     * @brief 由派生类调用, 初始化公共数据
     * @param id 单元id
     * @param nodes 按单元连接顺序排列的节点
     * @param materialIndex 材料数组中的0-based索引
     * @param integrationPointCount 积分点数量
     */
    SolidElement3D(MeshId id, std::vector<MeshNode> nodes, std::size_t materialIndex, std::size_t integrationPointCount)
        : id_(id), nodes_(std::move(nodes)), materialIndex_(materialIndex), integrationPointStates_(integrationPointCount) {}

    MeshId id_;
    std::vector<MeshNode> nodes_;
    std::size_t materialIndex_;
    std::vector<IntegrationPointState> integrationPointStates_;
};