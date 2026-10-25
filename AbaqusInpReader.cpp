#include "AbaqusInpReader.h"

#include <QFileDialog>
#include <QStringLiteral>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

MeshImportResult AbaqusInpReader::read(const QString &path) const {
    MeshImportResult result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("无法打开文件: %1").arg(path);
        return result;
    }
    QTextStream in(&f);
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto ugrid = vtkSmartPointer<vtkUnstructuredGrid>::New();

    QHash<int, vtkIdType> id2pid;
    id2pid.reserve(1000000);

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
            int nid = parts[0].trimmed().toInt(&okId);
            double x = parts[1].trimmed().toDouble(&okx);
            double y = parts[2].trimmed().toDouble(&oky);
            double z = parts[3].trimmed().toDouble(&okz);
            if (!okId || !okx || !oky || !okz)
                continue;

            vtkIdType pid = points->InsertNextPoint(x, y, z);
            id2pid.insert(nid, pid);
        } else if (sec == ElementC3D8) {
            if (parts.size() < 9)
                continue;
            bool ok = false;
            int eid = parts[0].trimmed().toInt(&ok);
            Q_UNUSED(eid);
            if (!ok)
                continue;

            vtkIdType conn[8];
            bool allok = true;
            for (int i = 0; i < 8; i++) {
                bool okn = false;
                int nid = parts[i + 1].trimmed().toInt(&okn);
                if (!okn || !id2pid.contains(nid)) {
                    allok = false;
                    break;
                }
                conn[i] = id2pid[nid];
            }
            if (!allok)
                continue;
            ugrid->InsertNextCell(VTK_HEXAHEDRON, 8, conn);
        }
    }

    f.close();
    ugrid->SetPoints(points);

    if (ugrid->GetNumberOfPoints() == 0 || ugrid->GetNumberOfCells() == 0) {
        result.error = QStringLiteral("未读取到有效网格");
        return result;
    }
    result.grid = ugrid;
    return result;
}