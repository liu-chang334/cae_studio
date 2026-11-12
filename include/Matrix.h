#pragma once

#include <array>

namespace linalg {
/**
 * @brief 矩阵模板
 * @tparam Rows 行数
 * @tparam Cols 列数
 * @details 用于表示任意维度的矩阵, 例如 3x4 矩阵为 Matrix<3, 4>
 */
template <std::size_t Rows, std::size_t Cols> using Matrix = std::array<std::array<double, Cols>, Rows>;

/**
 * @brief 向量模板
 * @tparam Rows 维数
 * @details 用于表示任意维度的向量, 例如 3 维向量为 Vector<3>
 */
template <std::size_t Rows> using Vector = std::array<double, Rows>;

/**
 * @brief 矩阵乘法
 * @param left 左矩阵
 * @param right 右矩阵
 * @return 矩阵乘法结果
 */
template <std::size_t Rows, std::size_t Inner, std::size_t Cols>
Matrix<Rows, Cols> multiply(const Matrix<Rows, Inner> &left, const Matrix<Inner, Cols> &right) {
    Matrix<Rows, Cols> result{};
    for (std::size_t row = 0; row < Rows; ++row) {
        for (std::size_t col = 0; col < Cols; ++col) {
            for (std::size_t k = 0; k < Inner; ++k) {
                result[row][col] += left[row][k] * right[k][col];
            }
        }
    }
    return result;
}

/**
 * @brief 矩阵转置
 * @param matrix 矩阵
 * @return 矩阵的置矩阵
 */
template <std::size_t Rows, std::size_t Cols> Matrix<Cols, Rows> transpose(const Matrix<Rows, Cols> &matrix) {
    Matrix<Cols, Rows> result{};
    for (std::size_t row = 0; row < Rows; ++row) {
        for (std::size_t col = 0; col < Cols; ++col) {
            result[col][row] = matrix[row][col];
        }
    }
    return result;
}

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