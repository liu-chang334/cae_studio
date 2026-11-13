#pragma once

#include "SolidElement3D.h"

#include <memory>
#include <optional>

struct Entry;
struct FEModel;
class AnalysisModel;

/**
 * @brief 从FEModel构建计算模型
 * @param[in] model FE模型
 * @return AnalysisModel
 */
AnalysisModel buildAnalysisModel(const FEModel &model);

class AnalysisModel {
public:
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
    std::size_t elementCount() const;

    /**
     * @brief 获取计算模型中指定1-based索引的单元的引用
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 单元指针的解引用
     */
    const SolidElement3D &element(std::size_t number) const;

    /**
     * @brief 获取计算模型中指定1-based索引的单元的源id
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 源id
     */
    std::optional<MeshId> sourceElementId(std::size_t number) const;

private:
    /**
     * @brief 将1-based的单元索引转换为0-based索引
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 单元在计算模型中的0-based索引
     * @throws std::out_of_range 如果单元索引无效
     */
    std::size_t indexOf(std::size_t number) const;

private:
    std::vector<Entry> elements_;
};
