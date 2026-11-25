#include "ElasticResultIO.h"

#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace {
constexpr char NodeVersion[] = "# CAEStudio nodal stress-strain v1";
constexpr char PointVersion[] = "# CAEStudio integration-point stress-strain v1";
constexpr char ComponentColumns[] = "E11,E22,E33,E12,E13,E23,S11,S22,S33,S12,S13,S23";
constexpr char IndexPrefix[] = "# frame_offsets=";
constexpr char IndexPointer[] = "# frame_index=";
constexpr int IndexWidth = 20;
constexpr qint64 IndexPointerBytes = sizeof(IndexPointer) - 1 + IndexWidth + 1;
constexpr char SetPrefix[] = "# result_set=";

QByteArray columnHeader(bool integrationPoints) {
    return QByteArray(integrationPoints ? "Time,ElementId,IntegrationPoint," : "Time,NodeId,") + ComponentColumns;
}

void writeHeader(QTextStream &out, bool integrationPoints, const QByteArray &hash, const QByteArray &resultSet,
                 std::size_t frameCount) {
    out.setEncoding(QStringConverter::Utf8);
    out.setLocale(QLocale::c());
    out.setRealNumberNotation(QTextStream::ScientificNotation);
    out.setRealNumberPrecision(17);
    out << (integrationPoints ? PointVersion : NodeVersion) << '\n'
        << "# displacement_sha256=" << QString::fromLatin1(hash) << '\n'
        << SetPrefix << QString::fromLatin1(resultSet) << '\n'
        << "# frames=" << static_cast<qulonglong>(frameCount) << '\n'
        << QString::fromLatin1(columnHeader(integrationPoints)) << '\n';
}

QByteArray openResult(QFile &file, bool integrationPoints, const QByteArray &hash, std::size_t frameCount, QString &error) {
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法打开 %1：%2").arg(file.fileName(), file.errorString());
        return {};
    }
    const auto version = file.readLine().trimmed();
    const auto source = file.readLine().trimmed();
    const auto resultSet = file.readLine().trimmed();
    if (version != (integrationPoints ? PointVersion : NodeVersion) || source != "# displacement_sha256=" + hash ||
        !resultSet.startsWith(SetPrefix) || resultSet.size() != sizeof(SetPrefix) - 1 + 36 ||
        file.readLine().trimmed() != "# frames=" + QByteArray::number(static_cast<qulonglong>(frameCount)) ||
        file.readLine().trimmed() != columnHeader(integrationPoints)) {
        error = QStringLiteral("应变应力 CSV 的版本、位移指纹或帧数不匹配：%1").arg(file.fileName());
        return {};
    }
    return resultSet;
}

bool openResultPair(QFile &nodes, QFile &points, const QByteArray &hash, std::size_t frameCount, QString &error) {
    const auto nodeSet = openResult(nodes, false, hash, frameCount, error);
    if (nodeSet.isEmpty())
        return false;
    const auto pointSet = openResult(points, true, hash, frameCount, error);
    if (pointSet.isEmpty())
        return false;
    if (nodeSet != pointSet) {
        error = QStringLiteral("节点与积分点 CSV 来自不同的计算批次");
        return false;
    }
    return true;
}

// 文件末尾的固定宽度指针定位帧索引，查看积分点时只读取所选帧。
std::vector<qint64> readPointIndex(QFile &file, std::size_t frameCount, QString &error) {
    const auto dataStart = file.pos();
    const auto pointerPos = file.size() - IndexPointerBytes;
    error = QStringLiteral("积分点 CSV 的帧索引不完整或无效：%1").arg(file.fileName());
    if (pointerPos < dataStart || !file.seek(pointerPos))
        return {};
    const auto pointer = file.readLine();
    bool ok = false;
    const auto indexPos = pointer.mid(sizeof(IndexPointer) - 1).trimmed().toLongLong(&ok);
    if (!pointer.startsWith(IndexPointer) || !ok || indexPos <= dataStart || indexPos >= pointerPos || !file.seek(indexPos))
        return {};
    const auto index = file.readLine();
    if (!index.startsWith(IndexPrefix) || file.pos() != pointerPos)
        return {};
    const auto columns = index.mid(sizeof(IndexPrefix) - 1).trimmed().split(',');
    if (static_cast<std::size_t>(columns.size()) != frameCount)
        return {};
    std::vector<qint64> offsets;
    offsets.reserve(frameCount + 1);
    for (const auto &column : columns) {
        const auto offset = column.toLongLong(&ok);
        if (!ok || offset >= indexPos || (offsets.empty() ? offset != dataStart : offset <= offsets.back()))
            return {};
        offsets.push_back(offset);
    }
    offsets.push_back(indexPos);
    error.clear();
    return offsets;
}

