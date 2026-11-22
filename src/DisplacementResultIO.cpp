#include "DisplacementResultIO.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkTypeInt64Array.h>

#include <array>
#include <cmath>

namespace {

constexpr const char *Version = "# CAEStudio displacement v1";
constexpr const char *Header = "NodeId,UMagnitude,U1,U2,U3";
const std::array<const char *, 4> FieldNames = {
    "DisplacementMagnitude", "DisplacementX", "DisplacementY", "DisplacementZ"};

}  // namespace

QByteArray DisplacementResultIO::fingerprint(const QString &path, QString &error) {
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取 INP：%1").arg(file.errorString());
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file) || file.error() != QFileDevice::NoError) {
        error = QStringLiteral("计算 INP 指纹失败：%1").arg(file.errorString());
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

    // 成功提交前不替换旧文件，避免失败或中断破坏已有结果。
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
    out << Version << '\n';
    out << "# inp_sha256=" << QString::fromLatin1(inputHash) << '\n';
    out << Header << '\n';

    for (vtkIdType point = 0; point < count; ++point) {
        out << static_cast<qint64>(nodeIds->GetValue(point));
        for (auto *field : fields) {
            const double value = field->GetComponent(point, 0);
            if (!std::isfinite(value)) {
                error = QStringLiteral("位移结果包含非有限数值");
                file.cancelWriting();
                return false;
            }
            out << ',' << value;
        }
        out << '\n';
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

vtkBuildResult DisplacementResultIO::load(const QString &path, vtkUnstructuredGrid *modelGrid, const QByteArray &expectedHash) {
    if (!modelGrid || modelGrid->GetNumberOfPoints() == 0)
        return {nullptr, QStringLiteral("请先加载模型")};
    if (expectedHash.isEmpty())
        return {nullptr, QStringLiteral("缺少 INP 指纹")};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {nullptr, QStringLiteral("无法打开结果文件：%1").arg(file.errorString())};

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    if (in.readLine() != QString::fromLatin1(Version))
        return {nullptr, QStringLiteral("结果文件版本或格式不正确")};
    const QString hashLine = QStringLiteral("# inp_sha256=") + QString::fromLatin1(expectedHash);
    if (in.readLine() != hashLine)
        return {nullptr, QStringLiteral("结果与当前 INP 不匹配，请重新求解")};
    if (in.readLine() != QString::fromLatin1(Header))
        return {nullptr, QStringLiteral("位移结果表头不正确")};

    auto *nodeIds = vtkTypeInt64Array::SafeDownCast(modelGrid->GetPointData()->GetArray("NodeIds"));
    const vtkIdType count = modelGrid->GetNumberOfPoints();
    if (!nodeIds || nodeIds->GetNumberOfComponents() != 1 || nodeIds->GetNumberOfTuples() != count)
        return {nullptr, QStringLiteral("模型缺少有效的节点编号")};

    QHash<qint64, vtkIdType> pointByNode;
    for (vtkIdType point = 0; point < count; ++point) {
        const qint64 id = static_cast<qint64>(nodeIds->GetValue(point));
        if (pointByNode.contains(id))
            return {nullptr, QStringLiteral("模型存在重复节点编号")};
        pointByNode.insert(id, point);
    }

    std::array<vtkSmartPointer<vtkDoubleArray>, 4> fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        fields[i] = vtkSmartPointer<vtkDoubleArray>::New();
        fields[i]->SetName(FieldNames[i]);
        fields[i]->SetNumberOfComponents(1);
        fields[i]->SetNumberOfTuples(count);
    }
    auto vectors = vtkSmartPointer<vtkDoubleArray>::New();
    vectors->SetName("Displacements");
    vectors->SetNumberOfComponents(3);
    vectors->SetNumberOfTuples(count);

    QSet<qint64> seen;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QStringList columns = line.split(',', Qt::KeepEmptyParts);
        if (columns.size() != 5)
            return {nullptr, QStringLiteral("结果行必须包含五列")};
        bool idOk = false;
        const qint64 id = columns[0].toLongLong(&idOk);
        if (!idOk || !pointByNode.contains(id))
            return {nullptr, QStringLiteral("结果包含无效或未知节点：%1").arg(columns[0])};
        if (seen.contains(id))
            return {nullptr, QStringLiteral("结果存在重复节点：%1").arg(id)};

        std::array<double, 4> values{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            bool ok = false;
            values[i] = columns[static_cast<int>(i) + 1].toDouble(&ok);
            if (!ok || !std::isfinite(values[i]))
                return {nullptr, QStringLiteral("结果包含无效位移数值")};
        }
        if (values[0] < 0.0)
            return {nullptr, QStringLiteral("总位移不能为负值")};
        const vtkIdType point = pointByNode.value(id);
        for (std::size_t i = 0; i < fields.size(); ++i)
            fields[i]->SetValue(point, values[i]);
        vectors->SetTuple3(point, values[1], values[2], values[3]);
        seen.insert(id);
    }
    if (file.error() != QFileDevice::NoError)
        return {nullptr, QStringLiteral("读取结果文件失败")};
    if (seen.size() != pointByNode.size())
        return {nullptr, QStringLiteral("结果文件缺少部分节点")};

    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->DeepCopy(modelGrid);
    for (const auto &field : fields)
        grid->GetPointData()->AddArray(field);
    grid->GetPointData()->SetVectors(vectors);
    return {grid, QString()};
}
