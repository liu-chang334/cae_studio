#include "AnalysisModel.h"
#include "C3D8Element.h"
#include "FEModel.h"
#include "LinearElastic.h"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

struct Entry {
    std::unique_ptr<SolidElement3D> element; // 单元指针
    std::optional<MeshId> sourceId;          // 源网格或源节点id
};

std::size_t AnalysisModel::addElement(std::unique_ptr<SolidElement3D> element, std::optional<MeshId> sourceId) {
    if (!element)
        throw std::invalid_argument("Cannot add a null element");
    elements_.push_back({std::move(element), sourceId});
    return elements_.size();
}

void AnalysisModel::removeElement(std::size_t number) {
    const auto index = static_cast<std::ptrdiff_t>(indexOf(number));
    elements_.erase(elements_.begin() + index);
}

std::size_t AnalysisModel::elementCount() const { return elements_.size(); }

const SolidElement3D &AnalysisModel::element(std::size_t number) const { return *elements_[indexOf(number)].element; }

std::optional<MeshId> AnalysisModel::sourceElementId(std::size_t number) const { return elements_[indexOf(number)].sourceId; }

std::size_t AnalysisModel::indexOf(std::size_t number) const {
    if (number == 0 || number > elements_.size())
        throw std::out_of_range("Invalid element number");
    return number - 1;
}

std::shared_ptr<const Material3D> resolveMaterial(MeshId elementId, const FEModel &model,
                                                  const std::unordered_map<std::string, const MaterialData *> &materialsByName,
                                                  std::unordered_map<std::string, std::shared_ptr<const Material3D>> &materialCache);

AnalysisModel buildAnalysisModel(const FEModel &model) {
    // 01. 构建节点源id->节点地址映射表
    std::unordered_map<MeshId, const MeshNode *> nodesById;
    nodesById.reserve(model.mesh.nodes.size());
    for (const auto &node : model.mesh.nodes) {
        if (!nodesById.emplace(node.id, &node).second) // 检查节点id是否重复
            throw std::invalid_argument("Duplicate node ID: " + std::to_string(node.id));
    }
    // 02. 构建材料名->材料地址映射表
    std::unordered_map<std::string, const MaterialData *> materialsByName;
    for (const auto &data : model.materials) {
        if (!materialsByName.emplace(data.name, &data).second) // 检查材料名是否重复
            throw std::invalid_argument("Duplicate material name: " + data.name);
    }
    // 03. 构建材料缓存表(首次使用才创建)
    std::unordered_map<std::string, std::shared_ptr<const Material3D>> materialCache;
    AnalysisModel analysis;
    std::unordered_set<MeshId> elementIds;
    elementIds.reserve(model.mesh.elements.size());
    // 04. 根据网格记录创建计算单元, 保留原始单元id
    for (const MeshElement &record : model.mesh.elements) {
        const std::string label = "Element" + std::to_string(record.id);
        if (!elementIds.insert(record.id).second) // 检查单元id是否重复
            throw std::invalid_argument("Duplicate element ID: " + std::to_string(record.id));
        switch (record.formulation) {
        case ElementFormulation::C3D8: {
            std::vector<MeshNode> nodes;
            nodes.reserve(record.nodeIds.size());
            for (MeshId nodeId : record.nodeIds) {
                const auto node = nodesById.find(nodeId);
                if (node == nodesById.end())
                    throw std::invalid_argument(label + " references unknown node " + std::to_string(nodeId));
                nodes.push_back(*(node->second));
            }
            // 05. 查找材料
            std::shared_ptr<const Material3D> material = resolveMaterial(record.id, model, materialsByName, materialCache);
            // 06. 创建计算单元
            try {
                std::unique_ptr<SolidElement3D> element = std::make_unique<C3D8Element>(std::move(nodes), std::move(material));
                analysis.addElement(std::move(element), record.id);
            } catch (const std::domain_error &error) {
                throw std::domain_error(label + ": " + error.what());
            }
            break;
        }
        case ElementFormulation::C3D8R:
            throw std::invalid_argument(label + ": C3D8R analysis is not supported");
        case ElementFormulation::Unspecified:
            throw std::invalid_argument(label + " has no element formulation");
        default:
            throw std::invalid_argument(label + " has an unsupported formulation");
        }
    }
    return analysis;
}

std::shared_ptr<const Material3D> resolveMaterial(MeshId elementId, const FEModel &model,
                                                  const std::unordered_map<std::string, const MaterialData *> &materialsByName,
                                                  std::unordered_map<std::string, std::shared_ptr<const Material3D>> &materialCache) {
    const std::string label = "Element" + std::to_string(elementId);
    const auto assignment = model.materialNameByElementId.find(elementId); // 源单元id->材料名
    if (assignment == model.materialNameByElementId.end())
        throw std::invalid_argument(label + " has no material assignment");
    const std::string &name = assignment->second;
    const auto cached = materialCache.find(name); // 材料名->已缓存材料
    if (cached != materialCache.end())
        return cached->second;                          // 已有缓存
    const auto definition = materialsByName.find(name); // 没有缓存, 材料名->材料数据
    if (definition == materialsByName.end())            // 找不到指定的材料名
        throw std::invalid_argument(label + " references unknown material " + name);
    const MaterialData &data = *definition->second;
    if (!data.elastic) // 如果数据中没有弹性参数
        throw std::invalid_argument(label + " requires a linear elastic material: " + name);
    // 创建新材料实例
    std::shared_ptr<const Material3D> material = std::make_shared<const LinearElasticMaterial>(*data.elastic);
    materialCache.emplace(name, material);
    return material;
}