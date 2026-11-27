#include "ElementStatesManager.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

ElementStatesManager::ElementStatesManager(const std::vector<AnalysisElement> &elements, std::size_t dofCount)
    : elements_(elements), dofCount_(dofCount) {
    if (!elements.empty() && dofCount % 3 != 0)
        throw std::invalid_argument("Solid element states require 3 DOFs per node");
    states_.reserve(elements.size());
    for (const auto &entry : elements) {
        if (!entry.element || entry.globalDofs.size() != entry.element->dofCount())
            throw std::invalid_argument("Invalid element state DOF map");
        for (std::size_t i = 0; i < entry.globalDofs.size(); i += 3) {
            const auto dof = entry.globalDofs[i];
            if (dof >= dofCount || dof % 3 != 0 || entry.globalDofs[i + 1] != dof + 1 || entry.globalDofs[i + 2] != dof + 2)
                throw std::invalid_argument("Element state DOFs must map to consecutive node components");
        }
        states_.push_back(entry.element->createState());
    }
}

void ElementStatesManager::updateTrial(const linalg::VectorX &displacements, double timeIncrement) {
    if (displacements.size() != dofCount_ || !std::isfinite(timeIncrement) || timeIncrement < 0.0)
        throw std::invalid_argument("Invalid displacement vector or time increment for material state");
    for (std::size_t i = 0; i < elements_.size(); ++i) {
        const auto &entry = elements_[i];  // 取分析用单元实例
        localDisplacements_.resize(entry.globalDofs.size());  // 单元节点位移向量容器
        for (std::size_t j = 0; j < entry.globalDofs.size(); ++j)
            localDisplacements_[j] = displacements[entry.globalDofs[j]];  // 从全局位移中提取当前单元结果
        try {
            entry.element->updateTrialState(localDisplacements_, timeIncrement, states_[i]);
        } catch (const std::logic_error &error) {
            if (!entry.sourceId)
                throw;
            throw std::runtime_error("Element " + std::to_string(*entry.sourceId) + ": " + error.what());
        }
    }
}

void ElementStatesManager::commit() {
    for (auto &state : states_)
        state.commit();
}

void ElementStatesManager::revert() {
    for (auto &state : states_)
        state.revert();
}

void ElementStatesManager::assemble(linalg::VectorX &internalForces, linalg::MatrixX &tangent) const {
    internalForces.assign(dofCount_, 0.0);
    tangent = linalg::MatrixX(dofCount_, dofCount_);
    for (std::size_t e = 0; e < elements_.size(); ++e) {
        const auto &entry = elements_[e];
        linalg::VectorX force;
        linalg::MatrixX stiffness(0, 0);
        entry.element->computeTrialResponse(states_[e], force, stiffness);
        const auto &dofs = entry.globalDofs;
        if (force.size() != dofs.size() || stiffness.rows() != dofs.size() || stiffness.cols() != dofs.size())
            throw std::logic_error("Invalid nonlinear element response dimensions");
        for (std::size_t i = 0; i < dofs.size(); ++i) {
            if (!std::isfinite(force[i]))
                throw std::domain_error("Non-finite element internal force");
            internalForces[dofs[i]] += force[i];
            for (std::size_t j = 0; j < dofs.size(); ++j) {
                if (!std::isfinite(stiffness(i, j)))
                    throw std::domain_error("Non-finite element tangent");
                tangent(dofs[i], dofs[j]) += stiffness(i, j);
            }
        }
    }
}

ElasticResultFrame ElementStatesManager::snapshot(double time) const {
    ElasticResultFrame frame;
    frame.time = time;
    frame.nodes.resize(dofCount_ / 3);
    frame.integrationPoints.reserve(states_.size());
    std::vector<std::size_t> contributions(frame.nodes.size());
    for (std::size_t i = 0; i < states_.size(); ++i) {
        auto &points = frame.integrationPoints.emplace_back();
        const auto &entry = elements_[i];
        points.reserve(states_[i].committed.size());
        for (const auto &state : states_[i].committed)
            points.push_back(state.fields);
        const auto nodal = entry.element->extrapolateNodalStates(points);
        if (nodal.size() != entry.element->nodeCount())
            throw std::logic_error("Extrapolated states do not match element connectivity");
        for (std::size_t local = 0; local < nodal.size(); ++local) {
            const auto node = entry.globalDofs[3 * local] / 3;
            const auto &state = nodal[local];
            for (std::size_t component = 0; component < state.strain.size(); ++component) {
                frame.nodes[node][component] += state.strain[component];
                frame.nodes[node][state.strain.size() + component] += state.stress[component];
            }
            ++contributions[node];
        }
    }
    for (std::size_t node = 0; node < frame.nodes.size(); ++node) {
        if (contributions[node] == 0) {
            frame.nodes[node].fill(std::numeric_limits<double>::quiet_NaN());
            continue;
        }
        for (auto &value : frame.nodes[node]) {
            value /= static_cast<double>(contributions[node]);
            if (!std::isfinite(value))
                throw std::domain_error("Non-finite averaged nodal state");
        }
    }
    return frame;
}
