#pragma once

#include "MeshData.h"

#include <string>
#include <variant>

using NodeTarget = std::variant<std::string, MeshId>;
