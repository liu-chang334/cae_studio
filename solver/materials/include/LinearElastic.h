#pragma once

#include "Material3D.h"
#include "MaterialData.h"
#include "Matrix.h"

/**
 * @brief 计算各向同性线弹性本构矩阵
 * @param material 各向同性线弹性材料数据
 * @return 各向同性线弹性本构矩阵
 */
linalg::Matrix<6, 6> computeIsotropicElasticMatrix(const IsotropicElasticData &material);

class LinearElasticMaterial : public Material3D {
public:
    explicit LinearElasticMaterial(const IsotropicElasticData &data)
        : data_(data), elasticMatrix_(computeIsotropicElasticMatrix(data_)) {}

    double youngModulus() const { return data_.youngModulus; }
    double poissonRatio() const { return data_.poissonRatio; }
    const linalg::Matrix<6, 6> &initialElasticMatrix() const { return elasticMatrix_; }

private:
    IsotropicElasticData data_;
    linalg::Matrix<6, 6> elasticMatrix_;
};