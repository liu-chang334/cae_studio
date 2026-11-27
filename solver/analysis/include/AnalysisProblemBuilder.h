#pragma once

#include "ExplicitDynamicsProblem.h"
#include "Matrix.h"
#include "NonlinearStaticSolver.h"
#include "RawLinearSystem.h"
#include "SolidElement3D.h"

#include <memory>
#include <optional>
#include <unordered_map>

struct FEModel;
enum class AnalysisType;

/**
 * @brief 计算模型类
 * @details 用于存储和管理计算模型
 */
class AnalysisProblemBuilder {
public:
    explicit AnalysisProblemBuilder(const FEModel &model);

    /// @brief 全量格式线弹性静力
    RawLinearSystem buildLinearStaticSystem() const;
    NonlinearStaticProblem buildNonlinearStaticProblem() const;
    ExplicitDynamicsProblem buildExplicitDynamicsProblem() const;

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

    /**
     * @brief 获取计算模型的全局自由度数量
     * @return 全局自由度数量
     */
    std::size_t globalDofCount() const { return 3 * nodeIndexById_.size(); }

    /**
     * @brief 获取计算模型中指定源节点在指定方向的0-based全局自由度索引
     * @param[in] nodeId 节点源id
     * @param[in] direction 方向, 1: x, 2: y, 3: z
     */
    std::size_t globalDofIndex(MeshId nodeId, int direction) const;

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
        std::shared_ptr<const SolidElement3D> element;  // 单元几何和材料定义
        std::optional<MeshId> sourceId;  // 源网格或源节点id
    };

    /**
     * @brief 将1-based的单元索引转换为0-based索引
     * @param[in] number 单元在计算模型中的1-based索引
     * @return 单元在计算模型中的0-based索引
     * @throws std::out_of_range 如果单元索引无效
     */
    std::size_t indexOf(std::size_t number) const;

    /**
     * @brief 初始化节点边界条件
     * @param[in] model 有限元模型
     * @details 初始化外力边界条件和位移边界条件
     */
    void initializeNodalConditions(const FEModel &model);

    std::vector<std::size_t> elementDofs(const SolidElement3D &element) const;
    std::vector<AnalysisElement> analysisElements() const;
    linalg::MatrixX assembleElasticStiffness(const std::vector<AnalysisElement> &elements) const;

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

    /**
     * @brief 外力边界条件
     * @details 每个元素为力值, 用于施加在对应节点的对应方向上
     */
    linalg::VectorX forceVector_;
    std::vector<ExplicitNodalLoad> timeDependentLoads_;

    /**
     * @brief 位移边界条件
     * @details 每个元素为可选的位移值, 有值表示该方向上的位移被固定为该值, 否则表示该方向自由
     */
    std::vector<std::optional<double>> prescribedDisplacements_;
    AnalysisType analysisType_;
    bool hasPlasticity_ = false;
};
