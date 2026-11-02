#include "hello_window.hpp"

#include <coil/platform/window_system.hpp>

#include <algorithm>
#include <cstdio>
#include <ostream>
#include <print>
#include <string>
#include <variant>

namespace hello_window {

namespace {

/// Writes the pixel-size/scale line (SPEC-0014 rules 1, 2). A failed query goes to stderr.
void logScale(const coil::platform::WindowSystem& windows, coil::platform::WindowId window,
              float scale, std::ostream& log) {
    const auto pixels = windows.pixelSize(window);
    if (!pixels) {
        std::println(stderr, "hello_window: cannot query pixel size: {}", pixels.error());
        return;
    }
    std::println(log, "hello_window: {}x{} pixels at scale {}", pixels->width, pixels->height,
                 scale);
}

} // namespace

int runHelloWindow(coil::platform::WindowSystem& windows, std::ostream& log) {
    const auto window = windows.createWindow({.title = std::string{kTitle},
                                              .size = kSize,
                                              .resizable = true,
                                              .limits = {.min = kMinSize}});
    if (!window) {
        std::println(stderr, "hello_window: cannot create window: {}", window.error());
        return 1;
    }
    if (const auto scale = windows.displayScale(*window); scale) {
        logScale(windows, *window, *scale, log);
    } else {
        std::println(stderr, "hello_window: cannot query display scale: {}", scale.error());
    }
    const coil::platform::WindowEvent closeRequested =
        coil::platform::CloseRequested{.window = *window};
    while (true) {
        const auto events = windows.waitEvents(kIdleTimeout);
        for (const coil::platform::WindowEvent& event : events) {
            if (const auto* resized = std::get_if<coil::platform::Resized>(&event);
                resized != nullptr && resized->window == *window) {
                std::println(log, "hello_window: resized to {}x{}", resized->size.width,
                             resized->size.height);
            } else if (const auto* rescaled = std::get_if<coil::platform::ScaleChanged>(&event);
                       rescaled != nullptr && rescaled->window == *window) {
                logScale(windows, *window, rescaled->scale, log);
            }
        }
        if (std::ranges::contains(events, closeRequested)) {
            // The program is ending either way: a failed destroy is reported but does not change
            // the exit code (SPEC-0007 §4.3).
            if (const auto destroyed = windows.destroyWindow(*window); !destroyed) {
                std::println(stderr, "hello_window: cannot destroy window: {}", destroyed.error());
            }
            return 0;
        }
    }
}

} // namespace hello_window
