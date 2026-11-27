#pragma once
#include "DisplacementFrame.h"
#include "ElementStatesManager.h"
#include <optional>

struct NonlinearStaticProblem {
    std::vector<AnalysisElement> elements;
    linalg::VectorX loads;
    std::vector<std::optional<double>> prescribedDisplacements;
};

/// @brief 一次分析任务的持续状态. 跨solve调用保留已收敛的材料历史和载荷
class AnalysisContext {
public:
    explicit AnalysisContext(const NonlinearStaticProblem &problem);

    /// @brief 单元状态与算法管理类
    ElementStatesManager elementStates;

    linalg::VectorX displacements;
    linalg::VectorX appliedLoads;
    std::vector<std::optional<double>> prescribedDisplacements;
    double time = 0.0;
};

struct NonlinearStaticOptions {
    double duration = 1.0;  // 本次加载历程的伪时间长度
    double initialIncrement = 0.1;  // 相对于本次历程的比例
    double minimumIncrement = 1e-5;
    std::size_t maxIterations = 25;
    double relativeTolerance = 1e-8;
    double absoluteTolerance = 1e-8;
};

struct NonlinearStaticResult {
    std::vector<DisplacementFrame> frames;
    std::vector<ElasticResultFrame> materialFrames;
    linalg::VectorX reactions;
    std::size_t cutbacks = 0;
};

class NonlinearStaticSolver {
public:
    // 目标载荷和指定位移从 context 的已提交值线性变化，可用于续加载或卸载。
    // 失败的增量回滚，之前已收敛的增量仍留在 context 中。
    NonlinearStaticResult solve(AnalysisContext &context, const linalg::VectorX &targetLoads,
                                const std::vector<std::optional<double>> &targetDisplacements,
                                const NonlinearStaticOptions &options = {}) const;
};
