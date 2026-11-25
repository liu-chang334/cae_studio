#include "AbaqusInpReader.h"
#include "AnalysisProblemBuilder.h"
#include "C3D8Element.h"
#include "ExplicitDynamicsSolver.h"
#include "LinearStaticSolver.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expectNear(double value, double expected, double tolerance, const std::string &label) {
    if (!std::isfinite(value) || std::abs(value - expected) > tolerance)
        throw std::runtime_error(label + ": expected " + std::to_string(expected) + ", got " + std::to_string(value));
}

void checkTimeIntegration() {
    linalg::MatrixX stiffness(2, 2);
    stiffness(0, 0) = 4.0;
    stiffness(0, 1) = stiffness(1, 0) = 2.0;
    stiffness(1, 1) = 6.0;
    const ExplicitDynamicsProblem problem{{2.0, 0.0}, {10.0, 1.0}, {std::nullopt, 0.5}, {{{0, 1}, stiffness}}};

    const ExplicitDynamicsSolver solver;
    const auto result = solver.solve(problem, {0.2, 0.2});
    expectNear(result.displacements[0], 0.09, 1e-12, "First-step displacement");
    expectNear(result.velocities[0], 0.882, 1e-12, "First-step velocity");
    expectNear(result.accelerations[0], 4.32, 1e-12, "First-step acceleration");
    expectNear(result.displacements[1], 0.5, 0.0, "Prescribed displacement");
    expectNear(result.velocities[1], 0.0, 0.0, "Prescribed velocity");
    expectNear(result.accelerations[1], 0.0, 0.0, "Prescribed acceleration");
    expectNear(result.reactions[1], 2.18, 1e-12, "Reaction");

    const auto shortened = solver.solve(problem, {0.3, 0.2}, 8);
    expectNear(shortened.time, 0.3, 0.0, "Final time with shortened increment");
    expectNear(shortened.displacements[0], 0.1998, 1e-12, "Shortened final increment");
    if (shortened.frames.size() != 3 || shortened.frames.front().displacements != linalg::VectorX{0.0, 0.5} ||
        shortened.frames.back().displacements != shortened.displacements)
        throw std::runtime_error("Displacement history must contain distinct initial, intermediate and final states");
    expectNear(shortened.frames[1].time, 0.2, 0.0, "Intermediate frame time");
    expectNear(shortened.frames[1].displacements[0], 0.09, 1e-12, "Intermediate frame displacement");

    try {
        solver.solve(problem, {2.0, 1.5});
        throw std::runtime_error("Unstable increment was accepted");
    } catch (const std::invalid_argument &) {
        // 该系统的稳定上限为 2 / sqrt(4 / 2)。
    }
}

void checkSmoothLoading() {
    const ExplicitDynamicsProblem problem{{2.0, 0.0}, {4.0, 1.0}, {std::nullopt, 0.0}, {},
                                          {{{{0.2, 0.0}, {0.6, 1.0}, {1.0, 0.25}}, {6.0, 3.0}}}};
    const ExplicitDynamicsSolver solver;
    for (const auto &[time, amplitude] : SmoothStepAmplitude{{0.1, 0.0}, {0.3, 0.103515625}, {0.6, 1.0},
                                                           {0.8, 0.625}, {1.2, 0.25}}) {
        const auto result = solver.solve(problem, {time, time});
        const double finalAcceleration = 2.0 + 3.0 * amplitude;
        expectNear(result.accelerations[0], finalAcceleration, 1e-12, "Acceleration at current load amplitude");
        expectNear(result.displacements[0], time * time, 1e-12, "Displacement from initial load amplitude");
        expectNear(result.velocities[0], 0.5 * time * (2.0 + finalAcceleration), 1e-12, "Velocity from both endpoint loads");
        expectNear(result.reactions[1], -1.0 - 3.0 * amplitude, 1e-12, "Reaction at current load amplitude");
    }
}

