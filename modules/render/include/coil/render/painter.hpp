#pragma once

#include <coil/foundation/geometry.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/paint.hpp>

#include <cstddef>

namespace coil::render {

/// Records drawing into a DrawList (ADR-0009). Never fails: degenerate input is normalized or
/// skipped, and clips are always balanced. Must not outlive its DrawList. Not copyable or movable.
class Painter {
public:
    explicit Painter(DrawList& target) noexcept
        : m_target(&target) {}

    /// Records a PopClip for every clip still open.
    ~Painter();
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;
    Painter(Painter&&) = delete;
    Painter& operator=(Painter&&) = delete;

    /// Skipped if `rect` is empty, negative or not finite.
    void fillRect(Rect rect, const Paint& paint);
    /// Skipped like fillRect. Negative or NaN radii become 0; each radius is limited to half the
    /// rect's smaller side.
    void fillRoundedRect(Rect rect, CornerRadii radii, const Paint& paint);
    /// A border inside `rect`. Skipped like fillRect, or if `width` is not finite or <= 0; radii as
    /// in fillRoundedRect; `width` limited to half the rect's smaller side.
    void strokeBorder(Rect rect, CornerRadii radii, float width, const Paint& paint);
    /// Clips to `rect` (an empty rect at its origin if `rect` is degenerate) until popClip().
    void pushClip(Rect rect);
    /// Ends the most recent clip; ignored when no clip is open.
    void popClip();

    /// Number of clips currently open.
    [[nodiscard]] std::size_t clipDepth() const noexcept { return m_clipDepth; }

private:
    DrawList* m_target;
    std::size_t m_clipDepth = 0;
};

} // namespace coil::render
