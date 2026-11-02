#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/alignment.hpp>
#include <coil/layout/box_constraints.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace coil::layout {

class LayoutElement;

/// One stage of an element's modifier chain (ADR-0019). Built-in and custom modifiers implement
/// this same interface. A stage receives constraints from outside, passes (possibly changed)
/// constraints inward, reports its own size from the inner size, and places the inner content in
/// its rectangle, like a wrapper would, without being a node in the tree.
class LayoutModifier {
public:
    LayoutModifier() = default;
    virtual ~LayoutModifier() = default;
    LayoutModifier(const LayoutModifier&) = delete;
    LayoutModifier& operator=(const LayoutModifier&) = delete;
    LayoutModifier(LayoutModifier&&) = delete;
    LayoutModifier& operator=(LayoutModifier&&) = delete;

    /// Stable type id, for example "coil.padding". Custom modifiers use their own prefix.
    [[nodiscard]] virtual std::string_view typeId() const noexcept = 0;
    /// The constraints for everything inside this stage, given the constraints this stage receives.
    [[nodiscard]] virtual BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept = 0;
    /// This stage's size, given its constraints and the measured size of everything inside it.
    [[nodiscard]] virtual Size outerSize(const BoxConstraints& outer,
                                         Size inner) const noexcept = 0;
    /// Where everything inside goes, given this stage's rectangle and the measured inner size.
    [[nodiscard]] virtual Rect innerBounds(Rect outer, Size inner) const noexcept = 0;
};

/// What a modifier factory returns: the modifier, or why its parameters were rejected.
using ModifierResult = Expected<std::unique_ptr<LayoutModifier>>;

/// An ordered list of modifiers. The first one added is the outermost. Move-only.
/// Built fluently: ModifierChain{}.then(padding(8)).then(myModifier(...)).
class ModifierChain {
public:
    ModifierChain() = default;
    ModifierChain(ModifierChain&&) noexcept = default;
    ModifierChain& operator=(ModifierChain&&) noexcept = default;
    ModifierChain(const ModifierChain&) = delete;
    ModifierChain& operator=(const ModifierChain&) = delete;
    ~ModifierChain() = default;

    /// Appends `modifier` as the new innermost stage. If it holds an error, the chain records it.
    /// Once the chain has an error, it keeps its first one and ignores further modifiers.
    [[nodiscard]] ModifierChain then(ModifierResult modifier) &&;

    /// The first error recorded, if any.
    [[nodiscard]] const std::optional<Error>& error() const noexcept;
    /// The type ids of the stages, outermost first.
    [[nodiscard]] std::vector<std::string_view> typeIds() const;

private:
    friend class LayoutElement; // Runs the stages; the chain's storage stays an implementation
                                // detail.

    std::vector<std::unique_ptr<LayoutModifier>> m_modifiers;
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

} // namespace coil::layout
