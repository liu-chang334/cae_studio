#pragma once

#include "AnalysisModel.h"

#include <QString>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

struct vtkBuildResult {
    vtkSmartPointer<vtkUnstructuredGrid> grid;
    QString error;
    bool success() const { return grid != nullptr && error.isEmpty(); }
};

class vtkBuilder {
public:
    /**
     * @brief 从网格数据中构建vtk网格, 用于显示网格
     * @param mesh 网格数据
     */
    vtkBuildResult build(const MeshData &mesh) const;

    /**
     * @brief 从网格数据和计算结果中构建vtk网格, 用于显示计算结果
     * @param mesh 网格数据
     * @param analysis 计算模型
     * @param solution 计算结果
     */
    vtkBuildResult buildResults(const MeshData &mesh, const AnalysisModel &analysis,
                                const AnalysisModel::LinearStaticResult &solution) const;
};