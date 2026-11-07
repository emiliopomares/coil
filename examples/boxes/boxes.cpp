#include "boxes.hpp"

#include <coil/platform/window_system.hpp>
#include <coil/render_software/software_render_backend.hpp>

#include <algorithm>
#include <cstdio>
#include <ostream>
#include <print>
#include <string>

namespace boxes {

int runBoxes(coil::platform::WindowSystem& windows,
             coil::render_software::SoftwareRenderBackend& renderer, std::ostream& log) {
    // Red stub (SPEC-0023): opens the window and waits for its close request; draws nothing.
    if (const auto none = renderer.destroyTarget({}); none) { // NotFound: no target exists
        return 1;
    }
    log.flush();
    const auto window = windows.createWindow({.title = std::string{kTitle},
                                              .size = kSize,
                                              .resizable = true,
                                              .limits = {.min = kMinSize}});
    if (!window) {
        std::println(stderr, "boxes: cannot create window: {}", window.error());
        return 1;
    }
    const coil::platform::WindowEvent closeRequested =
        coil::platform::CloseRequested{.window = *window};
    while (true) {
        if (std::ranges::contains(windows.waitEvents(kIdleTimeout), closeRequested)) {
            return 0;
        }
    }
}

} // namespace boxes
