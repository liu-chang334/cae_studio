#include "ElementStates.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

ElementStates::ElementStates(const std::vector<AnalysisElement> &elements, std::size_t dofCount)
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

void ElementStates::updateTrial(const linalg::VectorX &displacements, double timeIncrement) {
    if (displacements.size() != dofCount_ || !std::isfinite(timeIncrement) || timeIncrement < 0.0)
        throw std::invalid_argument("Invalid displacement vector or time increment for material state");
    for (std::size_t i = 0; i < elements_.size(); ++i) {
        const auto &entry = elements_[i];
        localDisplacements_.resize(entry.globalDofs.size());
        for (std::size_t j = 0; j < entry.globalDofs.size(); ++j)
            localDisplacements_[j] = displacements[entry.globalDofs[j]];
        try {
            entry.element->updateTrialState(localDisplacements_, timeIncrement, states_[i]);
        } catch (const std::logic_error &error) {
            if (!entry.sourceId)
                throw;
            throw std::runtime_error("Element " + std::to_string(*entry.sourceId) + ": " + error.what());
        }
    }
}

void ElementStates::commit() {
    for (auto &state : states_)
        state.commit();
}

ElasticResultFrame ElementStates::snapshot(double time) const {
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
