#pragma once

#include <coil/platform/clock.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/render/color.hpp>
#include <coil/render_software/software_render_backend.hpp>

#include <chrono>
#include <ostream>
#include <string_view>

namespace boxes {

inline constexpr std::string_view kTitle = "Coil boxes";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};
/// The window can't be resized below this.
inline constexpr coil::platform::WindowSize kMinSize{.width = 320, .height = 240};
/// How long one wait may block when nothing happens; nothing is drawn on timeout (SPEC-0023 Q4).
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};

/// The colours of the scene (opaque, sRGB; SPEC-0023 §4.2).
inline constexpr coil::render::Color kBackground = coil::render::Color::fromHex(0x1E1E24FFU);
inline constexpr coil::render::Color kHeader = coil::render::Color::fromHex(0x3B82F6FFU);
inline constexpr coil::render::Color kSidebar = coil::render::Color::fromHex(0x10B981FFU);
inline constexpr coil::render::Color kTopPanel = coil::render::Color::fromHex(0xF59E0BFFU);
inline constexpr coil::render::Color kBottomPanel = coil::render::Color::fromHex(0xEF4444FFU);
inline constexpr coil::render::Color kFooter = coil::render::Color::fromHex(0x8B5CF6FFU);

/// Opens one resizable window (kTitle, kSize, minimum kMinSize), draws the scene of SPEC-0023 §4.2
/// and redraws it whenever the window's size or display scale changes, until the user asks to close
/// it. Writes "boxes: frame <pw>x<ph> at scale <s>" to `log` for each frame drawn. Returns 0 after
/// the window was closed, or 1 if the window or render target could not be created (error on
/// stderr).
[[nodiscard]] int runBoxes(coil::platform::WindowSystem& windows,
                           coil::render_software::SoftwareRenderBackend& renderer,
                           std::ostream& log);

} // namespace boxes
