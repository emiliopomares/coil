#pragma once

#include <coil/foundation/geometry.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace coil::core {

/// Where Navigate moves: spatially, or through the focus order (SPEC-0028 Q1).
enum class NavigationDirection : std::uint8_t { Up, Down, Left, Right, Next, Previous };

/// Do the default action of the focused widget (press a button, toggle a checkbox).
struct Activate {
    platform::WindowId window;

    friend constexpr bool operator==(Activate, Activate) = default;
};

/// Back out: close a menu, abandon an edit.
struct Cancel {
    platform::WindowId window;

    friend constexpr bool operator==(Cancel, Cancel) = default;
};

/// Move focus or selection.
struct Navigate {
    platform::WindowId window;
    NavigationDirection direction = NavigationDirection::Next;

    friend constexpr bool operator==(Navigate, Navigate) = default;
};

/// Scroll content at `position` (DIPs) by wheel steps, as platform::PointerScrolled defines them.
struct Scroll {
    platform::WindowId window;
    Point position;
    float deltaX = 0.0F;
    float deltaY = 0.0F;

    friend constexpr bool operator==(Scroll, Scroll) = default;
};

/// A pointer moved to `position` (DIPs).
struct PointerMove {
    platform::WindowId window;
    platform::PointerId pointer;
    Point position;

    friend constexpr bool operator==(PointerMove, PointerMove) = default;
};

/// A pointer button went down at `position` (DIPs).
struct PointerDown {
    platform::WindowId window;
    platform::PointerId pointer;
    platform::PointerButton button = platform::PointerButton::Primary;
    Point position;

    friend constexpr bool operator==(PointerDown, PointerDown) = default;
};

/// A pointer button went up at `position` (DIPs).
struct PointerUp {
    platform::WindowId window;
    platform::PointerId pointer;
    platform::PointerButton button = platform::PointerButton::Primary;
    Point position;

    friend constexpr bool operator==(PointerUp, PointerUp) = default;
};

/// Text to insert, UTF-8.
struct TextInput {
    platform::WindowId window;
    std::string text;

    friend bool operator==(const TextInput&, const TextInput&) = default;
};

/// What widgets receive instead of device input (ADR-0007).
using Intent = std::variant<Activate, Cancel, Navigate, Scroll, PointerMove, PointerDown, PointerUp,
                            TextInput>;

} // namespace coil::core
