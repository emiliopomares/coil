#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/linear_layout.hpp>

#include <memory>
#include <utility>

namespace coil::layout::detail {

Expected<void> LinearLayout::setOptions(LinearLayoutOptions options) {
    m_options = options;
    return {};
}

Expected<void> LinearLayout::add(std::unique_ptr<LayoutElement> child) {
    m_children.push_back(std::move(child));
    return {};
}

Size LinearLayout::measure(const BoxConstraints& /*constraints*/) {
    m_children.shrink_to_fit();
    return Size{};
}

void LinearLayout::arrange(Rect /*contentBounds*/) {
    m_axis = m_axis == Axis::Horizontal ? Axis::Horizontal : Axis::Vertical;
}

} // namespace coil::layout::detail
