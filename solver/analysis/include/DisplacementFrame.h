#pragma once

#include "Matrix.h"

struct DisplacementFrame {
    double time = 0.0;
    linalg::VectorX displacements;
};
