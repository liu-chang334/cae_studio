#pragma once

#include "vtkBuilder.h"

#include <QByteArray>
#include <QString>

class DisplacementResultIO {
public:
    // 返回十六进制 SHA-256；失败时返回空数组并设置 error。
    static QByteArray fingerprint(const QString &path, QString &error);

    static bool save(const QString &path, vtkUnstructuredGrid *grid, const QByteArray &inputHash, QString &error);

    // 按节点编号将磁盘位移装填到当前 INP 对应的网格。
    static vtkBuildResult load(const QString &path, vtkUnstructuredGrid *modelGrid, const QByteArray &expectedHash);
};
