#include "DisplacementResultIO.h"
#include "vtkBuilder.h"

#include <QFile>
#include <QTemporaryDir>

#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkTypeInt64Array.h>
#include <vtkUnstructuredGrid.h>

#include <iostream>
#include <limits>
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

}  // namespace

int main() {
    try {
        checkResults();
        std::cout << "Displacement result smoke test passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Displacement result smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
