#include "AbaqusInpReader.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum Section { None, Node, Element, NodeSet, Load, ConcentratedLoad, Boundary, Elastic };
enum class StepState { ModelData, NeedsProcedure, InStep, Ended };

struct SectionAssignment {
    std::string setName;
    std::string materialName;
    std::size_t lineNumber;
};

struct BoundaryRow {
    std::vector<std::string> fields;
    std::size_t lineNumber;
};

std::string trim(const std::string &text) {
    const char *whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return {};
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::string toUpperAscii(std::string text) {
    for (char &ch : text) {
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
    }
    return text;
}

std::vector<std::string> splitComma(const std::string &line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string::npos) {
            fields.push_back(trim(line.substr(start)));
            return fields;
        }
        fields.push_back(trim(line.substr(start, comma - start)));
        start = comma + 1;
    }
}

std::string keywordValue(const std::vector<std::string> &fields, const std::string &name) {
    for (std::size_t i = 1; i < fields.size(); ++i) {
        const std::size_t equal = fields[i].find('=');
        if (equal == std::string::npos)
            continue;
        const std::string key = toUpperAscii(trim(fields[i].substr(0, equal)));
        if (key == name)
            return trim(fields[i].substr(equal + 1));
    }
    return {};
}

bool hasKeywordFlag(const std::vector<std::string> &fields, const std::string &flag) {
    for (std::size_t i = 1; i < fields.size(); ++i) {
        if (toUpperAscii(fields[i]) == flag)
            return true;
    }
    return false;
}

bool parseId(const std::string &field, MeshId &value) {
    std::istringstream stream(field);
    stream.imbue(std::locale::classic());
    return (stream >> value) && stream.eof();
}

bool parseDouble(const std::string &field, double &value) {
    std::istringstream stream(field);
    stream.imbue(std::locale::classic());
    return (stream >> value) && stream.eof() && std::isfinite(value);
}

bool parseDof(const std::string &field, int &dof) {
    MeshId value{};
    if (!parseId(field, value) || value < 1 || value > 3)
        return false;
    dof = static_cast<int>(value);
    return true;
}

NodeTarget parseNodeTarget(const std::string &field) {
    MeshId nodeId{};
    if (parseId(field, nodeId))
        return nodeId;
    return field;
}

bool appendBoundary(const std::vector<std::string> &fields, bool legacyFormat, FEModel &model) {
    int firstDof{};
    if (fields.size() < 2 || fields.size() > 4 || fields[0].empty() || !parseDof(fields[1], firstDof))
        return false;

    double displacement = 0.0;
    if (legacyFormat && (fields.size() == 3 || (fields.size() == 4 && fields[3].empty()))) {
        if (!parseDouble(fields[2], displacement))
            return false;
        model.boundaries.push_back({fields[0], firstDof, displacement});
        return true;
    }

    int lastDof = firstDof;
    if (fields.size() >= 3 && !fields[2].empty() && (!parseDof(fields[2], lastDof) || lastDof < firstDof))
        return false;
    if (fields.size() == 4 && !fields[3].empty() && !parseDouble(fields[3], displacement))
        return false;

    const auto target = parseNodeTarget(fields[0]);
    for (int dof = firstDof; dof <= lastDof; ++dof)
        model.boundaries.push_back({target, dof, displacement});
    return true;
}

std::string lineMessage(std::size_t lineNumber, const std::string &message) {
    return "第 " + std::to_string(lineNumber) + " 行：" + message;
}

