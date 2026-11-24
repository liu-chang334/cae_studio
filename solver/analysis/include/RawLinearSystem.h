#pragma once

#include "Matrix.h"

#include <optional>
#include <vector>

// 保存施加位移边界条件前的完整线性系统及各自由度的指定位移。
struct RawLinearSystem {
    linalg::MatrixX originalCoefficientMatrix;  // 施加指定位移约束前的完整 A
    linalg::VectorX originalRhs;  // 施加指定位移约束前的完整 b

    std::vector<std::optional<double>> prescribedValues;  // 待施加的位移边界条件
};
