#include "AbaqusInpReader.h"
#include "AnalysisProblemBuilder.h"
#include "MisesPlasticity.h"
#include "C3D8Element.h"
#include "LinearStaticSolver.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void expect(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance, const char *message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(std::string(message) + ": " + std::to_string(actual) + " vs " + std::to_string(expected));
}

// 光滑、正切线的非线性本构，用于迫使大增量失败并验证减步长恢复。
class CubicMaterial : public Material3D {
public:
    CubicMaterial() : elastic_(computeIsotropicElasticMatrix({1000, 0})) {}
    const linalg::Matrix<6, 6> &initialElasticMatrix() const override { return elastic_; }
    void integrate(const MaterialPointState &, const linalg::Vector<6> &strain, double,
                   MaterialPointState &trial) const override {
        trial = {};
        trial.fields.strain = strain;
        trial.tangent = elastic_;
        for (std::size_t i = 0; i < 6; ++i) {
            trial.fields.stress[i] = elastic_[i][i] * strain[i];
            if (i < 3) {
                trial.fields.stress[i] += 1e8 * strain[i] * strain[i] * strain[i];
                trial.tangent[i][i] += 3e8 * strain[i] * strain[i];
            }
        }
    }
private:
    linalg::Matrix<6, 6> elastic_;
};

void materialTest() {
    MisesPlasticityMaterial material({210000, 0.3}, {{{250, 0}, {1250, 0.1}}});
    MaterialPointState committed, trial, repeated;
    linalg::Vector<6> strain{0.004, -0.001, -0.0005, 0.002, -0.001, 0.0007};
    material.integrate(committed, strain, 0.1, trial);
    material.integrate(committed, strain, 0.1, repeated);
    expect(trial.internalVariables == repeated.internalVariables && committed.internalVariables.empty(),
           "Repeated Newton trial changed committed history");
    for (std::size_t col = 0; col < 6; ++col) {
        auto plus = strain, minus = strain;
        plus[col] += 1e-8;
        minus[col] -= 1e-8;
        MaterialPointState a, b;
        material.integrate(committed, plus, 0.1, a);
        material.integrate(committed, minus, 0.1, b);
        for (std::size_t row = 0; row < 6; ++row)
            near(trial.tangent[row][col], (a.fields.stress[row] - b.fields.stress[row]) / 2e-8,
                 1e-3, "Inconsistent plastic algorithmic tangent");
    }
    MisesPlasticityMaterial piecewise({210000, 0.3}, {{{250, 0}, {260, 0.001}, {270, 0.003}}});
    MaterialPointState large;
    piecewise.integrate({}, {0, 0, 0, 0.05, 0, 0}, 1, large);
    near(std::sqrt(3.0) * large.fields.stress[3], 270, 1e-8, "Hardening plateau return failed");
}

