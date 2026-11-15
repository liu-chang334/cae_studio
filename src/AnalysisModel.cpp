#include "AnalysisModel.h"
#include "C3D8Element.h"
#include "FEModel.h"
#include "LinearElastic.h"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
const std::vector<MeshId> &nodeIdsInSet(const FEModel &model, const std::string &setName) {
    const auto set = model.nodeSets.find(setName);
    if (set == model.nodeSets.end() || set->second.empty())
        throw std::invalid_argument("Unknown or empty node set: " + setName);
    return set->second;
}
} // namespace

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

std::size_t AnalysisModel::globalDofIndex(MeshId nodeId, int direction) const {
    if (direction < 1 || direction > 3)
        throw std::out_of_range("Direction must be in [1,3]");
    const auto node = nodeIndexById_.find(nodeId);
    if (node == nodeIndexById_.end())
        throw std::out_of_range("Unknown node ID: " + std::to_string(nodeId));
    return 3 * (node->second) + static_cast<std::size_t>(direction - 1);
}

std::size_t AnalysisModel::indexOf(std::size_t number) const {
    if (number == 0 || number > elements_.size())
        throw std::out_of_range("Invalid element number");
    return number - 1;
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

AnalysisModel::AnalysisModel(const FEModel &model) {
    // 01. 构建节点源id->连续0-based节点索引映射表
    nodeIndexById_.reserve(model.mesh.nodes.size());
    for (std::size_t i = 0; i < model.mesh.nodes.size(); ++i) {
        const MeshId sourceId = model.mesh.nodes[i].id;
        if (!nodeIndexById_.emplace(sourceId, i).second) // 检查节点id是否重复
            throw std::invalid_argument("Duplicate node ID: " + std::to_string(sourceId));
    }
    // 02. 构建材料名->材料地址映射表
    std::unordered_map<std::string, const MaterialData *> materialsByName;
    for (const auto &data : model.materials) {
        if (!materialsByName.emplace(data.name, &data).second) // 检查材料名是否重复
            throw std::invalid_argument("Duplicate material name: " + data.name);
    }
    // 03. 构建材料缓存表(首次使用才创建)
    std::unordered_map<std::string, std::shared_ptr<const Material3D>> materialCache;
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
                const auto iter = nodeIndexById_.find(nodeId);
                if (iter == nodeIndexById_.end())
                    throw std::invalid_argument(label + " references unknown node " + std::to_string(nodeId));
                nodes.push_back(model.mesh.nodes[iter->second]);
            }
            // 05. 查找材料
            std::shared_ptr<const Material3D> material = resolveMaterial(record.id, model, materialsByName, materialCache);
            // 06. 创建计算单元
            try {
                std::unique_ptr<SolidElement3D> element = std::make_unique<C3D8Element>(std::move(nodes), std::move(material));
                addElement(std::move(element), record.id);
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
    initializeNodalConditions(model);
}

void AnalysisModel::initializeNodalConditions(const FEModel &model) {
    forceVector_.assign(globalDofCount(), 0.0);
    prescribedDisplacements_.assign(globalDofCount(), std::nullopt);
    for (const auto &load : model.loads) {
        if (!std::isfinite(load.magnitude))
            throw std::invalid_argument("non-finite load on node set: " + load.nodeSetName);
        std::unordered_set<MeshId> loadedNodes;
        for (MeshId nodeId : nodeIdsInSet(model, load.nodeSetName)) {
            if (!loadedNodes.insert(nodeId).second)
                continue;
            const auto dof = globalDofIndex(nodeId, load.degreeOfFreedom);
            auto &force = forceVector_[dof];
            force += load.magnitude;
        }
    }
    for (const auto &boundary : model.boundaries) {
        if (!std::isfinite(boundary.displacement))
            throw std::invalid_argument("non-finite boundary displacement on node set: " + boundary.nodeSetName);
        for (MeshId nodeId : nodeIdsInSet(model, boundary.nodeSetName)) {
            const auto dof = globalDofIndex(nodeId, boundary.degreeOfFreedom);
            auto &prescribed = prescribedDisplacements_[dof];
            if (prescribed.has_value() && *prescribed != boundary.displacement)
                throw std::invalid_argument("Conflicting prescribed displacement at node " + std::to_string(nodeId) + ", direction " +
                                            std::to_string(boundary.degreeOfFreedom));
            prescribed = boundary.displacement;
        }
    }
}