AbaqusReadResult AbaqusInpReader::read(const std::filesystem::path &path) const {
    AbaqusReadResult result;
    auto &model = result.model;
    std::ifstream input(path);
    if (!input) {
        result.error = "无法打开文件: " + path.u8string();
        return result;
    }
    Section section = Section::None;
    ElementSectionInfo currentElement;
    std::string currentNodeSetName;
    std::string currentElementSet;
    std::vector<SectionAssignment> sectionAssignments;
    std::vector<BoundaryRow> boundaryRows;
    std::unordered_map<std::string, std::string> nodeSetNames;
    StepState stepState = StepState::ModelData;
    std::size_t stepLine = 0;
    bool nonlinearStep = false;
    bool perturbationStep = false;
    bool hasConcentratedLoad = false;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line.compare(0, 2, "**") == 0)
            continue;
        if (line.front() == '*') {
            const std::vector<std::string> fields = splitComma(line);
            const std::string keyword = toUpperAscii(fields[0]);
            section = Section::None;
            currentNodeSetName.clear();
            currentElementSet.clear();
            if (stepState == StepState::NeedsProcedure) {
                if (keyword == "*STATIC") {
                    if (nonlinearStep || perturbationStep || fields.size() != 1) {
                        result.error = lineMessage(lineNumber, "当前仅支持小变形静力分析，暂不支持该 *STEP 或 *STATIC 选项");
                        return result;
                    }
                    model.analysisType = AnalysisType::Static;
                } else if (keyword == "*DYNAMIC" && hasKeywordFlag(fields, "EXPLICIT")) {
                    model.analysisType = AnalysisType::ExplicitDynamics;
                } else {
                    result.error = lineMessage(lineNumber, "*STEP 后需要 *STATIC 或 *DYNAMIC, EXPLICIT，当前为 " + keyword);
                    return result;
                }
                stepState = StepState::InStep;
            } else if (keyword == "*STEP") {
                if (stepState != StepState::ModelData) {
                    result.error = lineMessage(lineNumber, "当前仅支持一个分析步");
                    return result;
                }
                stepLine = lineNumber;
                stepState = StepState::NeedsProcedure;
                nonlinearStep = hasKeywordFlag(fields, "NLGEOM") || toUpperAscii(keywordValue(fields, "NLGEOM")) == "YES";
                perturbationStep = hasKeywordFlag(fields, "PERTURBATION");
            } else if (keyword == "*END STEP") {
                if (stepState != StepState::InStep) {
                    result.error = lineMessage(lineNumber, "*END STEP 缺少对应的 *STEP");
                    return result;
                }
                stepState = StepState::Ended;
            } else if (keyword == "*STATIC" || keyword == "*DYNAMIC") {
                result.error = lineMessage(lineNumber, "分析过程卡片必须紧跟 *STEP，每个分析步只能定义一次分析过程");
                return result;
            } else if (keyword == "*NODE") {
                section = Section::Node;
            } else if (keyword == "*ELEMENT") {
                const std::string type = toUpperAscii(keywordValue(fields, "TYPE"));
                if (type.empty()) {
                    result.error = lineMessage(lineNumber, "*ELEMENT 缺少 TYPE");
                    return result;
                }
                if (type == "C3D8")
                    currentElement = {CellTopology::HEXAHEDRON, ElementFormulation::C3D8, 8};
                else if (type == "C3D8R")
                    currentElement = {CellTopology::HEXAHEDRON, ElementFormulation::C3D8R, 8};
                else {
                    result.warnings.push_back(lineMessage(lineNumber, "暂不支持单元 TYPE=" + type + "，已跳过该段"));
                    continue;
                }
                currentElementSet = toUpperAscii(keywordValue(fields, "ELSET"));
                section = Section::Element;
            } else if (keyword == "*NSET") {
                for (std::size_t i = 1; i < fields.size(); ++i) {
                    if (toUpperAscii(fields[i]) == "GENERATE") {
                        result.error = lineMessage(lineNumber, "暂不支持节点集 GENERATE");
                        return result;
                    }
                }
                currentNodeSetName = keywordValue(fields, "NSET");
                if (currentNodeSetName.empty()) {
                    result.error = lineMessage(lineNumber, "*NSET 缺少 NSET 名称");
                    return result;
                }
                currentNodeSetName = nodeSetNames.try_emplace(toUpperAscii(currentNodeSetName), currentNodeSetName).first->second;
                model.nodeSets.try_emplace(currentNodeSetName);
                section = Section::NodeSet;
            } else if (keyword == "*LOAD") {
                section = Section::Load;
            } else if (keyword == "*CLOAD") {
                if (fields.size() != 1) {
                    result.error = lineMessage(lineNumber, "暂不支持 *CLOAD 的关键字参数");
                    return result;
                }
                hasConcentratedLoad = true;
                section = Section::ConcentratedLoad;
            } else if (keyword == "*BOUNDARY") {
                if (fields.size() != 1) {
                    result.error = lineMessage(lineNumber, "暂不支持 *BOUNDARY 的关键字参数");
                    return result;
                }
                section = Section::Boundary;
            } else if (keyword == "*SOLID SECTION") {
                const std::string setName = keywordValue(fields, "ELSET");
                const std::string materialName = keywordValue(fields, "MATERIAL");
                if (setName.empty() || materialName.empty()) {
                    result.error = lineMessage(lineNumber, "*SOLID SECTION 需要 ELSET 和 MATERIAL");
                    return result;
                }
                sectionAssignments.push_back({setName, materialName, lineNumber});
            } else if (keyword == "*MATERIAL") {
                MaterialData material;
                material.name = keywordValue(fields, "NAME");
                if (material.name.empty()) {
                    result.error = lineMessage(lineNumber, "*MATERIAL 缺少 NAME");
                    return result;
                }
                model.materials.push_back(std::move(material));
            } else if (keyword == "*ELASTIC") {
                if (model.materials.empty()) {
                    result.error = lineMessage(lineNumber, "*ELASTIC 前缺少 *MATERIAL");
                    return result;
                }
                section = Section::Elastic;
            }
            continue;
        }
        if (section == Section::None)
            continue;
        std::vector<std::string> parts = splitComma(line);

        if (section == Section::Node) {
            MeshNode node{};
            if (parts.size() < 4 || !parseId(parts[0], node.id) || !parseDouble(parts[1], node.coordinates[0]) ||
                !parseDouble(parts[2], node.coordinates[1]) || !parseDouble(parts[3], node.coordinates[2])) {
                result.error = lineMessage(lineNumber, "节点数据应为 nid, x, y, z");
                return result;
            }
            model.mesh.nodes.push_back(node);
        } else if (section == Section::Element) {
            MeshElement element{};
            if (parts.size() != 1 + currentElement.nodeCount || !parseId(parts[0], element.id)) {
                result.error = lineMessage(lineNumber, "单元编号或连接表字段数量有误");
                return result;
            }
            element.topology = currentElement.topology;
            element.formulation = currentElement.formulation;
            element.nodeIds.reserve(currentElement.nodeCount);
            for (std::size_t i = 0; i < currentElement.nodeCount; ++i) {
                MeshId nodeId{};
                if (!parseId(parts[i + 1], nodeId)) {
                    result.error = lineMessage(lineNumber, "单元 " + std::to_string(element.id) + " 的第 " + std::to_string(i + 1) +
                                                               " 个节点编号无效");
                    return result;
                }
                element.nodeIds.push_back(nodeId);
            }
            if (!currentElementSet.empty())
                model.elemSets[currentElementSet].push_back(element.id);
            model.mesh.elements.push_back(std::move(element));
        } else if (section == Section::NodeSet) {
            if (parts.back().empty())
                parts.pop_back();
            std::vector<MeshId> &nodeIds = model.nodeSets.at(currentNodeSetName);
            for (const std::string &field : parts) {
                MeshId nodeId{};
                if (!parseId(field, nodeId)) {
                    result.error = lineMessage(lineNumber, "节点集 " + currentNodeSetName + " 中的节点编号无效");
                    return result;
                }
                nodeIds.push_back(nodeId);
            }
        } else if (section == Section::Load || section == Section::ConcentratedLoad) {
            if (parts.back().empty())
                parts.pop_back();
            int dof{};
            double value{};
            if (parts.size() != 3 || parts[0].empty() || !parseDof(parts[1], dof) || !parseDouble(parts[2], value)) {
                result.error = lineMessage(lineNumber, "集中载荷应为 节点编号或节点集名称, 自由度(1–3), 有限数值");
                return result;
            }
            auto target = section == Section::Load ? NodeTarget{parts[0]} : parseNodeTarget(parts[0]);
            model.loads.push_back({std::move(target), dof, value});
        } else if (section == Section::Boundary) {
            boundaryRows.push_back({std::move(parts), lineNumber});
        } else if (section == Section::Elastic) {
            IsotropicElasticData elastic;
            if (parts.size() < 2 || !parseDouble(parts[0], elastic.youngModulus) || !parseDouble(parts[1], elastic.poissonRatio)) {
                result.error = lineMessage(lineNumber, "*ELASTIC 需要有限的 E 和泊松比");
                return result;
            }
            model.materials.back().elastic = elastic;
        }
    }
    if (input.bad() || !input.eof()) {
        result.error = "读取文件失败：" + path.u8string();
        return result;
    }
    if (stepState == StepState::NeedsProcedure || stepState == StepState::InStep) {
        result.error = lineMessage(stepLine, stepState == StepState::NeedsProcedure ? "*STEP 缺少分析过程卡片" : "*STEP 缺少 *END STEP");
        return result;
    }
    if (model.mesh.nodes.empty() || model.mesh.elements.empty()) {
        result.error = "未读取到有效网格";
        return result;
    }
    const bool legacyBoundaryFormat = stepLine == 0 && !hasConcentratedLoad;
    for (const auto &row : boundaryRows) {
        if (!appendBoundary(row.fields, legacyBoundaryFormat, model)) {
            result.error = lineMessage(row.lineNumber, "*BOUNDARY 需要 节点编号或节点集名称, 起始自由度, 结束自由度, 位移值（自由度范围为 1–3）");
            return result;
        }
    }

    std::unordered_set<MeshId> nodeIds;
    nodeIds.reserve(model.mesh.nodes.size());
    for (const auto &node : model.mesh.nodes)
        nodeIds.insert(node.id);

    const auto validateTarget = [&](NodeTarget &target, const std::string &conditionName) {
        if (const auto *nodeId = std::get_if<MeshId>(&target)) {
            if (*nodeId > 0 && nodeIds.find(*nodeId) != nodeIds.end())
                return true;
            result.error = conditionName + "引用的节点不存在：" + std::to_string(*nodeId);
            return false;
        }
        auto &name = std::get<std::string>(target);
        const auto setName = nodeSetNames.find(toUpperAscii(name));
        if (setName == nodeSetNames.end() || model.nodeSets.at(setName->second).empty()) {
            result.error = conditionName + "引用的节点集不存在或为空：" + name;
            return false;
        }
        name = setName->second;
        return true;
    };
    for (auto &load : model.loads) {
        if (!validateTarget(load.target, "载荷"))
            return result;
    }
    for (auto &boundary : model.boundaries) {
        if (!validateTarget(boundary.target, "边界条件"))
            return result;
    }
    std::unordered_map<std::string, std::string> materialNames;
    for (const MaterialData &material : model.materials) {
        if (!materialNames.emplace(toUpperAscii(material.name), material.name).second) {
            result.error = "材料名称重复：" + material.name;
            return result;
        }
    }
    for (const SectionAssignment &assignment : sectionAssignments) {
        const auto set = model.elemSets.find(toUpperAscii(assignment.setName));
        if (set == model.elemSets.end() || set->second.empty()) {
            result.error = lineMessage(assignment.lineNumber, "单元集不存在或为空：" + assignment.setName);
            return result;
        }
        const auto material = materialNames.find(toUpperAscii(assignment.materialName));
        if (material == materialNames.end()) {
            result.error = lineMessage(assignment.lineNumber, "材料未定义：" + assignment.materialName);
            return result;
        }
        for (MeshId elementId : set->second) {
            const auto inserted = model.materialNameByElementId.emplace(elementId, material->second);
            if (!inserted.second && inserted.first->second != material->second) {
                result.error = lineMessage(assignment.lineNumber, "单元 " + std::to_string(elementId) + " 被分配了不同的材料");
                return result;
            }
        }
    }
    return result;
}
