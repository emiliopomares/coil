#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>

#include <algorithm>
#include <cmath>

namespace coil::layout {

namespace {

/// Valid per rule 3: a finite, non-negative minimum and a maximum no smaller than it.
bool isValidAxis(float min, float max) {
    return std::isfinite(min) && min >= 0.0F && !std::isnan(max) && max >= min;
}

float clampAxis(float value, float min, float max) {
    if (std::isnan(value)) {
        return min;
    }
    return std::clamp(value, min, max);
}

/// NaN and infinite insets count as 0, so deflate() keeps the invariants (rule 6).
float finiteOrZero(float value) {
    return std::isfinite(value) ? value : 0.0F;
}

float deflateBound(float bound, float amount) {
    if (std::isinf(bound)) {
        return bound;
    }
    return std::max(0.0F, bound - amount);
}

} // namespace

Expected<BoxConstraints> BoxConstraints::create(float minWidth, float maxWidth, float minHeight,
                                                float maxHeight) {
    if (!isValidAxis(minWidth, maxWidth) || !isValidAxis(minHeight, maxHeight)) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              "box constraints need 0 <= min <= max, a finite min and no NaN");
    }
    return BoxConstraints{minWidth, maxWidth, minHeight, maxHeight};
}

Expected<BoxConstraints> BoxConstraints::tight(Size size) {
    if (!std::isfinite(size.width) || !std::isfinite(size.height)) {
        return makeUnexpected(ErrorCode::InvalidArgument, "a tight size must be finite");
    }
    return create(size.width, size.width, size.height, size.height);
}

Expected<BoxConstraints> BoxConstraints::loose(Size size) {
    return create(0.0F, size.width, 0.0F, size.height);
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
    return m_minWidth == m_maxWidth && m_minHeight == m_maxHeight;
}

bool BoxConstraints::hasBoundedWidth() const noexcept {
    return std::isfinite(m_maxWidth);
}

bool BoxConstraints::hasBoundedHeight() const noexcept {
    return std::isfinite(m_maxHeight);
}

Size BoxConstraints::constrain(Size size) const noexcept {
    return Size{.width = clampAxis(size.width, m_minWidth, m_maxWidth),
                .height = clampAxis(size.height, m_minHeight, m_maxHeight)};
}

bool BoxConstraints::isSatisfiedBy(Size size) const noexcept {
    return size.width >= m_minWidth && size.width <= m_maxWidth && size.height >= m_minHeight &&
           size.height <= m_maxHeight;
}

BoxConstraints BoxConstraints::deflate(Insets insets) const noexcept {
    const float horizontal = finiteOrZero(insets.left) + finiteOrZero(insets.right);
    const float vertical = finiteOrZero(insets.top) + finiteOrZero(insets.bottom);
    return BoxConstraints{deflateBound(m_minWidth, horizontal),
                          deflateBound(m_maxWidth, horizontal), deflateBound(m_minHeight, vertical),
                          deflateBound(m_maxHeight, vertical)};
}

BoxConstraints BoxConstraints::loosen() const noexcept {
    return BoxConstraints{0.0F, m_maxWidth, 0.0F, m_maxHeight};
}

BoxConstraints BoxConstraints::enforce(const BoxConstraints& other) const noexcept {
    return BoxConstraints{std::clamp(other.m_minWidth, m_minWidth, m_maxWidth),
                          std::clamp(other.m_maxWidth, m_minWidth, m_maxWidth),
                          std::clamp(other.m_minHeight, m_minHeight, m_maxHeight),
                          std::clamp(other.m_maxHeight, m_minHeight, m_maxHeight)};
}

} // namespace coil::layout
