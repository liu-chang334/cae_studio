#include "AnalysisProblemBuilder.h"
#include "DisplacementResultIO.h"
#include "ElasticResultIO.h"
#include "FEModel.h"
#include "LinearStaticSolver.h"
#include "vtkBuilder.h"
#include "vtkScene.h"

#include <QFile>
#include <QTemporaryDir>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkActor2DCollection.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkScalarsToColors.h>
#include <vtkTypeInt64Array.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

namespace {

void expect(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

void checkResults() {
    QTemporaryDir directory;
    expect(directory.isValid(), "Could not create result test directory");
    const QByteArray fingerprint = "test-fingerprint";
    const auto path = directory.filePath(QStringLiteral("job.displacement.csv"));

    vtkNew<vtkUnstructuredGrid> grid;
    vtkNew<vtkPoints> points;
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    grid->SetPoints(points);
    vtkNew<vtkTypeInt64Array> nodeIds;
    nodeIds->SetName("NodeIds");
    nodeIds->SetNumberOfComponents(1);
    nodeIds->InsertNextValue(7);
    nodeIds->InsertNextValue(42);
    grid->GetPointData()->AddArray(nodeIds);

    expect(vtkBuilder::setDisplacements(grid, {3.0, 4.0, 0.0, 0.0, 0.0, -2.0}), "Could not display the single-frame result");
    QString error;
    if (!DisplacementResultIO::save(path, grid, fingerprint, error))
        throw std::runtime_error("Could not write the single-frame result: " + error.toStdString());
    const auto legacy = DisplacementResultIO::load(path, grid, fingerprint);
    expect(legacy.success() && legacy.frames.size() == 1 &&
               legacy.frames[0].displacements == linalg::VectorX{3.0, 4.0, 0.0, 0.0, 0.0, -2.0},
           "The original single-frame result did not round-trip");

    MeshData mesh;
    mesh.nodes = {{42, {1.0, 0.0, 0.0}}, {7, {0.0, 0.0, 0.0}}};
    const std::vector<DisplacementFrame> frames{{0.0, {0, 0, 0, 0, 0, 0}},
                                                {0.25, {1, 2, 3, 4, 5, 6}},
                                                {0.5, {-1, -2, -3, -4, -5, -6}}};
    if (!DisplacementResultIO::saveHistory(path, mesh, frames, fingerprint, error))
        throw std::runtime_error("Could not write displacement history: " + error.toStdString());
    const auto history = DisplacementResultIO::load(path, grid, fingerprint);
    expect(history.success() && history.frames.size() == frames.size(), "Displacement history did not round-trip");
    for (std::size_t i = 0; i < history.frames.size(); ++i) {
        expect(history.frames[i].time == frames[i].time, "History frame time changed on disk");
        const auto &u = frames[i].displacements;
        expect(history.frames[i].displacements == linalg::VectorX{u[3], u[4], u[5], u[0], u[1], u[2]},
               "Displacements were not reordered by node ID");
    }
    auto *originalMagnitude = grid->GetPointData()->GetArray("DisplacementMagnitude");
    const auto originalTime = originalMagnitude->GetMTime();
    expect(vtkBuilder::setDisplacements(grid, history.frames[1].displacements), "Could not display the intermediate frame");
    expect(grid->GetPointData()->GetArray("DisplacementMagnitude") == originalMagnitude && originalMagnitude->GetMTime() > originalTime,
           "Changing frames replaced or did not update the contour array");
    expect(grid->GetPointData()->GetVectors()->GetComponent(0, 0) == 4.0 &&
               grid->GetPointData()->GetArray("DisplacementY")->GetComponent(1, 0) == 2.0,
           "The intermediate frame did not update the displayed displacements");
    vtkScene scene;
    scene.setMesh(grid, false);
    expect(scene.setContour("DisplacementX", "U1"), "Could not show frame displacement contour");
    auto *bars = scene.renderer()->GetActors2D();
    bars->InitTraversal();
    auto *bar = vtkScalarBarActor::SafeDownCast(bars->GetNextActor2D());
    expect(bar && !bar->GetUseCustomLabels(), "Expected a scalar bar for the nonconstant frame");
    const auto *range = bar->GetLookupTable()->GetRange();
    expect(range[0] == 1.0 && range[1] == 4.0, "Scalar bar did not use the current frame range");
    expect(vtkBuilder::setDisplacements(grid, history.frames[2].displacements) && scene.setContour("DisplacementX", "U1"),
           "Could not show the next displacement frame");
    range = bar->GetLookupTable()->GetRange();
    expect(range[0] == -4.0 && range[1] == -1.0, "Scalar bar did not update to the next frame limits");
    expect(vtkBuilder::setDisplacements(grid, history.frames[0].displacements) && scene.setContour("DisplacementX", "U1"),
           "Could not show constant initial displacement");
    expect(bar->GetUseCustomLabels() && bar->GetCustomLabels()->GetNumberOfTuples() == 1 &&
               bar->GetCustomLabels()->GetValue(0) == 0.0,
           "Scalar bar should display the exact value for the constant frame");
    expect(!DisplacementResultIO::load(path, grid, "different-fingerprint").success(), "History accepted a different INP fingerprint");

    auto invalid = frames;
    invalid.back().displacements[0] = std::numeric_limits<double>::infinity();
    expect(!DisplacementResultIO::saveHistory(path, mesh, invalid, fingerprint, error), "History saved an invalid displacement");
    expect(DisplacementResultIO::load(path, grid, fingerprint).success(), "A failed save replaced the previous results");

    QFile file(path);
    expect(file.open(QIODevice::ReadWrite), "Could not open history for a truncation test");
    const auto data = file.readAll();
    expect(file.resize(data.lastIndexOf('\n', data.size() - 2) + 1), "Could not truncate the last node record");
    file.close();
    expect(!DisplacementResultIO::load(path, grid, fingerprint).success(), "A truncated displacement history was accepted");
}

void checkNodalResults() {
    FEModel model;
    const std::array<std::array<double, 3>, 8> cube{{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                                    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};
    std::map<std::array<double, 3>, MeshId> idsByPosition;
    for (int block = 0; block < 2; ++block) {
        const MeshId id = 41 + 37 * block;
        MeshElement element{id, {}, CellTopology::HEXAHEDRON, ElementFormulation::C3D8};
        for (auto coordinates : cube) {
            coordinates[0] += block;
            const MeshId nextId = 1007 + 13 * static_cast<MeshId>(model.mesh.nodes.size());
            const auto [entry, inserted] = idsByPosition.try_emplace(coordinates, nextId);
            if (inserted)
                model.mesh.nodes.push_back({entry->second, coordinates});
            element.nodeIds.push_back(entry->second);
        }
        const auto material = "material-" + std::to_string(id);
        model.materials.push_back({material, IsotropicElasticData{1000.0 * (block + 1), 0.25}});
        model.materialNameByElementId.emplace(id, material);
        model.mesh.elements.push_back(std::move(element));
    }
    constexpr MeshId isolatedId = 999;
    model.mesh.nodes.push_back({isolatedId, {3.0, 0.0, 0.0}});
    const AnalysisProblemBuilder analysis(model);
    linalg::VectorX displacements(analysis.globalDofCount());
    for (const auto &node : model.mesh.nodes) {
        const auto &[x, y, z] = node.coordinates;
        const auto dof = analysis.globalDofIndex(node.id, 1);
        displacements[dof] = 0.1 + 0.01 * x + 0.04 * y - 0.05 * z;
        displacements[dof + 1] = -0.02 * y + 0.06 * z;
        displacements[dof + 2] = 0.03 * z + 0.01 * x * y;
    }
    const auto expectedFields = [](double x, double y) -> vtkBuilder::NodalFieldValues {
        return {0.01, -0.02, 0.03, 0.04, -0.05 + 0.01 * y, 0.06 + 0.01 * x,
                16.0, -8.0, 32.0, 16.0, -20.0 + 4.0 * y, 24.0 + 4.0 * x};
    };
    auto system = analysis.buildLinearStaticSystem();
    system.prescribedValues.assign(displacements.begin(), displacements.end());
    LinearStaticSolver solver;
    const auto solution = solver.solve(system);
    expect(solution.elasticFrame && solution.elasticFrame->integrationPoints.size() == 2 &&
           solution.elasticFrame->integrationPoints.front().size() == 8, "Static solver did not capture the C3D8 states");

    // 节点和单元都重排。共享面位于 x = 1，其应力是两侧材料外推值的平均。
    auto displayMesh = model.mesh;
    std::reverse(displayMesh.nodes.begin(), displayMesh.nodes.end());
    std::reverse(displayMesh.elements.begin(), displayMesh.elements.end());
    const auto result = vtkBuilder{}.buildResults(displayMesh, analysis, solution);
    const auto modelGrid = vtkBuilder{}.build(model.mesh);
    expect(result.success() && modelGrid.success(), "Could not build the nodal result and model grids");
    for (std::size_t component = 0; component < vtkBuilder::nodalFieldNames.size(); ++component) {
        const auto *name = vtkBuilder::nodalFieldNames[component];
        auto *field = result.grid->GetPointData()->GetArray(name);
        expect(field && field->GetNumberOfTuples() == result.grid->GetNumberOfPoints() && !result.grid->GetCellData()->GetArray(name),
               "Nodal result is missing or attached to cells");
        for (vtkIdType point = 0; point < field->GetNumberOfTuples(); ++point) {
            const auto &node = displayMesh.nodes[static_cast<std::size_t>(point)];
            const double value = field->GetComponent(point, 0);
            if (node.id == isolatedId) {
                expect(std::isnan(value), "Isolated node received an element result");
                continue;
            }
            const auto &[x, y, z] = node.coordinates;
            const double stressFactor = 1.0 + 0.5 * x;
            const double expected = (component < 6 ? 1.0 : stressFactor) * expectedFields(x, y)[component];
            expect(std::abs(value - expected) < 1e-10, "Incorrect extrapolated or shared nodal field");
        }
    }
    QTemporaryDir directory;
    expect(directory.isValid(), "Could not create element result test directory");
    const auto path = directory.filePath(QStringLiteral("job.displacement.csv"));
    const QByteArray fingerprint = "element-test-fingerprint";
    QString error;
    expect(DisplacementResultIO::save(path, model.mesh, solution.displacements, fingerprint, error),
           "Could not save static displacement from the solver");
    const auto single = DisplacementResultIO::load(path, modelGrid.grid, fingerprint);
    expect(single.success() && single.frames[0].displacements == displacements, "Could not reload static displacement");
    const auto paths = ElasticResultPaths::forDisplacement(path);
    const auto staticHash = DisplacementResultIO::fingerprint(path, error);
    if (!ElasticResultIO::save(paths, model.mesh, {*solution.elasticFrame}, staticHash, error))
        throw std::runtime_error("Could not save static elastic CSV: " + error.toStdString());
    const auto staticNodal = ElasticResultIO::loadNodal(paths, model.mesh, single.frames, staticHash, error);
    expect(staticNodal.size() == 1 && std::abs(staticNodal.front()[1][6] - 24.0) < 1e-10,
           "The static shared node did not round-trip");

    const std::vector<DisplacementFrame> frames{{0.0, linalg::VectorX(displacements.size())}, {0.2, displacements}};
    system.prescribedValues.assign(displacements.size(), 0.0);
    const auto &zero = solver.solve(system);
    expect(zero.elasticFrame.has_value(), "Static solver did not capture the unloaded state");
    std::vector<ElasticResultFrame> elasticFrames{*zero.elasticFrame, *solution.elasticFrame};
    elasticFrames[1].time = frames[1].time;
    expect(DisplacementResultIO::saveHistory(path, model.mesh, frames, fingerprint, error), "Could not save nodal test history");
    const auto history = DisplacementResultIO::load(path, modelGrid.grid, fingerprint);
    expect(history.success() && history.frames.size() == 2, "Could not reload nodal test history");
    const auto historyHash = DisplacementResultIO::fingerprint(path, error);
    expect(historyHash != staticHash && ElasticResultIO::loadNodal(paths, model.mesh, history.frames, historyHash, error).empty(),
           "A newer displacement run accepted stale elastic CSVs");
    if (!ElasticResultIO::save(paths, model.mesh, elasticFrames, historyHash, error))
        throw std::runtime_error("Could not write solver snapshots as elastic history: " + error.toStdString());
    const auto nodal = ElasticResultIO::loadNodal(paths, model.mesh, history.frames, historyHash, error);
    if (nodal.size() != history.frames.size())
        throw std::runtime_error("Could not reload elastic history: " + error.toStdString());
    expect(std::isnan(nodal[1].back().front()), "An isolated node did not round-trip as missing");
    for (std::size_t frameIndex = 0; frameIndex < history.frames.size(); ++frameIndex) {
        const auto &expected = elasticFrames[frameIndex];
        for (std::size_t cell = 0; cell < model.mesh.elements.size(); ++cell) {
            const auto states = ElasticResultIO::loadIntegrationPoints(paths, model.mesh.elements[cell],
                                                                       history.frames, frameIndex, historyHash, error);
            expect(states.size() == expected.integrationPoints[cell].size(), "The indexed frame did not return the element points");
            for (std::size_t gp = 0; gp < states.size(); ++gp) {
                expect(states[gp].strain == expected.integrationPoints[cell][gp].strain &&
                       states[gp].stress == expected.integrationPoints[cell][gp].stress, "Integration-point state changed on disk");
            }
        }
    }
    auto invalid = elasticFrames;
    invalid.back().integrationPoints.back().pop_back();
    expect(!ElasticResultIO::save(paths, model.mesh, invalid, historyHash, error), "Saved a partial element state");
    expect(ElasticResultIO::loadNodal(paths, model.mesh, history.frames, historyHash, error).size() == history.frames.size(),
           "Failed result export overwrote the valid cache");
    const auto otherPaths = ElasticResultPaths::forDisplacement(directory.filePath(QStringLiteral("other.displacement.csv")));
    expect(ElasticResultIO::save(otherPaths, model.mesh, elasticFrames, historyHash, error),
           "Could not create a second elastic cache batch");
    const ElasticResultPaths mixed{paths.nodes, otherPaths.integrationPoints};
    expect(ElasticResultIO::loadNodal(mixed, model.mesh, history.frames, historyHash, error).empty() &&
               ElasticResultIO::loadIntegrationPoints(mixed, model.mesh.elements.front(), history.frames, 1, historyHash, error).empty(),
           "A partial replacement mixed different elastic cache batches");

    const auto &atRest = nodal[0];
    const auto &atLoad = nodal[1];
    expect(vtkBuilder::setNodalFields(modelGrid.grid, atLoad), "Could not show the loaded frame");
    auto *stress = modelGrid.grid->GetPointData()->GetArray("Stress11");
    expect(stress && std::abs(stress->GetComponent(1, 0) - 24.0) < 1e-10, "Incorrect stress at the shared node in the loaded frame");
    const auto before = stress->GetMTime();
    expect(vtkBuilder::setNodalFields(modelGrid.grid, atRest), "Could not return to the initial frame");
    expect(modelGrid.grid->GetPointData()->GetArray("Stress11") == stress && stress->GetMTime() > before &&
               stress->GetComponent(0, 0) == 0.0 && stress->GetComponent(1, 0) == 0.0,
           "Returning to the initial frame did not clear the previous stress");
    QFile points(paths.integrationPoints);
    expect(points.open(QIODevice::ReadWrite) && points.resize(points.size() - 1), "Could not truncate the integration-point index");
    points.close();
    expect(ElasticResultIO::loadNodal(paths, model.mesh, history.frames, historyHash, error).empty(),
           "An incomplete integration-point CSV was accepted");
}

}  // namespace

int main() {
    try {
        checkResults();
        checkNodalResults();
        std::cout << "Displacement result smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Displacement result smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
