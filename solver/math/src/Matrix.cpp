#include "Matrix.h"

#include <cmath>
#include <stdexcept>

namespace linalg {
MatrixX::MatrixX(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols) {
    if (cols_ != 0 && rows_ > values_.max_size() / cols_)
        throw std::length_error("MatrixX dimensions are too large");
    values_.resize(rows_ * cols_, 0.0);
}

Matrix<3, 3> adjugate_3x3(const Matrix<3, 3> &matrix) {
    const auto &[a00, a01, a02] = matrix[0];
    const auto &[a10, a11, a12] = matrix[1];
    const auto &[a20, a21, a22] = matrix[2];
    // 01. 代数余子式矩阵
    const Matrix<3, 3> cofactors{{{a11 * a22 - a12 * a21, a12 * a20 - a10 * a22, a10 * a21 - a11 * a20},
                                  {a02 * a21 - a01 * a22, a00 * a22 - a02 * a20, a01 * a20 - a00 * a21},
                                  {a01 * a12 - a02 * a11, a02 * a10 - a00 * a12, a00 * a11 - a01 * a10}}};
    // 02. 伴随矩阵是代数余子式矩阵的转置
    Matrix<3, 3> result = transpose(cofactors);
    return result;
}

double determinant_3x3(const Matrix<3, 3> &matrix) {
    const auto &[a00, a01, a02] = matrix[0];
    const auto &[a10, a11, a12] = matrix[1];
    const auto &[a20, a21, a22] = matrix[2];
    // 01. 计算第一行的三个代数余子式
    const double c00 = a11 * a22 - a12 * a21;
    const double c01 = a12 * a20 - a10 * a22;
    const double c02 = a10 * a21 - a11 * a20;
    // 02. 与第一行的元素对应相乘并求和
    double det = a00 * c00 + a01 * c01 + a02 * c02;
    return det;
}

Matrix<3, 3> inverse_3x3(const Matrix<3, 3> &matrix) {
    const double det = determinant_3x3(matrix);
    if (!std::isfinite(det) || det == 0.0)  // 检查行列式是否为0或非有限数
        throw std::domain_error("Matrix determinant is zero or non-finite");
    Matrix<3, 3> result = adjugate_3x3(matrix);
    for (auto &row : result) {
        for (double &value : row) {
            value /= det;
            if (!std::isfinite(value))  // 检查逆矩阵是否包含非有限数
                throw std::overflow_error("Inverse contains a non-finite value");
        }
    }
    return result;
}

VectorX solveCholesky(const MatrixX &A, const VectorX &rhs) {
    const auto n = A.rows();
    if (A.cols() != n || rhs.size() != n)
        throw std::invalid_argument("Cholesky system dimensions do not match");
    MatrixX L(n, n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double value = A(i, j);
            for (std::size_t k = 0; k < j; ++k)
                value -= L(i, k) * L(j, k);
            if (i == j) {
                if (!std::isfinite(value) || value <= 0.0)
                    throw std::domain_error("Cholesky requires positive finite pivots");
                L(i, i) = std::sqrt(value);
            } else
                L(i, j) = value / L(j, j);
        }
    }
    VectorX y(n);
    for (std::size_t i = 0; i < n; ++i) {
        double value = rhs[i];
        for (std::size_t j = 0; j < i; ++j)
            value -= L(i, j) * y[j];
        y[i] = value / L(i, i);
    }

    VectorX x(n);
    for (std::size_t i = n; i > 0; --i) {
        const auto row = i - 1;
        double value = y[row];
        for (std::size_t j = row + 1; j < n; ++j)
            value -= L(j, row) * x[j];  // L^T(row, j) = L(j, row)
        x[row] = value / L(row, row);
    }
    return x;
}
}  // namespace linalg