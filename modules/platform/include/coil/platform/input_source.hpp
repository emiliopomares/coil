#pragma once

#include <coil/foundation/geometry.hpp>
#include <coil/platform/window_system.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace coil::platform {

/// Identifies a pointer device: a mouse, or a touch or pen acting as one (SPEC-0026 Q6). Stable
/// while the device exists.
struct PointerId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(PointerId, PointerId) = default;
};

/// Pointer buttons by role. Primary is the main (usually left) button.
enum class PointerButton : std::uint8_t { Primary, Secondary, Middle, Back, Forward };

/// Monotonic time of an input event, from an adapter-defined origin. Only differences between
/// events of the same source are meaningful.
using InputTimestamp = std::chrono::nanoseconds;

/// The pointer moved. `position` is in DIPs from the window's top-left corner.
struct PointerMoved {
    WindowId window;
    PointerId pointer;
    Point position;
    InputTimestamp time{};

    friend constexpr bool operator==(const PointerMoved&, const PointerMoved&) = default;
};

/// A pointer button went down at `position` (DIPs).
struct PointerPressed {
    WindowId window;
    PointerId pointer;
    PointerButton button = PointerButton::Primary;
    Point position;
    InputTimestamp time{};

    friend constexpr bool operator==(const PointerPressed&, const PointerPressed&) = default;
};

/// A pointer button went up at `position` (DIPs).
struct PointerReleased {
    WindowId window;
    PointerId pointer;
    PointerButton button = PointerButton::Primary;
    Point position;
    InputTimestamp time{};

    friend constexpr bool operator==(const PointerReleased&, const PointerReleased&) = default;
};

/// Wheel or trackpad scroll at `position` (DIPs), in wheel steps (fractional for precise devices).
/// Positive deltaY scrolls content up (away from the user) and positive deltaX scrolls right,
/// whatever the OS "natural scrolling" setting (SPEC-0026 rule 4).
struct PointerScrolled {
    WindowId window;
    PointerId pointer;
    Point position;
    float deltaX = 0.0F;
    float deltaY = 0.0F;
    InputTimestamp time{};

    friend constexpr bool operator==(const PointerScrolled&, const PointerScrolled&) = default;
};

/// Keys by their meaning in the current keyboard layout: on AZERTY, the key that types "a" is
/// Key::A (SPEC-0027 Q1). Modifier keys count either side (Q3); Super is Command on macOS and the
/// Windows key elsewhere.
enum class Key : std::uint8_t {
    // clang-format off
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Enter, Escape, Backspace, Tab, Space,
    Left, Right, Up, Down, Home, End, PageUp, PageDown, Insert, Delete,
    Shift, Control, Alt, Super,
    // clang-format on
};

/// Modifier keys held when an event happened, either side counting.
struct KeyModifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;

    friend constexpr bool operator==(KeyModifiers, KeyModifiers) = default;
};

/// A key went down in the window with keyboard focus; auto-repeat sends more with `repeat` set.
struct KeyPressed {
    WindowId window;
    Key key = Key::A;
    KeyModifiers modifiers;
    bool repeat = false;
    InputTimestamp time{};

    friend constexpr bool operator==(const KeyPressed&, const KeyPressed&) = default;
};

/// A key went up in the window with keyboard focus.
struct KeyReleased {
    WindowId window;
    Key key = Key::A;
    KeyModifiers modifiers;
    InputTimestamp time{};

    friend constexpr bool operator==(const KeyReleased&, const KeyReleased&) = default;
};

/// Text the user entered (one character or a composed string), UTF-8 as the platform produced it.
/// Never empty.
struct TextEntered {
    WindowId window;
    std::string text;
    InputTimestamp time{};

    friend bool operator==(const TextEntered&, const TextEntered&) = default;
};

/// A raw input event, typed per device class (ADR-0007 Amendment 1). New classes add alternatives.
using RawInputEvent = std::variant<PointerMoved, PointerPressed, PointerReleased, PointerScrolled,
                                   KeyPressed, KeyReleased, TextEntered>;

/// Port: raw input from the user's devices (ADR-0007). Implemented by adapters in backends/. Used
/// from the UI thread.
class InputSource {
public:
    /// Pure, so implementations can also implement another port (SPEC-0026 Q1).
    virtual ~InputSource() = 0;

    InputSource(const InputSource&) = delete;
    InputSource& operator=(const InputSource&) = delete;
    InputSource(InputSource&&) = delete;
    InputSource& operator=(InputSource&&) = delete;

    /// Events since the previous call, in the order they happened, each once. Events for windows
    /// that are not live (or that this source's window system did not create) are dropped.
    /// Non-blocking.
    [[nodiscard]] virtual std::vector<RawInputEvent> pollInput() = 0;

protected:
    InputSource() = default;
};

} // namespace coil::platform
