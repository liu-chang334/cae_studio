#include "AbaqusInpReader.h"
#include "AnalysisProblemBuilder.h"
#include "C3D8Element.h"
#include "LinearElastic.h"
#include "LinearStaticSolver.h"
#include "MaterialData.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void checkElement(const AnalysisProblemBuilder &analysis, std::size_t elementNumber, MeshId expectedSourceId,
                  const IsotropicElasticData &expectedElastic) {
    const std::string label = "Element " + std::to_string(elementNumber);

    if (analysis.sourceElementId(elementNumber) != expectedSourceId)
        throw std::runtime_error(label + " has an incorrect source ID");

    const auto *element = dynamic_cast<const C3D8Element *>(&analysis.element(elementNumber));
    if (!element)
        throw std::runtime_error(label + " is not a C3D8 element");

    const auto referenceMaterial = std::make_shared<const LinearElasticMaterial>(expectedElastic);
    const C3D8Element reference(element->nodes(), referenceMaterial);

    const auto actual = element->computeElasticStiffness();
    const auto expected = reference.computeElasticStiffness();

    for (std::size_t row = 0; row < actual.rows(); ++row) {
        for (std::size_t col = 0; col < actual.cols(); ++col) {
            const double expectedValue = expected(row, col);
            const double tolerance = 1e-6 + 1e-10 * std::abs(expectedValue);
            const double actualValue = actual(row, col);

            if (!std::isfinite(actualValue) || std::abs(actualValue - expectedValue) > tolerance) {
                throw std::runtime_error(label + " stiffness mismatch at [" + std::to_string(row) + "][" + std::to_string(col) + "]");
            }
        }
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: AnalysisModelSmoke <path-to-job1.inp>\n";
        return 1;
    }

    try {
        ConstrainedLinearSystem smallSystem{linalg::MatrixX(3, 3), {10.0, 1.0, 7.0}, {std::nullopt, 2.0, std::nullopt}};
        auto &A = smallSystem.originalCoefficientMatrix;
        A(0, 0) = 4.0;
        A(1, 1) = 8.0;
        A(2, 2) = 5.0;
        A(0, 1) = A(1, 0) = -3.0;
        A(0, 2) = A(2, 0) = 1.0;
        A(1, 2) = A(2, 1) = 2.0;
        LinearStaticSolver smallSolver;
        const auto &solution = smallSolver.solve(smallSystem);
        const auto &u = solution.displacements;
        const auto &R = solution.reactions;
        constexpr double smallTol = 1e-12;
        if (!(std::abs(u[0] - 77.0 / 19.0) <= smallTol && u[1] == 2.0 && std::abs(u[2] + 4.0 / 19.0) <= smallTol &&
              std::abs(R[1] - 46.0 / 19.0) <= smallTol)) {
            throw std::runtime_error("Unexpected linear static displacement or reaction");
        }

        LinearStaticSolver penaltySolver;
        const auto &penaltySolution = penaltySolver.solve(
            smallSystem, LinearStaticAlgorithm::Cholesky, LinearStaticConstraintMethod::Penalty);
        constexpr double penaltyTolerance = 1e-7;
        for (std::size_t dof = 0; dof < u.size(); ++dof) {
            if (!(std::abs(penaltySolution.displacements[dof] - u[dof]) < penaltyTolerance &&
                  std::abs(penaltySolution.reactions[dof] - R[dof]) < penaltyTolerance)) {
                throw std::runtime_error("Penalty solution differs from the reduced solution");
            }
        }
        if (!(std::abs(penaltySolution.displacements[1] - u[1]) > smallTol)) {
            throw std::runtime_error("Penalty solution unexpectedly imposed the displacement exactly");
        }

        const auto result = AbaqusInpReader{}.read(argv[1]);
        if (!result.success())
            throw std::runtime_error(result.error);

        const AnalysisProblemBuilder analysis(result.model);
        if (analysis.elementCount() != 625)
            throw std::runtime_error("Expected 625 elements, got " + std::to_string(analysis.elementCount()));
        if (analysis.globalDofCount() != 2808 || analysis.globalDofIndex(1, 1) != 0 || analysis.globalDofIndex(936, 3) != 2807) {
            throw std::runtime_error("Unexpected global DOF numbering");
        }

        FEModel sparseModel;
        sparseModel.mesh.nodes = {
            {30, {0.0, 0.0, 0.0}},
            {10, {1.0, 0.0, 0.0}},
        };

        const AnalysisProblemBuilder sparseAnalysis(sparseModel);
        if (sparseAnalysis.globalDofCount() != 6 || sparseAnalysis.globalDofIndex(30, 2) != 1 ||
            sparseAnalysis.globalDofIndex(10, 3) != 5) {
            throw std::runtime_error("Unexpected DOF numbering for nonconsecutive node IDs");
        }
        checkElement(analysis, 300, 300, {2.1e9, 0.003});  // Mat-1
        checkElement(analysis, 301, 301, {3.2e9, 0.002});  // Mat-2
        const auto system = analysis.buildLinearStaticSystem();
        const auto &K = system.originalCoefficientMatrix;
        if (K.rows() != 2808 || K.cols() != 2808)
            throw std::runtime_error("Unexpected global stiffness dimensions");

        const auto &e1 = dynamic_cast<const C3D8Element &>(analysis.element(1));
        const auto &e2 = dynamic_cast<const C3D8Element &>(analysis.element(2));
        const auto g = analysis.globalDofIndex(2, 1);
        const double expected = e1.computeElasticStiffness()(15, 15) + e2.computeElasticStiffness()(12, 12);

        const double actual = K(g, g);
        const double tolerance = 1e-6 + 1e-10 * std::abs(expected);
        if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
            throw std::runtime_error("Shared node stiffness was not assembled correctly");

        const auto &forces = system.originalRhs;
        double appliedY = 0.0;
        for (std::size_t dof = 1; dof < forces.size(); dof += 3)
            appliedY += forces[dof];

        LinearStaticSolver jobSolver;
        for (const auto method : {LinearStaticConstraintMethod::Reduction, LinearStaticConstraintMethod::Penalty}) {
            const auto &jobSolution = jobSolver.solve(system, LinearStaticAlgorithm::Cholesky, method);
            double reactionY = 0.0;
            for (std::size_t dof = 1; dof < forces.size(); dof += 3)
                reactionY += jobSolution.reactions[dof];

            if (appliedY != -78000.0 || !(std::abs(appliedY + reactionY) <= 1e-3)) {
                throw std::runtime_error("job1: y-direction force balance failed (applied = " + std::to_string(appliedY) +
                                         ", reaction = " + std::to_string(reactionY) + ")");
            }
        }

        std::cout << "AnalysisModel smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AnalysisModel smoke test failed: " << error.what() << '\n';
        return 1;
    }
}

///   .\AnalysisModelSmoke.exe ..\..\..\inp\job1.inp
