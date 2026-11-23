#pragma once

#include "Matrix.h"

#include <optional>
#include <vector>

struct ConstrainedLinearSystem {
    linalg::MatrixX originalCoefficientMatrix;  // 施加指定位移约束前的完整 A
    linalg::VectorX originalRhs;  // 施加指定位移约束前的完整 b

    std::vector<std::optional<double>> prescribedValues;  // 待施加的位移边界条件
};