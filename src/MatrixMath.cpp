#include "MatrixMath.h"

namespace linalg {
Matrix<3, 3> adjugate(const Matrix<3, 3> &matrix) {
    const auto &[a00, a01, a02] = matrix[0];
    const auto &[a10, a11, a12] = matrix[1];
    const auto &[a20, a21, a22] = matrix[2];
    // 01. 代数余子式矩阵
    const Matrix<3, 3> cofactors{{{a11 * a22 - a12 * a21, a12 * a20 - a10 * a22, a10 * a21 - a11 * a20},
                                  {a02 * a21 - a01 * a22, a00 * a22 - a02 * a20, a01 * a20 - a00 * a21},
                                  {a01 * a12 - a02 * a11, a02 * a10 - a00 * a12, a00 * a11 - a01 * a10}}};
    // 02. 伴随矩阵是代数余子式矩阵的转置
    Matrix<3, 3> result{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col)
            result[row][col] = cofactors[col][row];
    }
    return result;
}

double determinant(const Matrix<3, 3> &matrix) {
    const auto &[a00, a01, a02] = matrix[0];
    const auto &[a10, a11, a12] = matrix[1];
    const auto &[a20, a21, a22] = matrix[2];
    // 01. 计算第一行的三个代数余子式
    const double c00 = a11 * a22 - a12 * a21;
    const double c01 = a12 * a20 - a10 * a22;
    const double c02 = a10 * a21 - a11 * a20;
    // 02. 与第一行的元素对应相乘并求和
    return a00 * c00 + a01 * c01 + a02 * c02;
}

Matrix<3, 3> inverse(const Matrix<3, 3> &matrix) {
    const double det = determinant(matrix);
    Matrix<3, 3> result = adjugate(matrix);
    for (auto &row : result) {
        for (double &value : row)
            value /= det;
    }
    return result;
}
} // namespace linalg