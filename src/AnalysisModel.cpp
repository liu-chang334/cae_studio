#include "AnalysisModel.h"

#include <stdexcept>

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