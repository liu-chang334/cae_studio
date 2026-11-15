#pragma once

#include <array>
#include <cassert>
#include <vector>

namespace linalg {
/**
 * @brief 静态矩阵模板
 * @tparam Rows 行数
 * @tparam Cols 列数
 * @details 用于表示任意维度的矩阵, 例如 3x4 矩阵为 Matrix<3, 4>
 * @note 基于std::array, 维度必须在编译期确定
 */
template <std::size_t Rows, std::size_t Cols> using Matrix = std::array<std::array<double, Cols>, Rows>;

/**
 * @brief 静态向量模板
 * @tparam Rows 维数
 * @details 用于表示任意维度的向量, 例如 3 维向量为 Vector<3>
 * @note 基于std::array, 维度必须在编译期确定
 */
template <std::size_t Rows> using Vector = std::array<double, Rows>;

/**
 * @brief 动态矩阵类
 * @details 用于表示任意维度的矩阵, 例如 3x4 矩阵为 MatrixX(3, 4)
 * @note 基于std::vector, 维度可在运行时确定
 */
class MatrixX {
public:
    MatrixX(std::size_t rows, std::size_t cols);
    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    double &operator()(std::size_t row, std::size_t col) {
        assert(row < rows_ && col < cols_);
        return values_[row * cols_ + col];
    }
    const double &operator()(std::size_t row, std::size_t col) const {
        assert(row < rows_ && col < cols_);
        return values_[row * cols_ + col];
    }

private:
    std::size_t rows_;
    std::size_t cols_;
    std::vector<double> values_;
};

/**
 * @brief 动态向量类型
 */
using VectorX = std::vector<double>;

/**
 * @brief 静态矩阵乘法
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
 * @brief 静态矩阵转置
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
Matrix<3, 3> adjugate_3x3(const Matrix<3, 3> &matrix);

/**
 * @brief 计算3x3矩阵的行列式
 * @param matrix 3x3矩阵
 * @return 3x3矩阵的行列式
 */
double determinant_3x3(const Matrix<3, 3> &matrix);

/**
 * @brief 计算3x3矩阵的逆矩阵
 * @param matrix 3x3矩阵
 * @return 3x3矩阵的逆矩阵
 */
Matrix<3, 3> inverse_3x3(const Matrix<3, 3> &matrix);
} // namespace linalg