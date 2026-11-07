#include "boxes.hpp"

#include <coil/foundation/error.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>
#include <coil/render_software/software_render_backend.hpp>

#include <cstdio>
#include <iostream>
#include <string_view>

namespace {

void writeToStderr(std::string_view text) noexcept {
    // A failed write to stderr cannot be reported anywhere, so its result is deliberately unused.
    static_cast<void>(std::fwrite(text.data(), 1, text.size(), stderr));
}

/// Prints "boxes: <Code>[: <message>]" to stderr. Unlike std::println it cannot throw, so
/// nothing escapes main.
void reportError(const coil::Error& error) noexcept {
    writeToStderr("boxes: ");
    writeToStderr(coil::foundation::toString(error.code()));
    if (!error.message().empty()) {
        writeToStderr(": ");
        writeToStderr(error.message());
    }
    writeToStderr("\n");
}

} // namespace

/// Composition root (ADR-0004): the only place that chooses the SDL3 and software backends.
int main() {
    auto windows = coil::platform_sdl3::createSdlWindowSystem();
    if (!windows) {
        reportError(windows.error());
        return 1;
    }
    coil::render_software::SoftwareRenderBackend renderer;
    return boxes::runBoxes(**windows, renderer, std::cout);
}
