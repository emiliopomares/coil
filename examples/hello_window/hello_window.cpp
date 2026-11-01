#include "hello_window.hpp"

#include <coil/platform/window_system.hpp>

#include <algorithm>
#include <cstdio>
#include <ostream>
#include <print>
#include <string>
#include <variant>

namespace hello_window {

int runHelloWindow(coil::platform::WindowSystem& windows, std::ostream& log) {
    const auto window = windows.createWindow({.title = std::string{kTitle},
                                              .size = kSize,
                                              .resizable = true,
                                              .limits = {.min = kMinSize}});
    if (!window) {
        std::println(stderr, "hello_window: cannot create window: {}", window.error());
        return 1;
    }
    const coil::platform::WindowEvent closeRequested =
        coil::platform::CloseRequested{.window = *window};
    while (true) {
        const auto events = windows.waitEvents(kIdleTimeout);
        for (const coil::platform::WindowEvent& event : events) {
            const auto* resized = std::get_if<coil::platform::Resized>(&event);
            if (resized != nullptr && resized->window == *window) {
                std::println(log, "hello_window: resized to {}x{}", resized->size.width,
                             resized->size.height);
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
