#pragma once

#include "Matrix.h"

struct IsotropicElasticData;

/**
 * @brief 计算各向同性线弹性本构矩阵
 * @param material 各向同性线弹性材料数据
 * @return 各向同性线弹性本构矩阵
 */
linalg::Matrix<6, 6> computeIsotropicElasticMatrix(const IsotropicElasticData &material);