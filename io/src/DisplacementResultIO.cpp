#include "DisplacementResultIO.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkTypeInt64Array.h>
#include <vtkUnstructuredGrid.h>

#include <array>
#include <cmath>
#include <utility>

namespace {

constexpr const char *Version = "# CAEStudio displacement v1";
constexpr const char *Header = "NodeId,UMagnitude,U1,U2,U3";
constexpr const char *HistoryVersion = "# CAEStudio displacement v2";
constexpr const char *HistoryHeader = "Time,NodeId,U1,U2,U3";
const std::array<const char *, 4> FieldNames = {
    "DisplacementMagnitude", "DisplacementX", "DisplacementY", "DisplacementZ"};

bool writeStaticRow(QTextStream &out, MeshId id, const std::array<double, 4> &values, QString &error) {
    out << static_cast<qint64>(id);
    for (const double value : values) {
        if (!std::isfinite(value)) {
            error = QStringLiteral("位移结果包含非有限数值");
            return false;
        }
        out << ',' << value;
    }
    out << '\n';
    return true;
}

template <typename WriteRows>
bool writeResult(const QString &path, const QByteArray &inputHash, const char *version, QString &error, WriteRows writeRows) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法创建结果文件：%1").arg(file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setLocale(QLocale::c());
    out.setRealNumberNotation(QTextStream::ScientificNotation);
    out.setRealNumberPrecision(17);
    out << version << '\n' << "# inp_sha256=" << QString::fromLatin1(inputHash) << '\n';
    if (!writeRows(out)) {
        file.cancelWriting();
        return false;
    }
    out.flush();
    if (out.status() != QTextStream::Ok) {
        error = QStringLiteral("写入位移结果失败");
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = QStringLiteral("提交结果文件失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

}  // namespace

QByteArray DisplacementResultIO::fingerprint(const QString &path, QString &error) {
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file) || file.error() != QFileDevice::NoError) {
        error = QStringLiteral("计算 %1 的指纹失败：%2").arg(path, file.errorString());
        return {};
    }
    return hash.result().toHex();
}

bool DisplacementResultIO::save(const QString &path, vtkUnstructuredGrid *grid, const QByteArray &inputHash, QString &error) {
    error.clear();
    if (!grid || grid->GetNumberOfPoints() == 0 || inputHash.isEmpty()) {
        error = QStringLiteral("没有可保存的位移结果");
        return false;
    }

    auto *nodeIds = vtkTypeInt64Array::SafeDownCast(grid->GetPointData()->GetArray("NodeIds"));
    const vtkIdType count = grid->GetNumberOfPoints();
    if (!nodeIds || nodeIds->GetNumberOfComponents() != 1 || nodeIds->GetNumberOfTuples() != count) {
        error = QStringLiteral("结果缺少有效的节点编号");
        return false;
    }

    std::array<vtkDataArray *, 4> fields{};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        fields[i] = grid->GetPointData()->GetArray(FieldNames[i]);
        if (!fields[i] || fields[i]->GetNumberOfComponents() != 1 || fields[i]->GetNumberOfTuples() != count) {
            error = QStringLiteral("缺少位移数组：%1").arg(QString::fromLatin1(FieldNames[i]));
            return false;
        }
    }

    return writeResult(path, inputHash, Version, error, [&](QTextStream &out) {
        out << Header << '\n';
        for (vtkIdType point = 0; point < count; ++point) {
            std::array<double, 4> values{};
            for (std::size_t i = 0; i < values.size(); ++i)
                values[i] = fields[i]->GetComponent(point, 0);
            if (!writeStaticRow(out, nodeIds->GetValue(point), values, error))
                return false;
        }
        return true;
    });
}

bool DisplacementResultIO::save(const QString &path, const MeshData &mesh, const linalg::VectorX &displacements,
                                 const QByteArray &inputHash, QString &error) {
    error.clear();
    if (mesh.nodes.empty() || displacements.size() != 3 * mesh.nodes.size() || inputHash.isEmpty()) {
        error = QStringLiteral("位移自由度与网格尺寸不一致或缺少 INP 指纹");
        return false;
    }
    return writeResult(path, inputHash, Version, error, [&](QTextStream &out) {
        out << Header << '\n';
        for (std::size_t node = 0; node < mesh.nodes.size(); ++node) {
            const double *u = displacements.data() + 3 * node;
            if (!writeStaticRow(out, mesh.nodes[node].id, {std::hypot(u[0], u[1], u[2]), u[0], u[1], u[2]}, error))
                return false;
        }
        return true;
    });
}

bool DisplacementResultIO::saveHistory(const QString &path, const MeshData &mesh, const std::vector<DisplacementFrame> &frames,
                                       const QByteArray &inputHash, QString &error) {
    error.clear();
    if (frames.empty() || mesh.nodes.empty() || inputHash.isEmpty()) {
        error = QStringLiteral("没有可保存的位移历程");
        return false;
    }
    return writeResult(path, inputHash, HistoryVersion, error, [&](QTextStream &out) {
        out << "# frames=" << static_cast<qulonglong>(frames.size()) << '\n' << HistoryHeader << '\n';
        double previousTime = -1.0;
        for (const auto &frame : frames) {
            if (!std::isfinite(frame.time) || frame.time < 0.0 || frame.time <= previousTime ||
                frame.displacements.size() != 3 * mesh.nodes.size()) {
                error = QStringLiteral("位移帧的时间或节点数量不正确");
                return false;
            }
            previousTime = frame.time;
            const QString time = QString::number(frame.time, 'e', 17);
            for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
                out << time << ',' << static_cast<qint64>(mesh.nodes[i].id);
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const double value = frame.displacements[3 * i + axis];
                    if (!std::isfinite(value)) {
                        error = QStringLiteral("位移帧包含非有限数值");
                        return false;
                    }
                    out << ',' << value;
                }
                out << '\n';
            }
        }
        return true;
    });
}

