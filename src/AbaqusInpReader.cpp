#include "AbaqusInpReader.h"

#include <cmath>

#include <QFile>
#include <QTextStream>

enum Section { None, Node, ElementC3D8, NodeSet, Load, Boundary, Elastic };

MeshReadResult AbaqusInpReader::read(const QString &path) const {
    MeshReadResult result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("无法打开文件: %1").arg(path);
        return result;
    }
    QTextStream in(&f);
    Section sec = None;
    std::string currentNodeSetName; // 当前正在读取的节点集名称
    qint64 lineNumber = 0;          // 行号计数
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed(); // 逐行读取
        ++lineNumber;
        if (line.isEmpty())
            continue; // 跳过空行
        if (line.startsWith("**"))
            continue; // 跳过注释行
        if (line.startsWith("*")) {
            const auto fields = line.split(',', Qt::KeepEmptyParts); // 将关键字与参数分开,如"*Nset", " nset=Set-1"
            const QString keyword = fields[0].trimmed().toUpper();
            sec = None;
            currentNodeSetName.clear();
            if (keyword == "*NODE") {
                sec = Node;
            } else if (keyword == "*ELEMENT") {
                if (line.toUpper().contains("TYPE=C3D8"))
                    sec = ElementC3D8;
            } else if (keyword == "*NSET") {
                QString setName;
                bool generate = false;
                for (qsizetype i = 1; i < fields.size(); ++i) {
                    const QString parameter = fields[i].trimmed();
                    if (parameter.compare(QStringLiteral("GENERATE"), Qt::CaseInsensitive) == 0) {
                        generate = true;
                        continue;
                    }
                    const auto equalPosition = parameter.indexOf('=');
                    if (equalPosition < 0)
                        continue;
                    const QString key = parameter.left(equalPosition).trimmed();
                    const QString value = parameter.mid(equalPosition + 1).trimmed();
                    if (key.compare(QStringLiteral("NSET"), Qt::CaseInsensitive) == 0) {
                        setName = value;
                    }
                }
                if (generate) {
                    result.error = QStringLiteral("第 %1 行: 暂不支持节点集的 GENERATE 形式. \n原文: %2").arg(lineNumber).arg(line);
                    return result;
                }
                if (setName.isEmpty()) {
                    result.error =
                        QStringLiteral("第 %1 行: 节点集缺少名称，应为 *Nset, nset=名称. \n原文: %2").arg(lineNumber).arg(line);
                    return result;
                }
                currentNodeSetName = setName.toStdString();
                result.mesh.nodeSets.try_emplace(currentNodeSetName);
                sec = NodeSet;
            } else if (keyword == "*LOAD") {
                sec = Load;
            } else if (keyword == "*BOUNDARY") {
                if (fields.size() != 1) {
                    result.error = QStringLiteral("第 %1 行: 当前暂不支持 *Boundary 的关键字参数.").arg(lineNumber);
                    return result;
                }
                sec = Boundary;
            } else if (keyword == "*MATERIAL") {
                MaterialData material;
                // fields[1] 为 " name=Material-1"  取等号右侧作为名称
                material.name = fields[1].section('=', 1).trimmed().toStdString();
                result.materials.push_back(std::move(material));
            } else if (keyword == "*ELASTIC") {
                sec = Elastic;
            }
            continue;
        }

        const auto parts = line.split(',', Qt::KeepEmptyParts); // 按逗号分隔
        if (sec == Node) {
            if (parts.size() < 4) {
                result.error = QStringLiteral("第 %1 行: 节点数据字段不足, 应为 nid, x, y, z. \n原文: %2").arg(lineNumber).arg(line);
                return result;
            }
            bool okId = false, okx = false, oky = false, okz = false;
            MeshId nid = parts[0].trimmed().toLongLong(&okId);
            double x = parts[1].trimmed().toDouble(&okx);
            double y = parts[2].trimmed().toDouble(&oky);
            double z = parts[3].trimmed().toDouble(&okz);
            if (!okId || !okx || !oky || !okz) {
                result.error = QStringLiteral("第 %1 行: 节点编号或坐标无法转换为数值. \n原文: %2").arg(lineNumber).arg(line);
                return result;
            }
            result.mesh.nodes.push_back(MeshNode{nid, {x, y, z}}); // 添加节点
        } else if (sec == ElementC3D8) {
            if (parts.size() < 9) {
                result.error = QStringLiteral("第 %1 行: C3D8单元数据字段不足,"
                                              "应为 eid, nid1, nid2, nid3, nid4, nid5, nid6, nid7, nid8. \n原文: %2")
                                   .arg(lineNumber)
                                   .arg(line);
                return result;
            }
            bool ok = false;
            MeshId eid = parts[0].trimmed().toLongLong(&ok);
            if (!ok) {
                result.error = QStringLiteral("第 %1 行: 单元编号无法转换为整数. \n原文: %2").arg(lineNumber).arg(line);
                return result;
            }
            MeshElement element;
            element.id = eid;
            element.type = ElementType::Hex8;
            element.nodeIds.reserve(8);
            for (int i = 0; i < 8; i++) {
                bool okn = false;
                MeshId nid = parts[i + 1].trimmed().toLongLong(&okn);
                if (!okn) {
                    result.error = QStringLiteral("第 %1 行: 单元 %2 的第 %3 个节点编号"
                                                  "无法转换为整数. \n原文: %4")
                                       .arg(lineNumber)
                                       .arg(static_cast<qlonglong>(eid))
                                       .arg(i + 1)
                                       .arg(line);
                    return result;
                }
                element.nodeIds.push_back(nid); // 添加节点ID
            }
            result.mesh.elements.push_back(std::move(element)); // 添加单元
        } else if (sec == NodeSet) {
            auto &nodeIds = result.mesh.nodeSets.at(currentNodeSetName);
            for (qsizetype i = 0; i < parts.size(); ++i) {
                const QString text = parts[i].trimmed();
                if (text.isEmpty() && i == parts.size() - 1)
                    continue;
                bool ok = false;
                const MeshId nid = text.toLongLong(&ok);
                if (!ok) {
                    result.error = QStringLiteral("第 %1 行: 节点集 %2 中的节点编号无法转换为整数。"
                                                  "\n原文: %3")
                                       .arg(lineNumber)
                                       .arg(QString::fromStdString(currentNodeSetName))
                                       .arg(line);
                    return result;
                }
                nodeIds.push_back(nid);
            }
        } else if (sec == Load) {
            auto loadParts = parts;
            if (!loadParts.isEmpty() && loadParts.back().trimmed().isEmpty())
                loadParts.removeLast();
            if (loadParts.size() != 3) {
                result.error = QStringLiteral("第 %1 行: 荷载应为 节点集名称, 方向, 力值. \n原文: %2").arg(lineNumber).arg(line);
                return result;
            }
            const QString setName = loadParts[0].trimmed();
            bool okDof = false;
            bool okMagnitude = false;
            const int dof = loadParts[1].trimmed().toInt(&okDof);
            const double magnitude = loadParts[2].trimmed().toDouble(&okMagnitude);
            if (setName.isEmpty() || !okDof || dof < 1 || dof > 3 || !okMagnitude || !std::isfinite(magnitude)) {
                result.error = QStringLiteral("第 %1 行: 荷载名称不能为空, 方向须为 1～3, 力值须为有限数值."
                                              "\n原文: %2")
                                   .arg(lineNumber)
                                   .arg(line);
                return result;
            }
            result.loads.push_back(NodeSetLoad{setName.toStdString(), dof, magnitude});
        } else if (sec == Boundary) {
            auto boundaryParts = parts;
            if (!boundaryParts.isEmpty() && boundaryParts.back().trimmed().isEmpty())
                boundaryParts.removeLast();
            if (boundaryParts.size() != 3) {
                result.error = QStringLiteral("第 %1 行: 边界条件应为 节点集名称, 自由度, 指定位移."
                                              "\n原文: %2")
                                   .arg(lineNumber)
                                   .arg(line);
                return result;
            }
            const QString setName = boundaryParts[0].trimmed();
            bool okDof = false;
            bool okDisplacement = false;
            const int dof = boundaryParts[1].trimmed().toInt(&okDof);
            const double displacement = boundaryParts[2].trimmed().toDouble(&okDisplacement);
            if (setName.isEmpty() || !okDof || dof < 1 || dof > 3 || !okDisplacement || !std::isfinite(displacement)) {
                result.error = QStringLiteral("第 %1 行: 节点集名称不能为空, 自由度须为 1～3,"
                                              "位移须为有限数值. \n原文: %2")
                                   .arg(lineNumber)
                                   .arg(line);
                return result;
            }
            result.boundaries.push_back(NodeSetBoundary{setName.toStdString(), dof, displacement});
        } else if (sec == Elastic) {
            const double E = parts[0].trimmed().toDouble();
            const double nu = parts[1].trimmed().toDouble();
            result.materials.back().elastic = IsotropicElasticData{E, nu};
        }
    }
    f.close();
    if (result.mesh.nodes.empty() || result.mesh.elements.empty()) {
        result.error = QStringLiteral("未读取到有效网格.");
        return result;
    }
    for (const auto &load : result.loads) {
        const auto it = result.mesh.nodeSets.find(load.nodeSetName);
        if (it == result.mesh.nodeSets.end() || it->second.empty()) {
            result.error = QStringLiteral("荷载引用的节点集不存在或为空: %1").arg(QString::fromStdString(load.nodeSetName));
            return result;
        }
    }
    for (const auto &boundary : result.boundaries) {
        const auto it = result.mesh.nodeSets.find(boundary.nodeSetName);
        if (it == result.mesh.nodeSets.end() || it->second.empty()) {
            result.error = QStringLiteral("边界条件引用的节点集不存在或为空: %1").arg(QString::fromStdString(boundary.nodeSetName));
            return result;
        }
    }
    return result;
}