#pragma once

#include "FEModel.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief 单元拓扑和分析类型以及单元节点数
 */
struct ElementSectionInfo {
    CellTopology topology = CellTopology::Unknown;
    ElementFormulation formulation = ElementFormulation::Unspecified;
    std::size_t nodeCount = 0;
};

/**
 * @brief 保存读取Abaqus inp文件的结果
 * @details 包含网格数据, 错误信息, 警告信息
 * @note 通过success()方法判断是否成功读取文件
 */
struct AbaqusReadResult {
    FEModel model;
    std::string error;
    std::vector<std::string> warnings;

    bool success() const { return error.empty() && !model.mesh.nodes.empty() && !model.mesh.elements.empty(); }
};

/**
 * @brief 读取Abaqus inp文件
 * @details 通过read()方法读取文件, 并返回读取结果
 */
class AbaqusInpReader {
public:
    /**
     * @brief 读取Abaqus inp文件
     * @param path 文件路径
     * @return 读取结果, 包含网格数据, 错误信息, 警告信息
     */
    AbaqusReadResult read(const std::filesystem::path &path) const;
};