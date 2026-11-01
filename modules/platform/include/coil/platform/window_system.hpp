#pragma once

#include <coil/foundation/error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
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

/// Parameters for creating a window.
struct WindowDesc {
    std::string title; ///< UTF-8. May be empty.
    WindowSize size;   ///< Both dimensions must be > 0.
};

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

protected:
    WindowSystem() = default;
};

} // namespace coil::platform
