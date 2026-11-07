#include "intents.hpp"

#include <coil/core/input_mapper.hpp>
#include <coil/core/intent.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <format>
#include <ostream>
#include <print>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace intents {

namespace {

using coil::Point;
using coil::core::Intent;
using coil::core::NavigationDirection;
using coil::platform::PointerButton;
using coil::platform::WindowEvent;
using coil::platform::WindowId;
using coil::platform::WindowSystem;

std::string_view nameOf(NavigationDirection direction) {
    switch (direction) {
    case NavigationDirection::Up:
        return "Up";
    case NavigationDirection::Down:
        return "Down";
    case NavigationDirection::Left:
        return "Left";
    case NavigationDirection::Right:
        return "Right";
    case NavigationDirection::Next:
        return "Next";
    case NavigationDirection::Previous:
        return "Previous";
    }
    return "?";
}

std::string_view nameOf(PointerButton button) {
    switch (button) {
    case PointerButton::Primary:
        return "Primary";
    case PointerButton::Secondary:
        return "Secondary";
    case PointerButton::Middle:
        return "Middle";
    case PointerButton::Back:
        return "Back";
    case PointerButton::Forward:
        return "Forward";
    }
    return "?";
}

/// "(x, y)", numbers in their shortest form (§4.2).
std::string pointText(Point point) {
    return std::format("({}, {})", point.x, point.y);
}

/// One line per alternative (§4.2), without the "intents: " prefix.
struct Describer {
    std::string operator()(const coil::core::Activate& /*intent*/) const { return "Activate"; }

    std::string operator()(const coil::core::Cancel& /*intent*/) const { return "Cancel"; }

    std::string operator()(const coil::core::Navigate& intent) const {
        return std::format("Navigate {}", nameOf(intent.direction));
    }

    std::string operator()(const coil::core::Scroll& intent) const {
        return std::format("Scroll {}, {} at {}", intent.deltaX, intent.deltaY,
                           pointText(intent.position));
    }

    std::string operator()(const coil::core::PointerMove& intent) const {
        return std::format("PointerMove {} at {}", intent.pointer.value,
                           pointText(intent.position));
    }

    std::string operator()(const coil::core::PointerDown& intent) const {
        return std::format("PointerDown {} {} at {}", intent.pointer.value, nameOf(intent.button),
                           pointText(intent.position));
    }

    std::string operator()(const coil::core::PointerUp& intent) const {
        return std::format("PointerUp {} {} at {}", intent.pointer.value, nameOf(intent.button),
                           pointText(intent.position));
    }

    std::string operator()(const coil::core::TextInput& intent) const {
        return std::format("TextInput \"{}\"", intent.text);
    }
};

/// Fills the window with kBackground at its pixel size and writes the window line (rules 1, 3).
/// A failure goes to stderr, and the line is skipped (rule 6).
void showWindowState(WindowSystem& windows, WindowId window, std::ostream& log) {
    const auto size = windows.size(window);
    if (!size) {
        std::println(stderr, "intents: cannot query the window size: {}", size.error());
        return;
    }
    const auto scale = windows.displayScale(window);
    if (!scale) {
        std::println(stderr, "intents: cannot query the display scale: {}", scale.error());
        return;
    }
    const auto pixels = windows.pixelSize(window);
    if (!pixels) {
        std::println(stderr, "intents: cannot query the pixel size: {}", pixels.error());
        return;
    }
    // Window sizes are always > 0 (WindowSystem contract), so the casts keep their values.
    coil::Image background{static_cast<std::uint32_t>(pixels->width),
                           static_cast<std::uint32_t>(pixels->height)};
    std::ranges::fill(background.pixels(), kBackground);
    if (const auto shown = windows.presentImage(window, background); !shown) {
        std::println(stderr, "intents: cannot present the background: {}", shown.error());
        return;
    }
    std::println(log, "intents: window {}x{} at scale {}", size->width, size->height, *scale);
}

/// Whether `event` changes the size or scale of `window` (rule 3).
bool changesWindow(const WindowEvent& event, WindowId window) {
    if (const auto* resized = std::get_if<coil::platform::Resized>(&event)) {
        return resized->window == window;
    }
    if (const auto* rescaled = std::get_if<coil::platform::ScaleChanged>(&event)) {
        return rescaled->window == window;
    }
    return false;
}

bool closes(const WindowEvent& event, WindowId window) {
    const auto* close = std::get_if<coil::platform::CloseRequested>(&event);
    return close != nullptr && close->window == window;
}

WindowId windowOf(const Intent& intent) {
    return std::visit([](const auto& alternative) { return alternative.window; }, intent);
}

} // namespace

std::string describe(const Intent& intent) {
    return "intents: " + std::visit(Describer{}, intent);
}

int runIntents(WindowSystem& windows, coil::platform::InputSource& input, std::ostream& log) {
    const auto window = windows.createWindow(
        coil::platform::WindowDesc{.title = std::string{kTitle}, .size = kSize, .resizable = true});
    if (!window) {
        std::println(stderr, "intents: cannot create window: {}", window.error());
        return 1;
    }
    showWindowState(windows, *window, log);
    const coil::core::InputMapper mapper;
    for (;;) {
        const std::vector<WindowEvent> events = windows.waitEvents(kIdleTimeout);
        for (const coil::platform::RawInputEvent& raw : input.pollInput()) {
            for (const Intent& intent : mapper.map(raw)) {
                if (windowOf(intent) == *window) {
                    std::println(log, "{}", describe(intent));
                }
            }
        }
        if (std::ranges::any_of(events,
                                [&](const WindowEvent& e) { return changesWindow(e, *window); })) {
            showWindowState(windows, *window, log);
        }
        if (std::ranges::any_of(events, [&](const WindowEvent& e) { return closes(e, *window); })) {
            if (const auto destroyed = windows.destroyWindow(*window); !destroyed) {
                std::println(stderr, "intents: cannot destroy window: {}", destroyed.error());
            }
            log.flush();
            return 0;
        }
    }
}

} // namespace intents
