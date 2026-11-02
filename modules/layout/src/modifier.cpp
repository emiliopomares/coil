#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/modifier.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace coil::layout {

namespace {

class PaddingModifier final : public LayoutModifier {
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

} // namespace

ModifierChain ModifierChain::then(ModifierResult /*modifier*/) && {
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

ModifierResult padding(Insets /*insets*/) {
    return std::make_unique<PaddingModifier>();
}

ModifierResult padding(float /*all*/) {
    return std::make_unique<PaddingModifier>();
}

} // namespace coil::layout
