#pragma once

#include "Matrix.h"
#include "SolidElement3D.h"

#include <memory>
#include <optional>
#include <unordered_map>

struct FEModel;
class AnalysisModel;

/**
 * @brief 计算模型类
 * @details 用于存储和管理计算模型
 */
class AnalysisModel {
public:
    explicit AnalysisModel(const FEModel &model);

    /**
     * @brief 向计算模型中添加一个单元
     * @param[in] element 单元指针
     * @param[in] sourceId 源网格或源节点id, 用于标识单元的来源
     * @return 单元在计算模型中的索引
     */
    std::size_t addElement(std::unique_ptr<SolidElement3D> element, std::optional<MeshId> sourceId = std::nullopt);

    /**
     * @brief 从计算模型中移除一个单元
     * @param[in] number 单元在计算模型中的1-based索引
     */
    void removeElement(std::size_t number);

    /**
     * @brief 获取计算模型中单元的数量
     * @return 单元数量
     */
    std::size_t elementCount() const { return elements_.size(); }

    std::size_t globalDofCount() const { return 3 * nodeIndexById_.size(); }

    /**
     * @brief 获取计算模型中指定源节点在指定方向的0-based全局自由度索引
     */
    std::size_t globalDofIndex(MeshId nodeId, int direction) const;

    const linalg::VectorX &forceVector() const noexcept { return forceVector_; }

    const std::vector<std::optional<double>> &prescribedDisplacements() const noexcept { return prescribedDisplacements_; }

    /**
     * @brief 获取计算模型中指定1-based索引的单元的引用
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 单元指针的解引用
     */
    const SolidElement3D &element(std::size_t number) const { return *elements_[indexOf(number)].element; }

    /**
     * @brief 获取计算模型中指定1-based索引的单元的源id
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 源id
     */
    std::optional<MeshId> sourceElementId(std::size_t number) const { return elements_[indexOf(number)].sourceId; }

private:
    /**
     * @brief 用于计算的单元实体定义
     * @details 包含单元指针和源id
     */
    struct Entry {
        std::unique_ptr<SolidElement3D> element; // 单元指针
        std::optional<MeshId> sourceId;          // 源网格或源节点id
    };

    /**
     * @brief 将1-based的单元索引转换为0-based索引
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 单元在计算模型中的0-based索引
     * @throws std::out_of_range 如果单元索引无效
     */
    std::size_t indexOf(std::size_t number) const;

    void initializeNodalConditions(const FEModel &model);

private:
    /**
     * @brief 计算模型中的单元实体
     * @details 包含所有添加的单元实体
     */
    std::vector<Entry> elements_;

    /**
     * @brief 节点源id->0-based节点索引的映射
     * @details 值为连续的0-based节点索引
     */
    std::unordered_map<MeshId, std::size_t> nodeIndexById_;

    linalg::VectorX forceVector_;
    std::vector<std::optional<double>> prescribedDisplacements_;
};
