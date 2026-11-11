#pragma once

#include "Matrix.h"
#include "SolidElement3D.h"

#include <array>

class C3D8Element : public SolidElement3D {
public:
    C3D8Element(MeshId id, std::vector<MeshNode> nodes, std::size_t materialIndex);

    double volume() const { return volume_; }

private:
    /**
     * @brief 积分点几何信息
     * @details 包含B矩阵(Bbar)和积分体积因子(detJ x 积分权重)
     */
    struct IntegrationPointGeometry {
        linalg::Matrix<6, 24> Bbar{};
        double integrationFactor = 0.0;
    };

    /**
     * @brief 获取节点坐标矩阵
     * @details 返回单元的节点坐标矩阵
     */
    linalg::Matrix<8, 3> nodeCoordinates() const;

    /**
     * @brief 形函数对自然坐标的导数
     * @param naturalCoordinates 自然坐标
     * @return 当前积分点处形函数对自然坐标的导数矩阵
     * @details 静态方法, 可不实例化调用
     */
    static linalg::Matrix<3, 8> computeShapeGradientsReference(const linalg::Vector<3> &naturalCoordinates);

    /**
     * @brief 雅可比矩阵,
     * @param referenceGradients 形函数对自然坐标的导数矩阵
     * @param coordinates 节点坐标矩阵
     * @return 当前积分点处的雅可比矩阵, 第i行是三个物理坐标依次对第i个自然坐标的偏导
     * @details 静态方法, 可不实例化调用
     */
    static linalg::Matrix<3, 3> computeJacobian(const linalg::Matrix<3, 8> &referenceGradients,
                                                const linalg::Matrix<8, 3> &coordinates);

    /**
     * @brief 形函数对物理坐标的导数
     * @param referenceGradients 形函数对自然坐标的导数矩阵
     * @param jacobian 雅可比矩阵
     * @return 当前积分点处形函数对物理坐标的导数矩阵
     * @details 静态方法, 可不实例化调用
     */
    static linalg::Matrix<3, 8> computeShapeGradientsGlobal(const linalg::Matrix<3, 8> &referenceGradients,
                                                            const linalg::Matrix<3, 3> &jacobian);

    /**
     * @brief 计算应变矩阵B
     * @param globalGradients 形函数对物理坐标的导数矩阵
     * @return 当前积分点处的应变矩阵B
     * @details 静态方法, 可不实例化调用
     */
    static linalg::Matrix<6, 24> computeB(const linalg::Matrix<3, 8> &globalGradients);

    /**
     * @brief 计算应变矩阵B的体积部分
     */
    static linalg::Matrix<6, 24> computeVolumetricB(const linalg::Matrix<6, 24> &B);

    /**
     * @brief 初始化各积分点的Bbar, 积分因子和单元体积
     */
    void initializeGeometry();

private:
    std::array<IntegrationPointGeometry, 8> geometry_{};
    double volume_ = 0.0;
};