#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/modifier.hpp>

#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

namespace coil::layout {

namespace {

/// Brings a size reported by content or a modifier within `constraints` and makes it finite.
Size sanitize(const BoxConstraints& constraints, Size size) {
    Size result = constraints.constrain(size);
    if (!std::isfinite(result.width)) {
        result.width = constraints.minWidth();
    }
    if (!std::isfinite(result.height)) {
        result.height = constraints.minHeight();
    }
    return result;
}

} // namespace

Expected<void> LayoutElement::setModifiers(ModifierChain chain) {
    if (const auto& error = chain.error(); error.has_value()) {
        return makeUnexpected(error->code(), error->message());
    }
    m_modifiers = std::move(chain);
    const std::size_t stages = m_modifiers.layoutStages().size() + 1;
    m_stageConstraints.assign(stages, BoxConstraints{});
    m_stageSizes.assign(stages, Size{});
    return {};
}

const ModifierChain& LayoutElement::modifiers() const noexcept {
    return m_modifiers;
}

Size LayoutElement::measure(const BoxConstraints& constraints) {
    const auto stages = m_modifiers.layoutStages();
    m_stageConstraints.resize(stages.size() + 1);
    m_stageSizes.resize(stages.size() + 1);

    // Going in: each stage's constraints come from the stage outside it.
    auto constraint = m_stageConstraints.begin();
    *constraint = constraints;
    for (const auto& stage : stages) {
        const BoxConstraints inner = stage->innerConstraints(*constraint);
        constraint = std::next(constraint);
        *constraint = inner;
    }

    // The content, then coming out: each stage's size comes from the size inside it.
    m_stageSizes.back() =
        sanitize(m_stageConstraints.back(), measureContent(m_stageConstraints.back()));
    auto size = m_stageSizes.rbegin();
    auto stageConstraint = std::next(m_stageConstraints.rbegin());
    for (auto stage = stages.rbegin(); stage != stages.rend(); ++stage, ++stageConstraint) {
        const Size inner = *size;
        size = std::next(size);
        *size = sanitize(*stageConstraint, (*stage)->outerSize(*stageConstraint, inner));
    }
    m_measuredSize = m_stageSizes.front();
    return m_measuredSize;
}

void LayoutElement::arrange(Rect bounds) {
    const auto stages = m_modifiers.layoutStages();
    m_stageSizes.resize(stages.size() + 1);

    Rect inner = bounds;
    auto innerSize = m_stageSizes.begin();
    for (const auto& stage : stages) {
        innerSize = std::next(innerSize);
        inner = stage->innerBounds(inner, *innerSize);
    }
    m_bounds = bounds;
    m_contentBounds = inner;
    arrangeContent(inner);
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

Expected<void> layoutRoot(LayoutElement& root, Size size) {
    const Expected<BoxConstraints> constraints = BoxConstraints::tight(size);
    if (!constraints.has_value()) {
        return makeUnexpected(constraints.error().code(), constraints.error().message());
    }
    static_cast<void>(root.measure(*constraints));
    root.arrange(Rect{.origin = {}, .size = size});
    return {};
}

} // namespace coil::layout
