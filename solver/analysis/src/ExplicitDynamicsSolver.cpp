#include "ExplicitDynamicsSolver.h"
#include "FEModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

double stableTimeIncrement(const ExplicitDynamicsProblem &problem) {
    const auto size = problem.lumpedMass.size();
    linalg::VectorX rowSums(size, 0.0);
    for (const auto &element : problem.elements) {
        const auto &dofs = element.globalDofs;
        const auto &stiffness = element.elasticStiffness;
        if (dofs.size() != stiffness.rows() || dofs.size() != stiffness.cols() ||
            std::any_of(dofs.begin(), dofs.end(), [size](std::size_t dof) { return dof >= size; }))
            throw std::invalid_argument("Invalid element DOF map or stiffness dimensions");

        for (std::size_t i = 0; i < dofs.size(); ++i) {
            for (std::size_t j = 0; j < dofs.size(); ++j) {
                const double value = stiffness(i, j);
                if (!std::isfinite(value))
                    throw std::invalid_argument("Element stiffness must be finite");
                if (!problem.prescribedDisplacements[dofs[i]] && !problem.prescribedDisplacements[dofs[j]])
                    rowSums[dofs[i]] += std::abs(value);
            }
        }
    }

    // 单元绝对行和为全局 M^{-1}K 的行和提供上界，无需装配全局刚度矩阵。
    double eigenvalueBound = 0.0;
    for (std::size_t dof = 0; dof < size; ++dof) {
        if (!problem.prescribedDisplacements[dof])
            eigenvalueBound = std::max(eigenvalueBound, rowSums[dof] / problem.lumpedMass[dof]);
    }
    if (!std::isfinite(eigenvalueBound))
        throw std::overflow_error("Cannot determine a finite explicit stability bound");
    return eigenvalueBound > 0.0 ? 2.0 / std::sqrt(eigenvalueBound) : std::numeric_limits<double>::infinity();
}

void assembleInternalForces(const ExplicitDynamicsProblem &problem, const linalg::VectorX &displacements,
                            linalg::VectorX &internalForces) {
    std::fill(internalForces.begin(), internalForces.end(), 0.0);
    for (const auto &element : problem.elements) {
        const auto &dofs = element.globalDofs;
        const auto &stiffness = element.elasticStiffness;
        for (std::size_t i = 0; i < dofs.size(); ++i) {
            double force = 0.0;
            for (std::size_t j = 0; j < dofs.size(); ++j)
                force += stiffness(i, j) * displacements[dofs[j]];
            internalForces[dofs[i]] += force;
        }
    }
}

void validateTimeDependentLoads(const ExplicitDynamicsProblem &problem) {
    for (const auto &load : problem.timeDependentLoads) {
        if (load.amplitude.empty() || load.magnitudes.size() != problem.lumpedMass.size() ||
            !std::all_of(load.magnitudes.begin(), load.magnitudes.end(), [](double value) { return std::isfinite(value); }))
            throw std::invalid_argument("Invalid time-dependent nodal load");
        double previousTime = -1.0;
        for (const auto &point : load.amplitude) {
            if (!std::isfinite(point.time) || point.time < 0.0 || point.time <= previousTime || !std::isfinite(point.value))
                throw std::invalid_argument("Smooth step amplitudes require finite values and strictly increasing nonnegative times");
            previousTime = point.time;
        }
    }
}

double evaluateSmoothStep(const SmoothStepAmplitude &amplitude, double time) {
    if (time <= amplitude.front().time)
        return amplitude.front().value;
    if (time >= amplitude.back().time)
        return amplitude.back().value;
    const auto right = std::upper_bound(amplitude.begin(), amplitude.end(), time,
                                       [](double time, const AmplitudePoint &point) { return time < point.time; });
    const auto &left = *(right - 1);
    const double fraction = (time - left.time) / (right->time - left.time);
    const double weight = fraction * fraction * fraction * (10.0 + fraction * (-15.0 + 6.0 * fraction));
    return (1.0 - weight) * left.value + weight * right->value;
}

}  // namespace

