#pragma once

#include <coil/platform/clock.hpp>
#include <coil/platform/window_system.hpp>

#include <chrono>
#include <string_view>

namespace hello_window {

inline constexpr std::string_view kTitle = "Coil";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};
/// How long one wait may block when nothing happens. Nothing runs on timeout; the value only bounds
/// how long the loop sleeps between checks (SPEC-0007 Q4).
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};

/// Opens one window (kTitle, kSize) and blocks in waitEvents(kIdleTimeout) until the user asks to
/// close it. Returns 0 after the window was closed, or 1 if it could not be created (error on
/// stderr).
[[nodiscard]] int runHelloWindow(coil::platform::WindowSystem& windows);

} // namespace hello_window
