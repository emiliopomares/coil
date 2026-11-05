#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace coil::platform {

/// Identifies a window owned by a WindowSystem. Value 0 never identifies a window.
/// Ids are unique per WindowSystem instance and never reused, even after destruction.
struct WindowId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(WindowId, WindowId) = default;
};

/// Window client-area size in logical units (DIPs, ADR-0006).
struct WindowSize {
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(WindowSize, WindowSize) = default;
};

/// The range a window's size must stay in (SPEC-0011). Each bound is optional; both dimensions of
/// a bound must be > 0, and min must not exceed max in either dimension.
struct WindowSizeLimits {
    std::optional<WindowSize> min; ///< No minimum when empty.
    std::optional<WindowSize> max; ///< No maximum when empty.

    friend constexpr bool operator==(const WindowSizeLimits&, const WindowSizeLimits&) = default;
};

/// Parameters for creating a window.
struct WindowDesc {
    std::string title;         ///< UTF-8. May be empty.
    WindowSize size;           ///< Both dimensions must be > 0, and within `limits`.
    bool resizable = false;    ///< Whether the user may resize the window (e.g. by dragging edges).
    WindowSizeLimits limits{}; ///< Applies to every size change.
};

/// The OS or the user asked to close a window (close button, Cmd+W, Alt+F4, ...).
/// The window stays open until the application destroys it.
struct CloseRequested {
    WindowId window;

    friend constexpr bool operator==(CloseRequested, CloseRequested) = default;
};

/// A window's logical size changed: by setSize(), by the user, or by the OS (SPEC-0010).
struct Resized {
    WindowId window;
    WindowSize size; ///< The new size, as applied (may differ from a request, e.g. OS limits).

    friend constexpr bool operator==(Resized, Resized) = default;
};

/// A window's display scale (physical pixels per DIP) changed, e.g. it moved to a display with a
/// different density. Its pixel size may have changed with it; query pixelSize() (SPEC-0013).
struct ScaleChanged {
    WindowId window;
    float scale = 0.0F; ///< The new display scale, > 0.

    friend constexpr bool operator==(ScaleChanged, ScaleChanged) = default;
};

/// An event about a window. New alternatives are added by later specs.
using WindowEvent = std::variant<CloseRequested, Resized, ScaleChanged>;

/// Port: creates and manages OS windows. Implemented by adapters in backends/.
/// The WindowSystem owns every window it creates; callers refer to windows by WindowId.
/// Used from a single thread (the UI thread).
class WindowSystem {
public:
    virtual ~WindowSystem();

    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;
    WindowSystem(WindowSystem&&) = delete;
    WindowSystem& operator=(WindowSystem&&) = delete;

    /// Creates a window. Fails with InvalidArgument if a size dimension is <= 0.
    [[nodiscard]] virtual Expected<WindowId> createWindow(const WindowDesc& desc) = 0;

    /// Destroys a window. Fails with NotFound if `id` is not a live window.
    [[nodiscard]] virtual Expected<void> destroyWindow(WindowId id) = 0;

    /// Live windows, in creation order.
    [[nodiscard]] virtual std::vector<WindowId> windows() const = 0;

    /// UTF-8 title of a window. Fails with NotFound if `id` is not a live window.
    [[nodiscard]] virtual Expected<std::string> title(WindowId id) const = 0;

    /// Sets the UTF-8 title of a window. Fails with NotFound if `id` is not a live window.
    [[nodiscard]] virtual Expected<void> setTitle(WindowId id, std::string_view title) = 0;

    /// Client-area size of a window. Fails with NotFound if `id` is not a live window.
    [[nodiscard]] virtual Expected<WindowSize> size(WindowId id) const = 0;

    /// Asks to resize a window. The change is reported by a Resized event; size() returns the new
    /// size by the time that event has been returned. No event if the size does not change.
    /// Fails with InvalidArgument for a non-positive size and NotFound for an unknown window.
    [[nodiscard]] virtual Expected<void> setSize(WindowId id, WindowSize size) = 0;

    /// Whether the user may resize the window. Fails with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<bool> resizable(WindowId id) const = 0;

    /// The window's current size limits. Fails with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<WindowSizeLimits> sizeLimits(WindowId id) const = 0;

    /// Replaces the window's size limits. If the current size is outside them, the window is
    /// resized to the nearest size inside (reported by Resized). Fails with InvalidArgument for
    /// invalid limits and NotFound for an unknown window.
    [[nodiscard]] virtual Expected<void> setSizeLimits(WindowId id, WindowSizeLimits limits) = 0;

    /// Size of the window's drawable area in physical pixels. Fails with NotFound for an unknown
    /// window.
    [[nodiscard]] virtual Expected<WindowSize> pixelSize(WindowId id) const = 0;

    /// Physical pixels per DIP for the window (e.g. 2.0 on a Retina display). Always > 0. Fails
    /// with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<float> displayScale(WindowId id) const = 0;

    /// Shows `image` in the window, pixel for pixel from the top-left corner, at the window's pixel
    /// size (pixelSize()). Each pixel is composited over opaque black; window pixels the image does
    /// not cover become opaque black; image pixels beyond the window are cropped. Nothing is
    /// scaled. Fails with NotFound for an unknown window (SPEC-0022).
    [[nodiscard]] virtual Expected<void> presentImage(WindowId id, const Image& image) = 0;

    /// Returns all events that occurred since the previous call, in order; each event is returned
    /// once. Events for windows destroyed before this call are dropped. Non-blocking.
    [[nodiscard]] virtual std::vector<WindowEvent> pollEvents() = 0;

    /// Like pollEvents(), but if no event is pending it blocks until one arrives or `timeout`
    /// elapses. May return early with no events (e.g. woken by an event this port does not map);
    /// callers loop. A zero or negative timeout never blocks.
    [[nodiscard]] virtual std::vector<WindowEvent> waitEvents(Clock::Duration timeout) = 0;

protected:
    WindowSystem() = default;
};

} // namespace coil::platform
