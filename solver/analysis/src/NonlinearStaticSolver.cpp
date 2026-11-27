#include "NonlinearStaticSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

AnalysisContext::AnalysisContext(const NonlinearStaticProblem &problem)
    : elementStates(problem.elements, problem.loads.size()), displacements(problem.loads.size(), 0.0),
      appliedLoads(problem.loads.size(), 0.0), prescribedDisplacements(problem.prescribedDisplacements) {
    if (problem.elements.empty() || problem.loads.empty() || prescribedDisplacements.size() != problem.loads.size())
        throw std::invalid_argument("Invalid nonlinear static problem");
    // 初始约束为零, 目标非零位移随首个历程施加
    for (auto &value : prescribedDisplacements)
        if (value)
            value = 0.0;
}

NonlinearStaticResult NonlinearStaticSolver::solve(AnalysisContext &context, const linalg::VectorX &targetLoads,
                                                   const std::vector<std::optional<double>> &targetDisplacements,
                                                   const NonlinearStaticOptions &options) const {
    const auto n = context.elementStates.dofCount();
    if (targetLoads.size() != n || targetDisplacements.size() != n || context.displacements.size() != n ||
        context.appliedLoads.size() != n || context.prescribedDisplacements.size() != n || !std::isfinite(options.duration) ||
        options.duration <= 0.0 || !std::isfinite(options.initialIncrement) || options.initialIncrement <= 0.0 ||
        options.initialIncrement > 1.0 || !std::isfinite(options.minimumIncrement) || options.minimumIncrement <= 0.0 ||
        options.minimumIncrement > options.initialIncrement || options.maxIterations == 0 ||
        !std::isfinite(options.relativeTolerance) || options.relativeTolerance < 0.0 || !std::isfinite(options.absoluteTolerance) ||
        options.absoluteTolerance <= 0.0 || !std::isfinite(context.time) || !std::isfinite(context.time + options.duration) ||
        context.time + options.duration <= context.time)
        throw std::invalid_argument("Invalid nonlinear solver dimensions or options");
    std::vector<std::size_t> free;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(targetLoads[i]) || !std::isfinite(context.appliedLoads[i]) || !std::isfinite(context.displacements[i]) ||
            (targetDisplacements[i] && !std::isfinite(*targetDisplacements[i])) ||
            (context.prescribedDisplacements[i] && !std::isfinite(*context.prescribedDisplacements[i])) ||
            targetDisplacements[i].has_value() != context.prescribedDisplacements[i].has_value())
            throw std::invalid_argument("Invalid loading or changed constraint topology");
        if (!targetDisplacements[i])
            free.push_back(i);
    }
    const auto startLoads = context.appliedLoads;
    const auto startBoundary = context.prescribedDisplacements;
    const double startTime = context.time;
    NonlinearStaticResult result;
    result.frames.push_back({context.time, context.displacements});
    result.materialFrames.push_back(context.elementStates.snapshot(context.time));
    double fraction = 0.0, increment = options.initialIncrement;
    while (fraction < 1.0) {
        double next = std::min(1.0, fraction + increment);
        if (1.0 - next < 32.0 * std::numeric_limits<double>::epsilon())
            next = 1.0;
        if (next <= fraction)
            throw std::runtime_error("Nonlinear increment cannot advance");
        if (startTime + next * options.duration <= context.time)
            throw std::runtime_error("Nonlinear pseudo time increment cannot advance");
        auto u = context.displacements;
        auto loads = startLoads;
        auto boundary = startBoundary;
        for (std::size_t i = 0; i < n; ++i) {
            loads[i] += next * (targetLoads[i] - startLoads[i]);
            if (boundary[i]) {
                boundary[i] = *startBoundary[i] + next * (*targetDisplacements[i] - *startBoundary[i]);
                u[i] = *boundary[i];
            }
        }
        linalg::VectorX internal;
        linalg::MatrixX tangent(0, 0);
        bool converged = false;
        try {
            for (std::size_t iteration = 0; iteration <= options.maxIterations; ++iteration) {
                context.elementStates.updateTrial(u, (next - fraction) * options.duration);
                context.elementStates.assemble(internal, tangent);
                double residualNorm = 0.0, scale = 0.0;
                for (auto i : free) {
                    if (!std::isfinite(internal[i]))
                        throw std::runtime_error("Non-finite internal force");
                    residualNorm = std::max(residualNorm, std::abs(loads[i] - internal[i]));
                    scale = std::max({scale, std::abs(loads[i]), std::abs(internal[i])});
                }
                if (residualNorm <= options.absoluteTolerance + options.relativeTolerance * scale) {
                    converged = true;
                    break;
                }
                if (iteration == options.maxIterations)
                    break;
                linalg::MatrixX reduced(free.size(), free.size());
                linalg::VectorX residual(free.size());
                for (std::size_t i = 0; i < free.size(); ++i) {
                    residual[i] = loads[free[i]] - internal[free[i]];
                    for (std::size_t j = 0; j < free.size(); ++j)
                        reduced(i, j) = tangent(free[i], free[j]);
                }
                const auto correction = linalg::solveCholesky(reduced, residual);
                for (std::size_t i = 0; i < free.size(); ++i)
                    u[free[i]] += correction[i];
            }
        } catch (...) {
            context.elementStates.revert();
            throw;
        }
        if (!converged) {
            context.elementStates.revert();
            increment *= 0.5;
            ++result.cutbacks;
            if (increment < options.minimumIncrement)
                throw std::runtime_error("Nonlinear increment failed to converge at minimum increment; last converged state retained");
            continue;
        }
        context.elementStates.commit();
        context.displacements = std::move(u);
        context.appliedLoads = std::move(loads);
        context.prescribedDisplacements = std::move(boundary);
        context.time = startTime + next * options.duration;
        fraction = next;
        result.reactions.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i)
            if (context.prescribedDisplacements[i])
                result.reactions[i] = internal[i] - context.appliedLoads[i];
        result.frames.push_back({context.time, context.displacements});
        result.materialFrames.push_back(context.elementStates.snapshot(context.time));
    }
    return result;
}
