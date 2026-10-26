#pragma once

#include "MeshData.h"

#include <QString>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

struct vtkMeshBuildResult {
    vtkSmartPointer<vtkUnstructuredGrid> grid;
    QString error;
    bool success() const { return grid != nullptr && error.isEmpty(); }
};

class vtkMeshBuilder {
public:
    vtkMeshBuildResult build(const MeshData &mesh) const;
};