#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/alignment.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/modifier.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
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

class NeutralModifier final : public LayoutModifier {
public:
    [[nodiscard]] std::string_view typeId() const noexcept override { return ""; }

    [[nodiscard]] BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept override {
        return outer;
    }

    [[nodiscard]] Size outerSize(const BoxConstraints& /*outer*/,
                                 Size inner) const noexcept override {
        return inner;
    }

    [[nodiscard]] Rect innerBounds(Rect outer, Size /*inner*/) const noexcept override {
        return outer;
    }
};

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
        m_modifiers.push_back(std::move(*modifier));
    }
    return std::move(*this);
}

const std::optional<Error>& ModifierChain::error() const noexcept {
    return m_error;
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

ModifierResult size(Size /*exact*/) {
    return std::make_unique<NeutralModifier>();
}

ModifierResult minSize(Size /*minimum*/) {
    return std::make_unique<NeutralModifier>();
}

ModifierResult maxSize(Size /*maximum*/) {
    return std::make_unique<NeutralModifier>();
}

ModifierResult align(Alignment /*alignment*/) {
    return std::make_unique<NeutralModifier>();
}

ModifierResult center() {
    return std::make_unique<NeutralModifier>();
}

} // namespace coil::layout