void checkElementAssembly() {
    FEModel model;
    model.analysisType = AnalysisType::ExplicitDynamics;
    model.mesh.nodes = {{30, {0.0, 0.0, 0.0}}, {10, {1.0, 0.0, 0.0}}, {90, {1.0, 1.0, 0.0}}, {70, {0.0, 1.0, 0.0}},
                        {80, {0.0, 0.0, 1.0}}, {20, {1.0, 0.0, 1.0}}, {50, {1.0, 1.0, 1.3}}, {40, {0.0, 1.0, 1.0}}};
    model.mesh.elements.push_back({12, {30, 10, 90, 70, 80, 20, 50, 40}, CellTopology::HEXAHEDRON, ElementFormulation::C3D8});
    constexpr double density = 6.0;
    model.materials.push_back({"Solid", IsotropicElasticData{100.0, 0.25}, std::nullopt, density});
    model.materialNameByElementId.emplace(12, "Solid");
    for (const auto &node : model.mesh.nodes) {
        for (int dof = 1; dof <= 3; ++dof)
            model.boundaries.push_back({node.id, dof, 0.001 * node.id * dof});
    }

    const AnalysisProblemBuilder builder(model);
    const auto problem = builder.buildExplicitDynamicsProblem();
    const auto &element = dynamic_cast<const C3D8Element &>(builder.element(1));
    for (std::size_t direction = 0; direction < 3; ++direction) {
        double mass = 0.0;
        for (std::size_t dof = direction; dof < problem.lumpedMass.size(); dof += 3)
            mass += problem.lumpedMass[dof];
        expectNear(mass, density * element.volume(), 1e-12, "Total mass per direction");
    }
    if (std::abs(problem.lumpedMass[0] - problem.lumpedMass[18]) < 1e-6)
        throw std::runtime_error("Distorted element nodal masses were unexpectedly uniform");

    const auto result = ExplicitDynamicsSolver{}.solve(problem, {0.001, std::nullopt});
    model.analysisType = AnalysisType::Static;
    const auto linearSystem = AnalysisProblemBuilder(model).buildLinearStaticSystem();
    for (std::size_t i = 0; i < result.reactions.size(); ++i) {
        double expected = 0.0;
        for (std::size_t j = 0; j < result.displacements.size(); ++j)
            expected += linearSystem.originalCoefficientMatrix(i, j) * result.displacements[j];
        expectNear(result.reactions[i], expected, 1e-12, "Assembled element internal force");
    }
}

