#pragma once
#include <QString>
#include <QStringList>
#include <vtkSmartPointer.h>

class vtkUnstructuredGrid;

struct MeshImportResult {
    vtkSmartPointer<vtkUnstructuredGrid> grid;
    QString error;
    QStringList warnings;

    bool success() const { return grid != nullptr && error.isEmpty(); }
};

class AbaqusInpReader {
public:
    MeshImportResult read(const QString &path) const;
};