#include "AbaqusInpReader.h"

#include <QFile>
#include <QTextStream>

MeshReadResult AbaqusInpReader::read(const QString &path) const {
    MeshReadResult result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("无法打开文件: %1").arg(path);
        return result;
    }
    QTextStream in(&f);

    enum Section { None, Node, ElementC3D8 };
    Section sec = None;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith("**"))
            continue;
        if (line.startsWith("*")) {
            const QString upper = line.toUpper();
            if (upper.startsWith("*NODE")) {
                sec = Node;
            } else if (upper.startsWith("*ELEMENT")) {
                if (upper.contains("TYPE=C3D8"))
                    sec = ElementC3D8;
                else
                    sec = None;
            } else {
                sec = None;
            }
            continue;
        }

        const auto parts = line.split(',', Qt::SkipEmptyParts);

        if (sec == Node) {
            if (parts.size() < 4)
                continue;
            bool okId = false, okx = false, oky = false, okz = false;
            MeshId nid = parts[0].trimmed().toLongLong(&okId);
            double x = parts[1].trimmed().toDouble(&okx);
            double y = parts[2].trimmed().toDouble(&oky);
            double z = parts[3].trimmed().toDouble(&okz);
            if (!okId || !okx || !oky || !okz)
                continue;

            result.mesh.nodes.push_back(MeshNode{nid, {x, y, z}});
        } else if (sec == ElementC3D8) {
            if (parts.size() < 9)
                continue;
            bool ok = false;
            MeshId eid = parts[0].trimmed().toLongLong(&ok);
            if (!ok)
                continue;

            MeshElement element;
            element.id = eid;
            element.type = ElementType::Hex8;
            element.nodeIds.reserve(8);
            bool allok = true;
            for (int i = 0; i < 8; i++) {
                bool okn = false;
                MeshId nid = parts[i + 1].trimmed().toLongLong(&okn);
                if (!okn) {
                    allok = false;
                    break;
                }
                element.nodeIds.push_back(nid);
            }
            if (!allok)
                continue;
            result.mesh.elements.push_back(std::move(element));
        }
    }

    f.close();

    if (result.mesh.nodes.empty() || result.mesh.elements.empty()) {
        result.error = QStringLiteral("未读取到有效网格");
        return result;
    }
    return result;
}