void checkJob(const char *path) {
    const auto read = AbaqusInpReader{}.read(path);
    if (!read.success())
        throw std::runtime_error(read.error);
    const auto &model = read.model;
    if (model.analysisType != AnalysisType::ExplicitDynamics || !model.explicitDynamicsStep || model.materials.size() != 2 ||
        model.materials[0].density != 7800.0 || model.materials[1].density != 7800.0)
        throw std::runtime_error("Unexpected explicit step or material density in job1");
    const auto &step = *model.explicitDynamicsStep;
    if (model.loads.size() != 1 || model.loads[0].amplitudeName != "QUASISTATICRAMP" || step.timeIncrement)
        throw std::runtime_error("Unexpected smooth load or time increment in job1");
    const auto &amplitude = model.smoothStepAmplitudes.at(model.loads[0].amplitudeName);
    if (amplitude.size() != 2 || amplitude.front().time != 0.0 || amplitude.front().value != 0.0 ||
        amplitude.back().time != step.duration || amplitude.back().value != 1.0)
        throw std::runtime_error("job1 smooth loading must ramp from zero to the full load over the step");

    const AnalysisProblemBuilder builder(model);
    auto problem = builder.buildExplicitDynamicsProblem();
    if (problem.elements.size() != 625 || problem.lumpedMass.size() != 2808 || problem.timeDependentLoads.size() != 1)
        throw std::runtime_error("Unexpected explicit problem dimensions in job1");
    const ExplicitDynamicsSolver solver;
    const auto result = solver.solve(problem, step, 120);
    expectNear(result.time, step.duration, 0.0, "job1 final time");
    if (result.frames.size() != 121 || result.frames.front().time != 0.0 || result.frames.back().time != step.duration ||
        result.frames.back().displacements != result.displacements)
        throw std::runtime_error("job1 history did not include the initial state, sampled frames and final state");
    if (result.elasticFrames.size() != result.frames.size())
        throw std::runtime_error("job1 material snapshots do not match the number of displacement frames");
    for (std::size_t i = 0; i < result.frames.size(); ++i) {
        const auto &frame = result.elasticFrames[i];
        if (frame.time != result.frames[i].time || frame.integrationPoints.size() != model.mesh.elements.size() ||
            frame.nodes.size() != model.mesh.nodes.size() || frame.integrationPoints.front().size() != 8)
            throw std::runtime_error("job1 material snapshot does not match the mesh or displacement frame");
    }
    const auto &initialStress = result.elasticFrames.front().integrationPoints.front().front().stress;
    const auto &finalStress = result.elasticFrames.back().integrationPoints.front().front().stress;
    for (const double value : initialStress)
        expectNear(value, 0.0, 0.0, "Initial integration point stress");
    if (initialStress == finalStress)
        throw std::runtime_error("job1 material state did not change under load");
    for (std::size_t i = 1; i < result.frames.size(); ++i) {
        if (result.frames[i].time <= result.frames[i - 1].time)
            throw std::runtime_error("job1 history frame times must be strictly increasing");
    }
    for (const auto nodeId : model.nodeSets.at("nSet-2")) {
        for (int direction = 1; direction <= 3; ++direction)
            expectNear(result.displacements[builder.globalDofIndex(nodeId, direction)], 0.0, 0.0, "Fixed support");
    }
    const auto dof = builder.globalDofIndex(936, 2);

    auto staticModel = model;
    staticModel.analysisType = AnalysisType::Static;
    staticModel.loads[0].amplitudeName.clear();
    const auto staticSystem = AnalysisProblemBuilder(staticModel).buildLinearStaticSystem();
    LinearStaticSolver staticSolver;
    const auto &staticDisplacements = staticSolver.solve(staticSystem).displacements;
    double displacementNormSquared = 0.0;
    double errorNormSquared = 0.0;
    for (std::size_t i = 0; i < staticDisplacements.size(); ++i) {
        const double difference = result.displacements[i] - staticDisplacements[i];
        errorNormSquared += difference * difference;
        displacementNormSquared += staticDisplacements[i] * staticDisplacements[i];
    }
    const double displacementError = std::sqrt(errorNormSquared / displacementNormSquared);
    const double energyRatio = result.kineticEnergy / result.strainEnergy;
    std::cout << "job1 smooth loading: relative displacement error = " << displacementError << ", KE/strain energy = " << energyRatio
              << ", node 936 U2 = " << result.displacements[dof] << " (static " << staticDisplacements[dof] << ")\n";
    expectNear(displacementError, 0.0, 0.002, "job1 quasi-static displacement relative error");
    expectNear(energyRatio, 0.0, 1e-4, "job1 quasi-static energy ratio");

    problem.nodalForces = staticSystem.originalRhs;
    problem.timeDependentLoads.clear();
    // 独立装配集中质量和刚度后的阶跃载荷模态解，取 t = 0.005 时刻。
    constexpr double referenceU2 = -3.564049727988519e-5;
    const auto transient = solver.solve(problem, {0.005, 0.00002});
    if (transient.elasticFrames.size() != 1 || transient.elasticFrames.front().time != transient.time)
        throw std::runtime_error("job1 did not retain the final material state without history output");
    expectNear(transient.displacements[dof], referenceU2, 6e-9, "job1 transient displacement against modal solution");

    const auto refined = solver.solve(problem, {0.005, 0.00001});
    const double coarseError = std::abs(transient.displacements[dof] - referenceU2);
    const double refinedError = std::abs(refined.displacements[dof] - referenceU2);
    if (!std::isfinite(refinedError) || refinedError > coarseError / 3.5)
        throw std::runtime_error("job1 transient displacement did not show second-order convergence after halving the increment");
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ExplicitDynamicsSmoke <path-to-explicit-job1.inp>\n";
        return 1;
    }
    try {
        checkTimeIntegration();
        checkSmoothLoading();
        checkElementAssembly();
        checkJob(argv[1]);
        std::cout << "Explicit dynamics smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Explicit dynamics smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