bool readValues(const QList<QByteArray> &columns, qsizetype first, NodalStressStrainValues &values, bool isolated = false) {
    if (columns.size() != first + static_cast<qsizetype>(values.size()))
        return false;
    for (auto &value : values) {
        const auto &column = columns[first++];
        if (isolated && column.isEmpty()) {
            value = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        bool ok = false;
        value = column.toDouble(&ok);
        if (isolated || !ok || !std::isfinite(value))
            return false;
    }
    return true;
}

bool sameTime(const QByteArray &column, double time) {
    bool ok = false;
    return column.toDouble(&ok) == time && ok;
}

std::unordered_set<MeshId> connectedNodes(const MeshData &mesh) {
    std::unordered_set<MeshId> ids;
    for (const auto &element : mesh.elements)
        ids.insert(element.nodeIds.begin(), element.nodeIds.end());
    return ids;
}

bool commit(QSaveFile &file, QString &error) {
    if (file.commit())
        return true;
    error = QStringLiteral("无法保存 %1：%2").arg(file.fileName(), file.errorString());
    return false;
}
}  // namespace

ElasticResultPaths ElasticResultPaths::forDisplacement(const QString &displacementPath) {
    const auto suffix = QStringLiteral(".displacement.csv");
    const auto base = displacementPath.endsWith(suffix) ? displacementPath.chopped(suffix.size()) : displacementPath;
    return {base + QStringLiteral(".stress-strain.nodes.csv"), base + QStringLiteral(".stress-strain.integration-points.csv")};
}

bool ElasticResultIO::save(const ElasticResultPaths &paths, const MeshData &mesh, const std::vector<ElasticResultFrame> &frames,
                           const QByteArray &displacementHash, QString &error) {
    error.clear();
    if (frames.empty() || mesh.nodes.empty() || mesh.elements.empty() || displacementHash.isEmpty()) {
        error = QStringLiteral("没有可保存的应变应力结果");
        return false;
    }
    QSaveFile nodes(paths.nodes);
    QSaveFile points(paths.integrationPoints);
    for (auto *file : {&nodes, &points}) {
        if (!file->open(QIODevice::WriteOnly)) {
            error = QStringLiteral("无法创建 %1：%2").arg(file->fileName(), file->errorString());
            return false;
        }
    }
    QTextStream nodeOut(&nodes);
    QTextStream pointOut(&points);
    const auto resultSet = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    writeHeader(nodeOut, false, displacementHash, resultSet, frames.size());
    writeHeader(pointOut, true, displacementHash, resultSet, frames.size());
    const auto connected = connectedNodes(mesh);
    std::vector<qint64> frameOffsets;
    frameOffsets.reserve(frames.size());
    double previousTime = -1.0;
    for (const auto &frame : frames) {
        if (!std::isfinite(frame.time) || frame.time <= previousTime || frame.time < 0.0) {
            error = QStringLiteral("应变应力帧时间无效");
            return false;
        }
        previousTime = frame.time;
        if (frame.nodes.size() != mesh.nodes.size() || frame.integrationPoints.size() != mesh.elements.size()) {
            error = QStringLiteral("应变应力结果与网格尺寸不一致");
            return false;
        }
        const auto time = QString::number(frame.time, 'e', 17);
        for (std::size_t node = 0; node < mesh.nodes.size(); ++node) {
            const auto id = mesh.nodes[node].id;
            const bool isolated = connected.find(id) == connected.end();
            nodeOut << time << ',' << static_cast<qint64>(id);
            for (const double value : frame.nodes[node]) {
                nodeOut << ',';
                if (isolated ? !std::isnan(value) : !std::isfinite(value)) {
                    error = QStringLiteral("节点 %1 的应变应力值无效").arg(static_cast<qint64>(id));
                    return false;
                }
                if (!isolated)
                    nodeOut << value;
            }
            nodeOut << '\n';
        }

        pointOut.flush();
        frameOffsets.push_back(points.pos());
        for (std::size_t cell = 0; cell < mesh.elements.size(); ++cell) {
            const auto &element = mesh.elements[cell];
            const auto &states = frame.integrationPoints[cell];
            if (states.empty() || (element.formulation == ElementFormulation::C3D8 && states.size() != 8)) {
                error = QStringLiteral("单元 %1 的积分点数量无效").arg(static_cast<qint64>(element.id));
                return false;
            }
            for (std::size_t gp = 0; gp < states.size(); ++gp) {
                pointOut << time << ',' << static_cast<qint64>(element.id) << ',' << static_cast<qulonglong>(gp + 1);
                for (const auto *components : {&states[gp].strain, &states[gp].stress}) {
                    for (const double value : *components) {
                        if (!std::isfinite(value)) {
                            error = QStringLiteral("积分点结果包含非有限数值");
                            return false;
                        }
                        pointOut << ',' << value;
                    }
                }
                pointOut << '\n';
            }
        }
    }
    pointOut.flush();
    const auto indexPos = points.pos();
    pointOut << IndexPrefix << frameOffsets.front();
    for (std::size_t frame = 1; frame < frameOffsets.size(); ++frame)
        pointOut << ',' << frameOffsets[frame];
    pointOut << '\n' << IndexPointer << QString::number(indexPos).rightJustified(IndexWidth, QLatin1Char('0')) << '\n';
    nodeOut.flush();
    pointOut.flush();
    if (nodeOut.status() != QTextStream::Ok || pointOut.status() != QTextStream::Ok) {
        error = QStringLiteral("写入应变应力 CSV 失败");
        return false;
    }
    return commit(points, error) && commit(nodes, error);
}

