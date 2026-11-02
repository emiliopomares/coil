#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/alignment.hpp>
#include <coil/layout/box_constraints.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace coil::layout {

/// Base of every modifier (ADR-0019): its stable type id. Not copyable or movable; chains own
/// modifiers through std::unique_ptr.
class Modifier {
public:
    Modifier() = default;
    virtual ~Modifier() = default;
    Modifier(const Modifier&) = delete;
    Modifier& operator=(const Modifier&) = delete;
    Modifier(Modifier&&) = delete;
    Modifier& operator=(Modifier&&) = delete;

    /// Stable type id, for example "coil.padding". Custom modifiers use their own prefix.
    [[nodiscard]] virtual std::string_view typeId() const noexcept = 0;
};

/// One stage of an element's modifier chain. Built-in and custom layout modifiers implement this
/// same interface. A stage receives constraints from outside, passes (possibly changed)
/// constraints inward, reports its own size from the inner size, and places the inner content in
/// its rectangle, like a wrapper would, without being a node in the tree.
class LayoutModifier : public Modifier {
public:
    /// The constraints for everything inside this stage, given the constraints this stage receives.
    [[nodiscard]] virtual BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept = 0;
    /// This stage's size, given its constraints and the measured size of everything inside it.
    [[nodiscard]] virtual Size outerSize(const BoxConstraints& outer,
                                         Size inner) const noexcept = 0;
    /// Where everything inside goes, given this stage's rectangle and the measured inner size.
    [[nodiscard]] virtual Rect innerBounds(Rect outer, Size inner) const noexcept = 0;
};

/// Data for the parent (ADR-0019). It does not change how its own element is laid out; parents that
/// understand it read it with ModifierChain::find<T>(), and other parents ignore it.
class ParentDataModifier : public Modifier {};

/// What a modifier factory returns: the modifier, or why its parameters were rejected.
using ModifierResult = Expected<std::unique_ptr<Modifier>>;

/// An ordered list of modifiers of both kinds. The first one added is the outermost. Move-only.
/// Built fluently: ModifierChain{}.then(padding(8)).then(myModifier(...)).
class ModifierChain {
public:
    ModifierChain() = default;
    ModifierChain(ModifierChain&&) noexcept = default;
    ModifierChain& operator=(ModifierChain&&) noexcept = default;
    ModifierChain(const ModifierChain&) = delete;
    ModifierChain& operator=(const ModifierChain&) = delete;
    ~ModifierChain() = default;

    /// Appends `modifier` as the new innermost one. If it holds an error, the chain records it.
    /// Once the chain has an error, it keeps its first one and ignores further modifiers.
    [[nodiscard]] ModifierChain then(ModifierResult modifier) &&;

    /// The first error recorded, if any.
    [[nodiscard]] const std::optional<Error>& error() const noexcept;
    /// The type ids of every modifier, outermost first.
    [[nodiscard]] std::vector<std::string_view> typeIds() const;
    /// The layout modifiers, outermost first (parent-data modifiers are skipped).
    [[nodiscard]] std::span<const LayoutModifier* const> layoutStages() const noexcept;

    /// The outermost modifier of type T, or nullptr.
    template <std::derived_from<ParentDataModifier> T>
    [[nodiscard]] const T* find() const noexcept {
        for (const auto& modifier : m_modifiers) {
            if (const auto* match = dynamic_cast<const T*>(modifier.get())) {
                return match;
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<Modifier>> m_modifiers;
    std::vector<const LayoutModifier*> m_layoutStages; ///< Points into m_modifiers.
    std::optional<Error> m_error;
};

/// Space around the content. Fails with InvalidArgument unless every side is finite and >= 0.
[[nodiscard]] ModifierResult padding(Insets insets);
/// The same space on all four sides.
[[nodiscard]] ModifierResult padding(float all);

/// Asks for exactly `exact`; incoming constraints win where they disagree. Type id "coil.size".
/// Fails with InvalidArgument unless both sides are finite and >= 0.
[[nodiscard]] ModifierResult size(Size exact);
/// Asks for at least `minimum`; incoming constraints win. Type id "coil.minSize".
/// Fails with InvalidArgument unless both sides are finite and >= 0.
[[nodiscard]] ModifierResult minSize(Size minimum);
/// Asks for at most `maximum` (kUnbounded = no limit on that axis); incoming constraints win.
/// Type id "coil.maxSize". Fails with InvalidArgument if a side is NaN or negative.
[[nodiscard]] ModifierResult maxSize(Size maximum);
/// Lets the content take any size up to the available space, grows it to the incoming minimums, and
/// positions it by `alignment`. Type id "coil.align". Fails with InvalidArgument unless both
/// fractions are in [-1, 1].
[[nodiscard]] ModifierResult align(Alignment alignment);
/// align(Alignment::center()).
[[nodiscard]] ModifierResult center();

/// Share of the leftover main-axis space in a Row or Column (a parent-data modifier).
class Weight final : public ParentDataModifier {
public:
    explicit Weight(float value) noexcept
        : m_value(value) {}

    [[nodiscard]] std::string_view typeId() const noexcept override { return "coil.weight"; }

    [[nodiscard]] float value() const noexcept { return m_value; }

private:
    float m_value;
};

/// Type id "coil.weight". Fails with InvalidArgument unless `value` is finite and > 0.
[[nodiscard]] ModifierResult weight(float value);

} // namespace coil::layout
