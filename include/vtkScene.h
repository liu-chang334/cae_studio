#pragma once

#include "MeshData.h"

#include <optional>
#include <vtkSmartPointer.h>

class vtkActor;
class vtkRenderer;
class vtkUnstructuredGrid;

/**
 * @brief vtk场景类
 * @details 管理vtk场景, 场景对象, 渲染器, 相机
 */
class vtkScene {
public:
    vtkScene();
    ~vtkScene() = default;

    /**
     * @brief 获取渲染器
     * @return 渲染器指针
     */
    vtkRenderer *renderer() const;

    /**
     * @brief 设置网格
     * @param grid 网格指针
     * @details 显示网格, 并重置导入动画进度为0
     */
    void setMesh(vtkUnstructuredGrid *grid);

    // void setMeshOpacity(double opacity);

    /**
     * @brief 设置网格颜色渐变
     * @param progress 显示进度, 0.0-1.0
     * @note 用于导入动画, 网格颜色渐变显示
     */
    void setMeshRevealProgress(double progress);

    /**
     * @brief 设置沿网格边线传播的波纹相位，0 和 1 对应同一帧
     */
    void setEdgeWavePhase(double phase, double strength = 1.0);

    /**
     * @brief 清除场景
     */
    void clear();

    /**
     * @brief 重置相机
     */
    void resetCamera();

    /**
     * @brief 清除高亮
     */
    void clearHighlight();

    /**
     * @brief 选择单元
     * @param x 鼠标x坐标
     * @param y 鼠标y坐标
     * @return 选中的单元的有限元ID, 未选中返回空
     */
    std::optional<MeshId> pickElement(int x, int y);

    /**
     * @brief 选择节点
     * @param x 鼠标x坐标
     * @param y 鼠标y坐标
     * @return 选中的节点的有限元ID, 未选中返回空
     */
    std::optional<MeshId> pickNode(int x, int y);

private:
    /**
     * @brief 高亮单元
     * @param cellId 单元的vtk数据ID
     */
    void highlightCell(vtkIdType cellId);

    /**
     * @brief 高亮节点
     * @param pointId 节点的vtk数据ID
     */
    void highlightNode(vtkIdType pointId);

    /**
     * @brief 构建边线波纹
     */
    void buildEdgeWave();

private:
    /// @brief 渲染器
    vtkSmartPointer<vtkRenderer> renderer_;
    /// @brief 场景对象
    vtkSmartPointer<vtkActor> meshActor_;
    /// @brief 网格
    vtkSmartPointer<vtkUnstructuredGrid> grid_;
    /// @brief 高亮对象
    /// @details 用于高亮单元或节点, 选中后显示高亮框
    vtkSmartPointer<vtkActor> highlightActor_;
    /// @brief 边线对象
    vtkSmartPointer<vtkActor> edgeActor_;
    /// @brief 边线波纹对象
    vtkSmartPointer<vtkActor> edgeWaveActor_;
};
