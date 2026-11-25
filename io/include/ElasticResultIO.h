#pragma once

#include "DisplacementFrame.h"
#include "ElasticResultFrame.h"
#include "MeshData.h"

#include <QByteArray>
#include <QString>

struct ElasticResultPaths {
    QString nodes;
    QString integrationPoints;

    static ElasticResultPaths forDisplacement(const QString &displacementPath);
};

class ElasticResultIO {
public:
    using NodalHistory = std::vector<std::vector<NodalStressStrainValues>>;
    // 将求解器快照写成两个 CSV。displacementHash 是配套位移 CSV 的 SHA-256。
    static bool save(const ElasticResultPaths &paths, const MeshData &mesh, const std::vector<ElasticResultFrame> &frames,
                     const QByteArray &displacementHash, QString &error);

    // 返回网格节点顺序的历程，同时检查配套积分点 CSV 的指纹和帧索引。
    static NodalHistory loadNodal(const ElasticResultPaths &paths, const MeshData &mesh, const std::vector<DisplacementFrame> &frames,
                                  const QByteArray &displacementHash, QString &error);

    // 通过帧索引直接定位积分点 CSV，仅解析所选单元的状态。frameIndex 为 0-based。
    static std::vector<IntegrationPointState> loadIntegrationPoints(const ElasticResultPaths &paths, const MeshElement &element,
                                                                    const std::vector<DisplacementFrame> &frames,
                                                                    std::size_t frameIndex, const QByteArray &displacementHash,
                                                                    QString &error);
};
