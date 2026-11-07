#pragma once

#include <coil/core/intent.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <chrono>
#include <ostream>
#include <string>
#include <string_view>

namespace intents {

inline constexpr std::string_view kTitle = "Coil intents";
inline constexpr coil::platform::WindowSize kSize{.width = 640, .height = 400};
/// How long one wait may block when nothing happens.
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};
/// The window's fill (opaque, sRGB #1E1E24, as in boxes).
inline constexpr coil::foundation::Pixel kBackground{.r = 0x1E, .g = 0x1E, .b = 0x24, .a = 0xFF};

/// Formats one intent as a log line, without the trailing newline (SPEC-0029 §4.2).
[[nodiscard]] std::string describe(const coil::core::Intent& intent);

/// Opens one resizable window (kTitle, kSize) filled with kBackground and, until the user asks to
/// close it, writes one line per intent that the default InputMapper produces from `input` for that
/// window. Returns 0 after the window was closed, or 1 if it could not be created (error on
/// stderr).
[[nodiscard]] int runIntents(coil::platform::WindowSystem& windows,
                             coil::platform::InputSource& input, std::ostream& log);

} // namespace intents
