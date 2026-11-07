#include "intents.hpp"

#include <coil/core/intent.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <cstdio>
#include <ostream>
#include <print>
#include <string>
#include <variant>
#include <vector>

namespace intents {

// Red-phase stub (SPEC-0029): a wrong description (the alternative's index).
std::string describe(const coil::core::Intent& intent) {
    return std::to_string(intent.index());
}

// Red-phase stub (SPEC-0029): opens the window and closes it on request, writing nothing.
int runIntents(coil::platform::WindowSystem& windows, coil::platform::InputSource& input,
               std::ostream& log) {
    const auto window = windows.createWindow(
        coil::platform::WindowDesc{.title = std::string{kTitle}, .size = kSize, .resizable = true});
    if (!window) {
        std::println(stderr, "intents: cannot create window: {}", window.error());
        return 1;
    }
    for (;;) {
        const std::vector<coil::platform::WindowEvent> events = windows.waitEvents(kIdleTimeout);
        static_cast<void>(input.pollInput());
        for (const auto& event : events) {
            if (const auto* close = std::get_if<coil::platform::CloseRequested>(&event);
                close != nullptr && close->window == *window) {
                if (const auto destroyed = windows.destroyWindow(*window); !destroyed) {
                    std::println(stderr, "intents: cannot destroy window: {}", destroyed.error());
                }
                log.flush();
                return 0;
            }
        }
    }
}

} // namespace intents
