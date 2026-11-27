#pragma once

#include "AnalysisElement.h"
#include "ElasticResultFrame.h"

/**
 * @brief 单元状态与算法管理类
 * @details 管理所有单元的积分点历史，持有单元定义副本与算法共享指针，可跨求解调用保留状态
 */
class ElementStatesManager {
public:
    ElementStatesManager(const std::vector<AnalysisElement> &elements, std::size_t dofCount);

    /// @brief 检查是否有单元状态记录
    /// @return true 如果有记录，false 否则
    bool empty() const { return states_.empty(); }

    void updateTrial(const linalg::VectorX &displacements, double timeIncrement);
    void commit();
    void revert();
    // 从当前试算应力/算法切线组装，必须先调用 updateTrial。
    void assemble(linalg::VectorX &internalForces, linalg::MatrixX &tangent) const;
    std::size_t dofCount() const { return dofCount_; }
    ElasticResultFrame snapshot(double time) const;

private:
    // 按值持有定义，shared_ptr 保证单元与材料不依赖 builder 的生命周期。
    std::vector<AnalysisElement> elements_;
    std::size_t dofCount_;
    std::vector<ElementState> states_;
    linalg::VectorX localDisplacements_;
};
