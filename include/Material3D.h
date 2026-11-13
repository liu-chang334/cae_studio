#pragma once
#include "Matrix.h"

class Material3D {
public:
    virtual ~Material3D() = default;
    virtual const linalg::Matrix<6, 6> &initialElasticMatrix() const = 0;
};