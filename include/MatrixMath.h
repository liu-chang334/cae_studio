#pragma once

#include "Matrix.h"

namespace linalg {
/**
 * @brief 计算3x3矩阵的伴随矩阵
 * @param matrix 3x3矩阵
 * @return 3x3伴随矩阵
 */
Matrix<3, 3> adjugate(const Matrix<3, 3> &matrix);

/**
 * @brief 计算3x3矩阵的行列式
 * @param matrix 3x3矩阵
 * @return 3x3矩阵的行列式
 */
double determinant(const Matrix<3, 3> &matrix);

/**
 * @brief 计算3x3矩阵的逆矩阵
 * @param matrix 3x3矩阵
 * @return 3x3矩阵的逆矩阵
 */
Matrix<3, 3> inverse(const Matrix<3, 3> &matrix);
} // namespace linalg