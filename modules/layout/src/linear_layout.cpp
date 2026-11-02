#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/linear_layout.hpp>
#include <coil/layout/modifier.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

namespace coil::layout::detail {

namespace {

/// Maps main/cross values to x/y for one axis.
struct AxisMap {
    Axis axis;

    [[nodiscard]] float main(Size size) const noexcept {
        return axis == Axis::Horizontal ? size.width : size.height;
    }

    [[nodiscard]] float cross(Size size) const noexcept {
        return axis == Axis::Horizontal ? size.height : size.width;
    }

    [[nodiscard]] float main(Point point) const noexcept {
        return axis == Axis::Horizontal ? point.x : point.y;
    }

    [[nodiscard]] float cross(Point point) const noexcept {
        return axis == Axis::Horizontal ? point.y : point.x;
    }

    [[nodiscard]] float mainMax(const BoxConstraints& constraints) const noexcept {
        return axis == Axis::Horizontal ? constraints.maxWidth() : constraints.maxHeight();
    }

    [[nodiscard]] float crossMax(const BoxConstraints& constraints) const noexcept {
        return axis == Axis::Horizontal ? constraints.maxHeight() : constraints.maxWidth();
    }

    [[nodiscard]] Size size(float main, float cross) const noexcept {
        return axis == Axis::Horizontal ? Size{.width = main, .height = cross}
                                        : Size{.width = cross, .height = main};
    }

    [[nodiscard]] Point point(float main, float cross) const noexcept {
        return axis == Axis::Horizontal ? Point{.x = main, .y = cross}
                                        : Point{.x = cross, .y = main};
    }

    /// Constraints for a child; every value is valid by construction (rules 2-4).
    [[nodiscard]] BoxConstraints constraints(float mainMin, float mainMax, float crossMin,
                                             float crossMax) const noexcept {
        const Expected<BoxConstraints> result =
            axis == Axis::Horizontal ? BoxConstraints::create(mainMin, mainMax, crossMin, crossMax)
                                     : BoxConstraints::create(crossMin, crossMax, mainMin, mainMax);
        return result.value_or(BoxConstraints{});
    }
};

float totalSpacing(float spacing, std::size_t count) {
    return count > 1 ? spacing * static_cast<float>(count - 1) : 0.0F;
}

/// Lead before the first child and extra gap between children (rule 6).
struct Distribution {
    float lead = 0.0F;
    float gap = 0.0F;
};

Distribution distribute(MainAxisAlignment alignment, float free, std::size_t count) {
    const float positive = std::max(free, 0.0F);
    const auto n = static_cast<float>(count);
    switch (alignment) {
    case MainAxisAlignment::Start:
        return {};
    case MainAxisAlignment::Center:
        return {.lead = free / 2.0F, .gap = 0.0F};
    case MainAxisAlignment::End:
        return {.lead = free, .gap = 0.0F};
    case MainAxisAlignment::SpaceBetween:
        return {.lead = 0.0F, .gap = count > 1 ? positive / (n - 1.0F) : 0.0F};
    case MainAxisAlignment::SpaceAround:
        return {.lead = positive / (2.0F * n), .gap = positive / n};
    case MainAxisAlignment::SpaceEvenly:
        return {.lead = positive / (n + 1.0F), .gap = positive / (n + 1.0F)};
    }
    return {};
}

float crossOffset(CrossAxisAlignment alignment, float available, float child) {
    switch (alignment) {
    case CrossAxisAlignment::Center:
        return (available - child) / 2.0F;
    case CrossAxisAlignment::End:
        return available - child;
    case CrossAxisAlignment::Start:
    case CrossAxisAlignment::Stretch:
        return 0.0F;
    }
    return 0.0F;
}

} // namespace

Expected<void> LinearLayout::setOptions(LinearLayoutOptions options) {
    if (!std::isfinite(options.spacing) || options.spacing < 0.0F) {
        return makeUnexpected(ErrorCode::InvalidArgument, "spacing must be finite and >= 0");
    }
    m_options = options;
    return {};
}

Expected<void> LinearLayout::add(std::unique_ptr<LayoutElement> child) {
    if (child == nullptr) {
        return makeUnexpected(ErrorCode::InvalidArgument, "a child must not be null");
    }
    m_children.push_back(std::move(child));
    return {};
}

Size LinearLayout::measure(const BoxConstraints& constraints) {
    const AxisMap axes{m_axis};
    const float mainMax = axes.mainMax(constraints);
    const float crossMax = axes.crossMax(constraints);
    const bool stretch =
        m_options.crossAxis == CrossAxisAlignment::Stretch && std::isfinite(crossMax);
    const float crossMin = stretch ? crossMax : 0.0F;
    const bool weightsApply = std::isfinite(mainMax);
    const float spacing = totalSpacing(m_options.spacing, m_children.size());

    // Unweighted children first, unbounded on the main axis (rule 3).
    float usedMain = 0.0F;
    float cross = 0.0F;
    float totalWeight = 0.0F;
    for (const auto& child : m_children) {
        if (const auto* weight = child->modifiers().find<Weight>();
            weightsApply && weight != nullptr) {
            totalWeight += weight->value();
            continue;
        }
        const Size measured =
            child->measure(axes.constraints(0.0F, kUnbounded, crossMin, crossMax));
        usedMain += axes.main(measured);
        cross = std::max(cross, axes.cross(measured));
    }
    if (totalWeight <= 0.0F) {
        return axes.size(usedMain + spacing, cross);
    }

    // Weighted children share what is left, each tight at its share (rule 4).
    const float leftover = std::max(0.0F, mainMax - usedMain - spacing);
    for (const auto& child : m_children) {
        const auto* weight = child->modifiers().find<Weight>();
        if (weight == nullptr) {
            continue;
        }
        const float share = leftover * weight->value() / totalWeight;
        const Size measured = child->measure(axes.constraints(share, share, crossMin, crossMax));
        cross = std::max(cross, axes.cross(measured));
    }
    return axes.size(mainMax, cross);
}

void LinearLayout::arrange(Rect contentBounds) {
    if (m_children.empty()) {
        return;
    }
    const AxisMap axes{m_axis};
    const float availableMain = axes.main(contentBounds.size);
    const float availableCross = axes.cross(contentBounds.size);

    float childrenMain = 0.0F;
    for (const auto& child : m_children) {
        childrenMain += axes.main(child->measuredSize());
    }
    const float free =
        availableMain - childrenMain - totalSpacing(m_options.spacing, m_children.size());
    const Distribution distribution = distribute(m_options.mainAxis, free, m_children.size());

    const float crossStart = axes.cross(contentBounds.origin);
    float position = axes.main(contentBounds.origin) + distribution.lead;
    for (const auto& child : m_children) {
        const Size measured = child->measuredSize();
        const bool stretch = m_options.crossAxis == CrossAxisAlignment::Stretch;
        const float childCross = stretch ? availableCross : axes.cross(measured);
        const float offset = crossOffset(m_options.crossAxis, availableCross, childCross);
        child->arrange(Rect{.origin = axes.point(position, crossStart + offset),
                            .size = axes.size(axes.main(measured), childCross)});
        position += axes.main(measured) + m_options.spacing + distribution.gap;
    }
}

} // namespace coil::layout::detail
