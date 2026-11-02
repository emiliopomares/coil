#pragma once

#include <format>
#include <string_view>

namespace coil::foundation {

/// A width and height in DIPs.
struct Size {
    float width = 0.0F;
    float height = 0.0F;

    friend constexpr bool operator==(Size, Size) = default;
};

/// A position in DIPs. x grows rightwards, y grows downwards.
struct Point {
    float x = 0.0F;
    float y = 0.0F;

    friend constexpr bool operator==(Point, Point) = default;
};

/// An axis-aligned rectangle: its top-left corner and its size, in DIPs.
struct Rect {
    Point origin;
    Size size;

    /// origin.x
    [[nodiscard]] constexpr float left() const noexcept { return origin.x; }

    /// origin.y
    [[nodiscard]] constexpr float top() const noexcept { return origin.y; }

    /// origin.x + size.width
    [[nodiscard]] constexpr float right() const noexcept { return origin.x + size.width; }

    /// origin.y + size.height
    [[nodiscard]] constexpr float bottom() const noexcept { return origin.y + size.height; }

    friend constexpr bool operator==(Rect, Rect) = default;
};

/// Space around the four edges of a box, in DIPs.
struct Insets {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;

    /// left + right
    [[nodiscard]] constexpr float horizontal() const noexcept { return left + right; }

    /// top + bottom
    [[nodiscard]] constexpr float vertical() const noexcept { return top + bottom; }

    friend constexpr bool operator==(Insets, Insets) = default;
};

} // namespace coil::foundation

namespace coil {
using foundation::Insets;
using foundation::Point;
using foundation::Rect;
using foundation::Size;
} // namespace coil

/// Formats as "100x50".
template <>
struct std::formatter<coil::foundation::Size> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(coil::foundation::Size size, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(
            std::format("{}x{}", size.width, size.height), ctx);
    }
};

/// Formats as "(10, 20)".
template <>
struct std::formatter<coil::foundation::Point> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(coil::foundation::Point point, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(std::format("({}, {})", point.x, point.y),
                                                        ctx);
    }
};

/// Formats as "(10, 20) 100x50": origin, then size.
template <>
struct std::formatter<coil::foundation::Rect> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(coil::foundation::Rect rect, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(
            std::format("{} {}", rect.origin, rect.size), ctx);
    }
};

/// Formats as "{1, 2, 3, 4}": left, top, right, bottom.
template <>
struct std::formatter<coil::foundation::Insets> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(coil::foundation::Insets insets, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(
            std::format("{{{}, {}, {}, {}}}", insets.left, insets.top, insets.right, insets.bottom),
            ctx);
    }
};
