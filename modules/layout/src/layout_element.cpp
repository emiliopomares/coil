#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/modifier.hpp>

#include <utility>

namespace coil::layout {

Expected<void> LayoutElement::setModifiers(ModifierChain chain) {
    m_modifiers = std::move(chain);
    return {};
}

const ModifierChain& LayoutElement::modifiers() const noexcept {
    return m_modifiers;
}

Size LayoutElement::measure(const BoxConstraints& /*constraints*/) {
    return m_measuredSize;
}

void LayoutElement::arrange(Rect /*bounds*/) {
    m_contentBounds = Rect{};
}

Size LayoutElement::measuredSize() const noexcept {
    return m_measuredSize;
}

Rect LayoutElement::bounds() const noexcept {
    return m_bounds;
}

Rect LayoutElement::contentBounds() const noexcept {
    return m_contentBounds;
}

Expected<void> layoutRoot(LayoutElement& /*root*/, Size /*size*/) {
    return {};
}

} // namespace coil::layout
