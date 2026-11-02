#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/alignment.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/modifier.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace coil::layout {

namespace {

class PaddingModifier final : public LayoutModifier {
public:
    explicit PaddingModifier(Insets insets)
        : m_insets(insets) {}

    [[nodiscard]] std::string_view typeId() const noexcept override { return "coil.padding"; }

    [[nodiscard]] BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept override {
        return outer.deflate(m_insets);
    }

    [[nodiscard]] Size outerSize(const BoxConstraints& /*outer*/,
                                 Size inner) const noexcept override {
        return Size{.width = inner.width + m_insets.horizontal(),
                    .height = inner.height + m_insets.vertical()};
    }

    [[nodiscard]] Rect innerBounds(Rect outer, Size /*inner*/) const noexcept override {
        return Rect{
            .origin = {.x = outer.origin.x + m_insets.left, .y = outer.origin.y + m_insets.top},
            .size = {.width = std::max(0.0F, outer.size.width - m_insets.horizontal()),
                     .height = std::max(0.0F, outer.size.height - m_insets.vertical())}};
    }

private:
    Insets m_insets;
};

/// size, minSize and maxSize: ask for `requested`, clamped into the incoming constraints.
class ConstraintsModifier final : public LayoutModifier {
public:
    ConstraintsModifier(std::string_view typeId, BoxConstraints requested)
        : m_typeId(typeId)
        , m_requested(requested) {}

    [[nodiscard]] std::string_view typeId() const noexcept override { return m_typeId; }

    [[nodiscard]] BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept override {
        return outer.enforce(m_requested);
    }

    [[nodiscard]] Size outerSize(const BoxConstraints& /*outer*/,
                                 Size inner) const noexcept override {
        return inner;
    }

    [[nodiscard]] Rect innerBounds(Rect outer, Size /*inner*/) const noexcept override {
        return outer;
    }

private:
    std::string_view m_typeId; ///< Always a string literal.
    BoxConstraints m_requested;
};

/// Offset of content of length `inner` inside length `outer`, for an alignment fraction in [-1, 1].
float alignedOffset(float outer, float inner, float fraction) {
    return (outer - inner) * (fraction + 1.0F) / 2.0F;
}

class AlignModifier final : public LayoutModifier {
public:
    explicit AlignModifier(Alignment alignment)
        : m_alignment(alignment) {}

    [[nodiscard]] std::string_view typeId() const noexcept override { return "coil.align"; }

    [[nodiscard]] BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept override {
        return outer.loosen();
    }

    [[nodiscard]] Size outerSize(const BoxConstraints& outer, Size inner) const noexcept override {
        return outer.constrain(inner);
    }

    [[nodiscard]] Rect innerBounds(Rect outer, Size inner) const noexcept override {
        return Rect{.origin = {.x = outer.origin.x + alignedOffset(outer.size.width, inner.width,
                                                                   m_alignment.horizontal),
                               .y = outer.origin.y + alignedOffset(outer.size.height, inner.height,
                                                                   m_alignment.vertical)},
                    .size = inner};
    }

private:
    Alignment m_alignment;
};

bool isValidFraction(float value) {
    return value >= -1.0F && value <= 1.0F; // false for NaN
}

ModifierResult constraintsModifier(std::string_view typeId,
                                   const Expected<BoxConstraints>& requested) {
    if (!requested.has_value()) {
        return makeUnexpected(requested.error().code(), requested.error().message());
    }
    return std::make_unique<ConstraintsModifier>(typeId, *requested);
}

bool isValidInset(float value) {
    return std::isfinite(value) && value >= 0.0F;
}

} // namespace

ModifierChain ModifierChain::then(ModifierResult modifier) && {
    if (m_error.has_value()) {
        return std::move(*this);
    }
    if (!modifier.has_value()) {
        m_error = std::move(modifier.error());
    } else if (*modifier == nullptr) {
        m_error = Error{ErrorCode::InvalidArgument, "a modifier factory returned no modifier"};
    } else {
        if (const auto* stage = dynamic_cast<const LayoutModifier*>(modifier->get())) {
            m_layoutStages.push_back(stage);
        }
        m_modifiers.push_back(std::move(*modifier));
    }
    return std::move(*this);
}

const std::optional<Error>& ModifierChain::error() const noexcept {
    return m_error;
}

std::span<const LayoutModifier* const> ModifierChain::layoutStages() const noexcept {
    return m_layoutStages;
}

std::vector<std::string_view> ModifierChain::typeIds() const {
    std::vector<std::string_view> ids;
    ids.reserve(m_modifiers.size());
    for (const auto& modifier : m_modifiers) {
        ids.push_back(modifier->typeId());
    }
    return ids;
}

ModifierResult padding(Insets insets) {
    if (!isValidInset(insets.left) || !isValidInset(insets.top) || !isValidInset(insets.right) ||
        !isValidInset(insets.bottom)) {
        return makeUnexpected(ErrorCode::InvalidArgument, "padding must be finite and >= 0");
    }
    return std::make_unique<PaddingModifier>(insets);
}

ModifierResult padding(float all) {
    return padding(Insets{.left = all, .top = all, .right = all, .bottom = all});
}

ModifierResult size(Size exact) {
    return constraintsModifier("coil.size", BoxConstraints::tight(exact));
}

ModifierResult minSize(Size minimum) {
    return constraintsModifier("coil.minSize", BoxConstraints::create(minimum.width, kUnbounded,
                                                                      minimum.height, kUnbounded));
}

ModifierResult maxSize(Size maximum) {
    return constraintsModifier("coil.maxSize",
                               BoxConstraints::create(0.0F, maximum.width, 0.0F, maximum.height));
}

ModifierResult align(Alignment alignment) {
    if (!isValidFraction(alignment.horizontal) || !isValidFraction(alignment.vertical)) {
        return makeUnexpected(ErrorCode::InvalidArgument, "alignment fractions must be in [-1, 1]");
    }
    return std::make_unique<AlignModifier>(alignment);
}

ModifierResult center() {
    return align(Alignment::center());
}

ModifierResult weight(float value) {
    return std::make_unique<Weight>(value);
}

} // namespace coil::layout
