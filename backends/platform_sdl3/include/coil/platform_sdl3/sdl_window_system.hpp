#pragma once

#include <coil/foundation/error.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace coil::platform_sdl3 {

/// Options for creating the SDL3-backed window system.
struct SdlWindowSystemOptions {
    /// SDL video driver to use (e.g. "cocoa", "x11", "wayland", "windows", "offscreen").
    /// Empty: let SDL choose the platform default.
    std::string videoDriver;
};

/// Creates a WindowSystem backed by SDL3. Initializes SDL's video subsystem; the returned object
/// shuts it down when destroyed. Must be called, and the result used, on the main thread.
/// Fails with Unavailable if SDL video cannot be initialized or is already initialized.
[[nodiscard]] Expected<std::unique_ptr<platform::WindowSystem>>
createSdlWindowSystem(const SdlWindowSystemOptions& options = {});

/// SDL's window id (SDL_WindowID) of a window created by an SDL window system, so callers can
/// address the window through SDL directly. Fails with InvalidArgument if `system` was not created
/// by createSdlWindowSystem, and with NotFound if `id` is not a live window.
[[nodiscard]] Expected<std::uint32_t> sdlWindowId(const platform::WindowSystem& system,
                                                  platform::WindowId id);

/// The InputSource of an SDL window system (SPEC-0026); valid as long as `system`. Fails with
/// InvalidArgument if `system` was not created by createSdlWindowSystem.
[[nodiscard]] Expected<std::reference_wrapper<platform::InputSource>>
inputSourceOf(platform::WindowSystem& system);

} // namespace coil::platform_sdl3
