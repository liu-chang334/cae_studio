#pragma once

#include "AnalysisElement.h"
#include "ElasticResultFrame.h"

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