DisplacementReadResult DisplacementResultIO::load(const QString &path, vtkUnstructuredGrid *modelGrid, const QByteArray &expectedHash) {
    if (!modelGrid || modelGrid->GetNumberOfPoints() == 0)
        return {{}, QStringLiteral("请先加载模型")};
    if (expectedHash.isEmpty())
        return {{}, QStringLiteral("缺少 INP 指纹")};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {{}, QStringLiteral("无法打开结果文件：%1").arg(file.errorString())};

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const auto version = in.readLine();
    const bool history = version == QLatin1String(HistoryVersion);
    if (!history && version != QLatin1String(Version))
        return {{}, QStringLiteral("结果文件版本或格式不正确")};
    const QString hashLine = QStringLiteral("# inp_sha256=") + QString::fromLatin1(expectedHash);
    if (in.readLine() != hashLine)
        return {{}, QStringLiteral("结果与当前 INP 不匹配，请重新求解")};
    int frameCount = 1;
    if (history) {
        const auto frameCountLine = in.readLine();
        const auto prefix = QStringLiteral("# frames=");
        bool ok = false;
        frameCount = frameCountLine.mid(prefix.size()).toInt(&ok);
        if (!frameCountLine.startsWith(prefix) || !ok || frameCount <= 0)
            return {{}, QStringLiteral("位移历程的帧数不正确")};
    }
    if (in.readLine() != QLatin1String(history ? HistoryHeader : Header))
        return {{}, QStringLiteral("位移结果表头不正确")};

    auto *nodeIds = vtkTypeInt64Array::SafeDownCast(modelGrid->GetPointData()->GetArray("NodeIds"));
    const vtkIdType count = modelGrid->GetNumberOfPoints();
    if (!nodeIds || nodeIds->GetNumberOfComponents() != 1 || nodeIds->GetNumberOfTuples() != count)
        return {{}, QStringLiteral("模型缺少有效的节点编号")};

    QHash<qint64, vtkIdType> pointByNode;
    for (vtkIdType point = 0; point < count; ++point) {
        const qint64 id = static_cast<qint64>(nodeIds->GetValue(point));
        if (pointByNode.contains(id))
            return {{}, QStringLiteral("模型存在重复节点编号")};
        pointByNode.insert(id, point);
    }

    DisplacementReadResult result;
    const auto pointCount = static_cast<std::size_t>(count);
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        DisplacementFrame frame{0.0, linalg::VectorX(3 * pointCount)};
        std::vector<bool> seen(pointCount, false);
        for (std::size_t row = 0; row < pointCount;) {
            if (in.atEnd())
                return {{}, QStringLiteral("结果缺少位移帧或节点")};
            const auto line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const auto columns = line.split(',', Qt::KeepEmptyParts);
            if (columns.size() != 5)
                return {{}, QStringLiteral("结果行必须包含五列")};

            bool timeOrMagnitudeOk = false;
            const double timeOrMagnitude = columns[history ? 0 : 1].toDouble(&timeOrMagnitudeOk);
            if (!timeOrMagnitudeOk || !std::isfinite(timeOrMagnitude) || timeOrMagnitude < 0.0)
                return {{}, QStringLiteral("结果包含无效的帧时间或总位移")};
            if (history) {
                if (row == 0) {
                    frame.time = timeOrMagnitude;
                    if (!result.frames.empty() && frame.time <= result.frames.back().time)
                        return {{}, QStringLiteral("位移帧的时间必须严格递增")};
                } else if (timeOrMagnitude != frame.time) {
                    return {{}, QStringLiteral("同一位移帧中的节点时间不一致")};
                }
            }

            const auto &idColumn = columns[history ? 1 : 0];
            bool idOk = false;
            const qint64 id = idColumn.toLongLong(&idOk);
            const auto entry = pointByNode.constFind(id);
            if (!idOk || entry == pointByNode.cend())
                return {{}, QStringLiteral("结果包含无效或未知节点：%1").arg(idColumn)};
            const auto point = static_cast<std::size_t>(entry.value());
            if (seen[point])
                return {{}, QStringLiteral("结果帧存在重复节点：%1").arg(id)};
            for (int axis = 0; axis < 3; ++axis) {
                bool ok = false;
                const double value = columns[2 + axis].toDouble(&ok);
                if (!ok || !std::isfinite(value))
                    return {{}, QStringLiteral("结果包含无效位移数值")};
                frame.displacements[3 * point + axis] = value;
            }
            seen[point] = true;
            ++row;
        }
        result.frames.push_back(std::move(frame));
    }
    while (!in.atEnd()) {
        if (!in.readLine().trimmed().isEmpty())
            return {{}, QStringLiteral("结果包含超出声明帧数的位移记录")};
    }
    if (file.error() != QFileDevice::NoError)
        return {{}, QStringLiteral("读取结果文件失败")};
    return result;
}