ExplicitDynamicsResult ExplicitDynamicsSolver::solve(const ExplicitDynamicsProblem &problem,
                                                     const ExplicitDynamicsStep &step) const {
    if (!std::isfinite(step.duration) || step.duration <= 0.0 ||
        (step.timeIncrement && (!std::isfinite(*step.timeIncrement) || *step.timeIncrement <= 0.0)))
        throw std::invalid_argument("Explicit step duration and time increment must be finite and positive");

    const auto size = problem.lumpedMass.size();
    if (size == 0 || problem.nodalForces.size() != size || problem.prescribedDisplacements.size() != size)
        throw std::invalid_argument("Explicit dynamics problem dimensions do not match");
    validateTimeDependentLoads(problem);

    ExplicitDynamicsResult result{linalg::VectorX(size), linalg::VectorX(size), linalg::VectorX(size), linalg::VectorX(size), 0.0};
    std::vector<std::size_t> freeDofs;
    for (std::size_t dof = 0; dof < size; ++dof) {
        const auto prescribed = problem.prescribedDisplacements[dof];
        const double mass = problem.lumpedMass[dof];
        if (!std::isfinite(mass) || mass < 0.0 || (mass == 0.0 && !prescribed) || !std::isfinite(problem.nodalForces[dof]) ||
            (prescribed && !std::isfinite(*prescribed)))
            throw std::invalid_argument("Invalid mass, load or prescribed displacement at global DOF " + std::to_string(dof));
        if (prescribed)
            result.displacements[dof] = *prescribed;
        else
            freeDofs.push_back(dof);
    }

    const double stabilityLimit = stableTimeIncrement(problem);
    const double increment = std::min(step.duration, step.timeIncrement.value_or(0.9 * stabilityLimit));
    if (increment >= stabilityLimit) {
        std::ostringstream message;
        message << "Requested time increment " << increment << " must be below the conservative explicit stability limit " << stabilityLimit;
        throw std::invalid_argument(message.str());
    }

    linalg::VectorX internalForces(size);
    auto appliedForces = problem.nodalForces;
    const auto updateAccelerations = [&](double time) {
        if (!problem.timeDependentLoads.empty()) {
            appliedForces = problem.nodalForces;
            for (const auto &load : problem.timeDependentLoads) {
                const double scale = evaluateSmoothStep(load.amplitude, time);
                for (std::size_t dof = 0; dof < size; ++dof)
                    appliedForces[dof] += scale * load.magnitudes[dof];
            }
        }
        assembleInternalForces(problem, result.displacements, internalForces);
        for (const auto dof : freeDofs) {
            const double acceleration = (appliedForces[dof] - internalForces[dof]) / problem.lumpedMass[dof];
            if (!std::isfinite(acceleration))
                throw std::overflow_error("Non-finite acceleration in explicit dynamics");
            result.accelerations[dof] = acceleration;
        }
    };

    updateAccelerations(0.0);
    for (std::size_t stepNumber = 1; result.time < step.duration; ++stepNumber) {
        const double nextTime = std::min(static_cast<double>(stepNumber) * increment, step.duration);
        const double dt = nextTime - result.time;
        if (dt <= 0.0)
            throw std::invalid_argument("Time increment is too small to advance the explicit step");

        for (const auto dof : freeDofs) {
            result.velocities[dof] += 0.5 * dt * result.accelerations[dof];  // 半步速度
            result.displacements[dof] += dt * result.velocities[dof];
        }
        updateAccelerations(nextTime);
        for (const auto dof : freeDofs) {
            result.velocities[dof] += 0.5 * dt * result.accelerations[dof];  // 整步速度
            if (!std::isfinite(result.velocities[dof]) || !std::isfinite(result.displacements[dof]))
                throw std::overflow_error("Non-finite displacement or velocity in explicit dynamics");
        }
        result.time = nextTime;
    }

    for (std::size_t dof = 0; dof < size; ++dof) {
        const double velocity = result.velocities[dof];
        result.kineticEnergy += (0.5 * problem.lumpedMass[dof] * velocity) * velocity;
        result.strainEnergy += (0.5 * result.displacements[dof]) * internalForces[dof];
        if (problem.prescribedDisplacements[dof]) {
            result.reactions[dof] = internalForces[dof] - appliedForces[dof];
            if (!std::isfinite(result.reactions[dof]))
                throw std::overflow_error("Non-finite reaction in explicit dynamics");
        }
    }
    if (!std::isfinite(result.kineticEnergy) || !std::isfinite(result.strainEnergy))
        throw std::overflow_error("Non-finite energy in explicit dynamics");
    return result;
}
