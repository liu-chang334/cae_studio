#pragma once
#include "ElementState.h"
#include "Matrix.h"

#include <optional>

class Material3D {
public:
    virtual ~Material3D() = default;

    /// @brief 获取初始弹性矩阵
    virtual const linalg::Matrix<6, 6> &initialElasticMatrix() const = 0;

    /// @brief 返回当前增量的试算状态, 每次迭代都从同一份已提交状态开始
    /// @details 实现必须同时更新 fields、internalVariables 和 tangent（应力对工程总应变的算法一致切线）。
    virtual void integrate(const MaterialPointState &committed, const linalg::Vector<6> &totalStrain, double timeIncrement,
                           MaterialPointState &trial) const = 0;
    std::optional<double> density() const { return density_; }

protected:
    explicit Material3D(std::optional<double> density = std::nullopt) : density_(density) {}

private:
    std::optional<double> density_;
};
