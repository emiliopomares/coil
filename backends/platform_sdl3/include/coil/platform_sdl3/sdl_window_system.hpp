#pragma once

#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>

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

} // namespace coil::platform_sdl3
