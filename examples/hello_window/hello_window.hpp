#pragma once

#include <coil/platform/clock.hpp>
#include <coil/platform/window_system.hpp>

#include <chrono>
#include <ostream>
#include <string_view>

namespace hello_window {

inline constexpr std::string_view kTitle = "Coil";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};
/// The window can't be resized below this (SPEC-0012).
inline constexpr coil::platform::WindowSize kMinSize{.width = 320, .height = 240};
/// How long one wait may block when nothing happens. Nothing runs on timeout; the value only bounds
/// how long the loop sleeps between checks (SPEC-0007 Q4).
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};

/// Opens one resizable window (kTitle, kSize, minimum kMinSize) and blocks in
/// waitEvents(kIdleTimeout) until the user asks to close it. Writes one line to `log` for each size
/// change of that window: "hello_window: resized to <w>x<h>". Returns 0 after the window was
/// closed, or 1 if it could not be created (error on stderr).
[[nodiscard]] int runHelloWindow(coil::platform::WindowSystem& windows, std::ostream& log);

} // namespace hello_window
