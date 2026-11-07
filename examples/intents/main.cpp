#include "intents.hpp"

#include <coil/foundation/error.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <cstdio>
#include <iostream>
#include <string_view>

namespace {

void writeToStderr(std::string_view text) noexcept {
    // A failed write to stderr cannot be reported anywhere, so its result is deliberately unused.
    static_cast<void>(std::fwrite(text.data(), 1, text.size(), stderr));
}

/// Prints "intents: <Code>[: <message>]" to stderr. Unlike std::println it cannot throw, so
/// nothing escapes main.
void reportError(const coil::Error& error) noexcept {
    writeToStderr("intents: ");
    writeToStderr(coil::foundation::toString(error.code()));
    if (!error.message().empty()) {
        writeToStderr(": ");
        writeToStderr(error.message());
    }
    writeToStderr("\n");
}

} // namespace

/// Composition root (ADR-0004): the only place that chooses the SDL3 backend.
int main() {
    auto windows = coil::platform_sdl3::createSdlWindowSystem();
    if (!windows) {
        reportError(windows.error());
        return 1;
    }
    auto input = coil::platform_sdl3::inputSourceOf(**windows);
    if (!input) {
        reportError(input.error());
        return 1;
    }
    return intents::runIntents(**windows, input->get(), std::cout);
}
