#pragma once

#include <array>
#include <cstddef>

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
} // namespace linalg
