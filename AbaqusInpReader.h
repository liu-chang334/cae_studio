#pragma once
#include "MeshData.h"

#include <QString>
#include <QStringList>

struct MeshReadResult {
    MeshData mesh;
    QString error;
    QStringList warnings;

    bool success() const {
        return error.isEmpty() && !mesh.nodes.empty() && !mesh.elements.empty();
    }
};

class AbaqusInpReader {
public:
    MeshReadResult read(const QString &path) const;
};