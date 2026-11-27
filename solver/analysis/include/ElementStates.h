#pragma once

#include "AnalysisElement.h"
#include "ElasticResultFrame.h"

/**
 * @brief 单元状态与算法管理类
 * @details 管理一次求解过程中所有单元的积分点状态, 并持有单元算法的引用, 方便更新试算状态
 */
class ElementStates {
public:
    ElementStates(const std::vector<AnalysisElement> &elements, std::size_t dofCount);
    bool empty() const { return states_.empty(); }

    void updateTrial(const linalg::VectorX &displacements, double timeIncrement);
    void commit();
    ElasticResultFrame snapshot(double time) const;

private:
    const std::vector<AnalysisElement> &elements_;
    std::size_t dofCount_;
    std::vector<ElementState> states_;
    linalg::VectorX localDisplacements_;
};