ElasticResultIO::NodalHistory ElasticResultIO::loadNodal(const ElasticResultPaths &paths, const MeshData &mesh,
                                                         const std::vector<DisplacementFrame> &frames,
                                                         const QByteArray &displacementHash, QString &error) {
    error.clear();
    QFile nodes(paths.nodes);
    QFile points(paths.integrationPoints);
    if (!openResultPair(nodes, points, displacementHash, frames.size(), error) ||
        readPointIndex(points, frames.size(), error).empty())
        return {};

    QHash<MeshId, std::size_t> nodeIndices;
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
        nodeIndices.insert(mesh.nodes[i].id, i);
    const auto connected = connectedNodes(mesh);
    NodalHistory result;
    result.reserve(frames.size());
    for (const auto &frame : frames) {
        auto &values = result.emplace_back(mesh.nodes.size());
        std::vector<bool> seen(values.size(), false);
        for (std::size_t row = 0; row < values.size(); ++row) {
            const auto columns = nodes.readLine().trimmed().split(',');
            bool idOk = false;
            const auto id = columns.size() >= 2 ? columns[1].toLongLong(&idOk) : 0;
            const auto node = nodeIndices.constFind(id);
            if (!idOk || node == nodeIndices.cend() || seen[node.value()] || !sameTime(columns[0], frame.time) ||
                !readValues(columns, 2, values[node.value()], connected.find(id) == connected.end())) {
                error = QStringLiteral("节点应变应力 CSV 包含无效、重复或缺失的节点记录");
                return {};
            }
            seen[node.value()] = true;
        }
    }
    if (!nodes.atEnd() || nodes.error() != QFileDevice::NoError) {
        error = QStringLiteral("节点应变应力 CSV 超出声明的帧数或读取失败");
        return {};
    }
    return result;
}

std::vector<IntegrationPointState> ElasticResultIO::loadIntegrationPoints(const ElasticResultPaths &paths, const MeshElement &element,
                                                                          const std::vector<DisplacementFrame> &frames,
                                                                          std::size_t frameIndex, const QByteArray &displacementHash,
                                                                          QString &error) {
    error.clear();
    if (frameIndex >= frames.size()) {
        error = QStringLiteral("积分点帧编号超出范围");
        return {};
    }
    QFile nodes(paths.nodes);
    QFile file(paths.integrationPoints);
    if (!openResultPair(nodes, file, displacementHash, frames.size(), error))
        return {};
    const auto offsets = readPointIndex(file, frames.size(), error);
    if (offsets.empty())
        return {};
    error = QStringLiteral("积分点 CSV 中的单元 %1 记录不完整或无效").arg(static_cast<qint64>(element.id));
    if (!file.seek(offsets[frameIndex]))
        return {};

    std::vector<IntegrationPointState> states;
    while (file.pos() < offsets[frameIndex + 1]) {
        const auto line = file.readLine();
        const auto first = line.indexOf(',');
        const auto second = line.indexOf(',', first + 1);
        if (first < 0 || second < 0)
            return {};
        bool idOk = false;
        const auto id = line.mid(first + 1, second - first - 1).toLongLong(&idOk);
        if (!idOk)
            return {};
        if (id != element.id) {
            if (!states.empty())
                break;
            continue;
        }
        const auto columns = line.trimmed().split(',');
        bool pointOk = false;
        const auto point = columns[2].toULongLong(&pointOk);
        NodalStressStrainValues values{};
        if (!pointOk || point != states.size() + 1 || !sameTime(columns[0], frames[frameIndex].time) || !readValues(columns, 3, values))
            return {};
        auto &state = states.emplace_back();
        const auto stressStart = values.begin() + state.strain.size();
        std::copy(values.begin(), stressStart, state.strain.begin());
        std::copy(stressStart, values.end(), state.stress.begin());
    }
    if (states.empty() || (element.formulation == ElementFormulation::C3D8 && states.size() != 8))
        return {};
    error.clear();
    return states;
}
