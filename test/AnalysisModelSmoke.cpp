#include "AbaqusInpReader.h"
#include "AnalysisModel.h"
#include "C3D8Element.h"
#include "LinearElastic.h"
#include "MaterialData.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void checkElement(const AnalysisModel &analysis, std::size_t elementNumber, MeshId expectedSourceId,
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

} // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: AnalysisModelSmoke <path-to-job1.inp>\n";
        return 1;
    }

    try {
        const auto result = AbaqusInpReader{}.read(argv[1]);
        if (!result.success())
            throw std::runtime_error(result.error);

        const AnalysisModel analysis(result.model);
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

        const AnalysisModel sparseAnalysis(sparseModel);
        if (sparseAnalysis.globalDofCount() != 6 || sparseAnalysis.globalDofIndex(30, 2) != 1 ||
            sparseAnalysis.globalDofIndex(10, 3) != 5) {
            throw std::runtime_error("Unexpected DOF numbering for nonconsecutive node IDs");
        }
        checkElement(analysis, 300, 300, {2.1e9, 0.003}); // Mat-1
        checkElement(analysis, 301, 301, {3.2e9, 0.002}); // Mat-2
        const auto K = analysis.assembleElasticStiffness();
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
        std::cout << "AnalysisModel smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "AnalysisModel smoke test failed: " << error.what() << '\n';
        return 1;
    }
}

///   .\AnalysisModelSmoke.exe ..\..\..\inp\job1.inp