int main(int argc, char **argv) {
    try {
        expect(argc == 2, "Expected cube input path");
        materialTest();
        const auto read = AbaqusInpReader{}.read(argv[1]);
        expect(read.success(), read.error.c_str());
        // builder 和 problem 销毁后，context 应继续保持单元定义有效。
        auto context = [&] {
            const AnalysisProblemBuilder builder(read.model);
            const auto problem = builder.buildNonlinearStaticProblem();
            return AnalysisContext(problem);
        }();
        const AnalysisProblemBuilder builder(read.model);
        const auto problem = builder.buildNonlinearStaticProblem();
        bool linearRejected = false;
        try { builder.buildLinearStaticSystem(); } catch (const std::invalid_argument &) { linearRejected = true; }
        expect(linearRejected, "Linear solver silently accepted plasticity");
        NonlinearStaticSolver solver;
        const auto loaded = solver.solve(context, problem.loads, problem.prescribedDisplacements);
        const double plasticStrain = (260.0 - 250.0) / 10000.0;
        const double loadedDisplacement = 260.0 / 210000.0 + plasticStrain;
        near(context.displacements[3], loadedDisplacement, 1e-9, "Plastic cube axial displacement");
        near(loaded.materialFrames.back().integrationPoints[0][0].stress[0], 260, 1e-6, "Axial stress");
        double reaction = 0;
        for (std::size_t i = 0; i < loaded.reactions.size(); i += 3) reaction += loaded.reactions[i];
        near(reaction, -260, 1e-6, "Reaction equilibrium");
        near(context.time, 1, 1e-12, "Loading pseudo time");
        const auto unloaded = solver.solve(context, linalg::VectorX(24, 0.0), problem.prescribedDisplacements);
        near(context.displacements[3], plasticStrain, 1e-9, "Unloading lost plastic history");
        near(unloaded.materialFrames.back().integrationPoints[0][0].stress[0], 0, 1e-6, "Unloaded stress");
        near(context.time, 2, 1e-12, "Continuation pseudo time");
        for (std::size_t i = 1; i < unloaded.frames.size(); ++i)
            expect(unloaded.frames[i].time > unloaded.frames[i - 1].time, "Continuation frame times not increasing");
        // 线弹性通过同一增量流程，应与全量线性求解一致。
        auto elasticModel = read.model;
        elasticModel.materials[0].plasticity.reset();
        AnalysisProblemBuilder elasticBuilder(elasticModel);
        const auto elasticProblem = elasticBuilder.buildNonlinearStaticProblem();
        AnalysisContext elasticContext(elasticProblem);
        solver.solve(elasticContext, elasticProblem.loads, elasticProblem.prescribedDisplacements);
        LinearStaticSolver linear;
        const auto &linearResult = linear.solve(elasticBuilder.buildLinearStaticSystem());
        for (std::size_t i = 0; i < 24; ++i)
            near(elasticContext.displacements[i], linearResult.displacements[i], 1e-10, "Linear regression mismatch");
        // 全指定位移加载，也必须积分材料并产生正确反力。
        auto prescribedProblem = problem;
        prescribedProblem.loads.assign(24, 0.0);
        for (std::size_t i = 0; i < 24; ++i) prescribedProblem.prescribedDisplacements[i] = loaded.frames.back().displacements[i];
        AnalysisContext prescribedContext(prescribedProblem);
        const auto prescribed = solver.solve(prescribedContext, prescribedProblem.loads, prescribedProblem.prescribedDisplacements);
        near(prescribed.materialFrames.back().integrationPoints[0][0].stress[0], 260, 1e-6,
             "Prescribed displacement material update failed");
        auto failed = AnalysisContext(problem);
        NonlinearStaticOptions options;
        options.initialIncrement = 1;
        options.minimumIncrement = 1;
        options.maxIterations = 1;
        bool rejected = false;
        try { solver.solve(failed, problem.loads, problem.prescribedDisplacements, options); }
        catch (const std::runtime_error &) { rejected = true; }
        expect(rejected, "Expected nonlinear convergence failure");
        near(failed.time, 0, 0, "Failed increment advanced time");
        near(failed.displacements[3], 0, 0, "Failed increment changed displacements");
        solver.solve(failed, problem.loads, problem.prescribedDisplacements);
        near(failed.displacements[3], loadedDisplacement, 1e-9, "Failed trial contaminated committed history");
        auto cubicProblem = problem;
        cubicProblem.elements[0].element = std::make_shared<C3D8Element>(read.model.mesh.nodes, std::make_shared<CubicMaterial>());
        auto cutback = AnalysisContext(cubicProblem);
        options.minimumIncrement = 1e-5;
        options.maxIterations = 5;
        const auto adaptive = solver.solve(cutback, cubicProblem.loads, cubicProblem.prescribedDisplacements, options);
        expect(adaptive.cutbacks > 0, "Expected automatic increment cutback");
        const double u = cutback.displacements[3];
        near(1000 * u + 1e8 * u * u * u, 260, 1e-5, "Cutback did not reach equilibrium");
        near(cutback.time, 1, 1e-12, "Cutback did not complete loading");
        std::cout << "Nonlinear static smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
