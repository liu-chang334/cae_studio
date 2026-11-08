#pragma once

#include "BoundaryData.h"
#include "LoadData.h"
#include "MeshData.h"

#include <QString>
#include <QStringList>

/**
 * @brief 保存读取Abaqus inp文件的结果
 * @details 包含网格数据, 错误信息, 警告信息
 * @note 通过success()方法判断是否成功读取文件
 */
struct MeshReadResult {
    MeshData mesh;
    std::vector<NodeSetLoad> loads;
    std::vector<NodeSetBoundary> boundaries;

    QString error;
    QStringList warnings;

    bool success() const {
        return error.isEmpty() && !mesh.nodes.empty() && !mesh.elements.empty();
    }
};

/**
 * @brief 读取Abaqus inp文件
 * @details 通过read()方法读取文件, 并返回读取结果
 */
class AbaqusInpReader {
public:
    /**
     * @brief 读取Abaqus inp文件
     * @param path 文件路径
     * @return 读取结果, 包含网格数据, 错误信息, 警告信息
     */
    MeshReadResult read(const QString &path) const;
};