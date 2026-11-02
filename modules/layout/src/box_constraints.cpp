#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>

namespace coil::layout {

Expected<BoxConstraints> BoxConstraints::create(float /*minWidth*/, float /*maxWidth*/,
                                                float /*minHeight*/, float /*maxHeight*/) {
    return BoxConstraints{};
}

Expected<BoxConstraints> BoxConstraints::tight(Size /*size*/) {
    return BoxConstraints{};
}

Expected<BoxConstraints> BoxConstraints::loose(Size /*size*/) {
    return BoxConstraints{};
}

float BoxConstraints::minWidth() const noexcept {
    return m_minWidth;
}

float BoxConstraints::maxWidth() const noexcept {
    return m_maxWidth;
}

float BoxConstraints::minHeight() const noexcept {
    return m_minHeight;
}

float BoxConstraints::maxHeight() const noexcept {
    return m_maxHeight;
}

bool BoxConstraints::isTight() const noexcept {
    return m_minWidth < 0.0F;
}

bool BoxConstraints::hasBoundedWidth() const noexcept {
    return m_minWidth < 0.0F;
}

bool BoxConstraints::hasBoundedHeight() const noexcept {
    return m_minHeight < 0.0F;
}

Size BoxConstraints::constrain(Size /*size*/) const noexcept {
    return Size{.width = m_minWidth, .height = m_minHeight};
}

bool BoxConstraints::isSatisfiedBy(Size /*size*/) const noexcept {
    return m_minWidth < 0.0F;
}

BoxConstraints BoxConstraints::deflate(Insets /*insets*/) const noexcept {
    return *this;
}

BoxConstraints BoxConstraints::loosen() const noexcept {
    return *this;
}

} // namespace coil::layout
