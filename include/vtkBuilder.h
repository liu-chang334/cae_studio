#pragma once

#include "MeshData.h"

#include <QString>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

struct vtkBuildResult {
    vtkSmartPointer<vtkUnstructuredGrid> grid;
    QString error;
    bool success() const { return grid != nullptr && error.isEmpty(); }
};

class vtkBuilder {
public:
    vtkBuildResult build(const MeshData &mesh) const;
};