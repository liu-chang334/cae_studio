#pragma once

#include "DisplacementFrame.h"
#include "MeshData.h"

#include <QByteArray>
#include <QString>

class vtkUnstructuredGrid;

struct DisplacementReadResult {
    std::vector<DisplacementFrame> frames;
    QString error;
    bool success() const { return !frames.empty() && error.isEmpty(); }
};

class DisplacementResultIO {
public:
    // 返回十六进制 SHA-256；失败时返回空数组并设置 error。
    static QByteArray fingerprint(const QString &path, QString &error);

    static bool save(const QString &path, vtkUnstructuredGrid *grid, const QByteArray &inputHash, QString &error);
    static bool save(const QString &path, const MeshData &mesh, const linalg::VectorX &displacements,
                     const QByteArray &inputHash, QString &error);
    static bool saveHistory(const QString &path, const MeshData &mesh, const std::vector<DisplacementFrame> &frames,
                            const QByteArray &inputHash, QString &error);

    // 按当前网格的点顺序读取单帧或多帧位移。
    static DisplacementReadResult load(const QString &path, vtkUnstructuredGrid *modelGrid, const QByteArray &expectedHash);
};
