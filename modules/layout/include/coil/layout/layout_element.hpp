#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/modifier.hpp>

#include <vector>

namespace coil::layout {

/// Anything that takes part in layout: a leaf or a container, built-in or custom (ADR-0005
/// Amendment 1). Containers call measure() and arrange() on their children from measureContent()
/// and arrangeContent(); how they store their children is up to them. All rectangles are in the
/// root's coordinate space (DIPs).
class LayoutElement {
public:
    virtual ~LayoutElement() = default;
    LayoutElement(const LayoutElement&) = delete;
    LayoutElement& operator=(const LayoutElement&) = delete;
    LayoutElement(LayoutElement&&) = delete;
    LayoutElement& operator=(LayoutElement&&) = delete;

    /// Replaces the chain. Fails with the chain's first error, keeping the current chain.
    [[nodiscard]] Expected<void> setModifiers(ModifierChain chain);
    [[nodiscard]] const ModifierChain& modifiers() const noexcept;

    /// Measures the element through its chain. Returns a finite size that satisfies `constraints`,
    /// also kept as measuredSize().
    [[nodiscard]] Size measure(const BoxConstraints& constraints);
    /// Places the element: `bounds` is its outer rectangle; the chain decides where the content
    /// goes.
    void arrange(Rect bounds);

    /// Size from the last measure() (0x0 before any).
    [[nodiscard]] Size measuredSize() const noexcept;
    /// Outer rectangle from the last arrange() (all zero before any).
    [[nodiscard]] Rect bounds() const noexcept;
    /// Content rectangle (inside every modifier) from the last arrange() (all zero before any).
    [[nodiscard]] Rect contentBounds() const noexcept;

protected:
    LayoutElement() = default;

    /// The content's size under `constraints` (after every modifier has adjusted them).
    [[nodiscard]] virtual Size measureContent(const BoxConstraints& constraints) = 0;
    /// Places the content (a container arranges its children here).
    virtual void arrangeContent(Rect contentBounds) = 0;

private:
    ModifierChain m_modifiers;
    std::vector<Size> m_stageSizes; ///< Measured size inside each stage, from the last measure().
    Size m_measuredSize;
    Rect m_bounds;
    Rect m_contentBounds;
};

/// Lays out `root` to fill `size`: measures it with tight constraints, then arranges it at (0, 0).
/// Fails with InvalidArgument (and does nothing) unless both sides are finite and >= 0.
[[nodiscard]] Expected<void> layoutRoot(LayoutElement& root, Size size);

} // namespace coil::layout
