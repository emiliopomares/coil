#pragma once

#include <coil/foundation/geometry.hpp>
#include <coil/render/paint.hpp>

#include <span>
#include <variant>
#include <vector>

namespace coil::render {

/// Radius of each corner of a rounded rectangle, in DIPs.
struct CornerRadii {
    float topLeft = 0.0F;
    float topRight = 0.0F;
    float bottomRight = 0.0F;
    float bottomLeft = 0.0F;

    /// The same radius on every corner.
    [[nodiscard]] static constexpr CornerRadii all(float radius) noexcept {
        return {.topLeft = radius, .topRight = radius, .bottomRight = radius, .bottomLeft = radius};
    }

    friend constexpr bool operator==(CornerRadii, CornerRadii) = default;
};

/// Fills `rect` with `paint`.
struct FillRect {
    Rect rect;
    Paint paint;

    friend bool operator==(const FillRect&, const FillRect&) = default;
};

/// Fills `rect` with rounded corners.
struct FillRoundedRect {
    Rect rect;
    CornerRadii radii;
    Paint paint;

    friend bool operator==(const FillRoundedRect&, const FillRoundedRect&) = default;
};

/// A border of `width` drawn inside `rect` (its outer edge is the rect's edge).
struct StrokeBorder {
    Rect rect;
    CornerRadii radii;
    float width = 0.0F;
    Paint paint;

    friend bool operator==(const StrokeBorder&, const StrokeBorder&) = default;
};

/// Intersects the current clip with `rect` until the matching PopClip.
struct PushClip {
    Rect rect;

    friend constexpr bool operator==(PushClip, PushClip) = default;
};

/// Ends the most recent PushClip.
struct PopClip {
    friend constexpr bool operator==(PopClip, PopClip) = default;
};

/// One recorded drawing operation (ADR-0009).
using DrawCommand = std::variant<FillRect, FillRoundedRect, StrokeBorder, PushClip, PopClip>;

/// Recorded drawing, in painting order. Plain data: copyable, movable, comparable.
class DrawList {
public:
    [[nodiscard]] std::span<const DrawCommand> commands() const noexcept { return m_commands; }

    [[nodiscard]] bool empty() const noexcept { return m_commands.empty(); }

    void clear() noexcept { m_commands.clear(); }

    /// Records `command` as given (Painter normalizes before appending).
    void append(DrawCommand command);

    friend bool operator==(const DrawList&, const DrawList&) = default;

private:
    std::vector<DrawCommand> m_commands;
};

} // namespace coil::render
