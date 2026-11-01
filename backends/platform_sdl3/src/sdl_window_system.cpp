#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coil::platform_sdl3 {

namespace {

struct SdlWindowDeleter {
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

using SdlWindowPtr = std::unique_ptr<SDL_Window, SdlWindowDeleter>;

std::unexpected<Error> sdlFailure(ErrorCode code, std::string_view what) {
    return makeUnexpected(code, std::format("{}: {}", what, SDL_GetError()));
}

std::unexpected<Error> notFound(platform::WindowId id) {
    return makeUnexpected(ErrorCode::NotFound, std::format("no window with id {}", id.value));
}

/// WindowSystem backed by SDL3. Owns SDL's video subsystem for its lifetime (SPEC-0005).
class SdlWindowSystem final : public platform::WindowSystem {
private:
    struct Window {
        platform::WindowId id;
        SdlWindowPtr handle;
    };

    /// The live window with `id`, or nullptr.
    [[nodiscard]] auto* find(this auto& self, platform::WindowId id) {
        const auto it = std::ranges::find(self.m_windows, id, &Window::id);
        return it == self.m_windows.end() ? nullptr : &*it;
    }

    std::vector<Window> m_windows; ///< Live windows, in creation order.
    std::uint64_t m_nextId = 1;    ///< Next id to issue; ids are never reused.

public:
    SdlWindowSystem() = default;

    ~SdlWindowSystem() override {
        m_windows.clear(); // Windows must be destroyed before the video subsystem shuts down.
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    SdlWindowSystem(const SdlWindowSystem&) = delete;
    SdlWindowSystem& operator=(const SdlWindowSystem&) = delete;
    SdlWindowSystem(SdlWindowSystem&&) = delete;
    SdlWindowSystem& operator=(SdlWindowSystem&&) = delete;

    [[nodiscard]] Expected<platform::WindowId>
    createWindow(const platform::WindowDesc& desc) override {
        if (desc.size.width <= 0 || desc.size.height <= 0) {
            return makeUnexpected(ErrorCode::InvalidArgument,
                                  std::format("window size must be positive, got {}x{}",
                                              desc.size.width, desc.size.height));
        }
        SdlWindowPtr handle{
            SDL_CreateWindow(desc.title.c_str(), desc.size.width, desc.size.height, 0)};
        if (!handle) {
            return sdlFailure(ErrorCode::Internal, "SDL_CreateWindow failed");
        }
        const platform::WindowId id{.value = m_nextId++};
        m_windows.push_back(Window{.id = id, .handle = std::move(handle)});
        return id;
    }

    [[nodiscard]] Expected<void> destroyWindow(platform::WindowId id) override {
        const auto removed =
            std::erase_if(m_windows, [id](const Window& window) { return window.id == id; });
        if (removed == 0) {
            return notFound(id);
        }
        return {};
    }

    [[nodiscard]] std::vector<platform::WindowId> windows() const override {
        std::vector<platform::WindowId> ids;
        ids.reserve(m_windows.size());
        for (const Window& window : m_windows) {
            ids.push_back(window.id);
        }
        return ids;
    }

    [[nodiscard]] Expected<std::string> title(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        return std::string{SDL_GetWindowTitle(window->handle.get())};
    }

    [[nodiscard]] Expected<void> setTitle(platform::WindowId id, std::string_view title) override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        const std::string terminated{title};
        if (!SDL_SetWindowTitle(window->handle.get(), terminated.c_str())) {
            return sdlFailure(ErrorCode::Internal, "SDL_SetWindowTitle failed");
        }
        return {};
    }

    [[nodiscard]] Expected<platform::WindowSize> size(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        platform::WindowSize result;
        if (!SDL_GetWindowSize(window->handle.get(), &result.width, &result.height)) {
            return sdlFailure(ErrorCode::Internal, "SDL_GetWindowSize failed");
        }
        return result;
    }

    [[nodiscard]] std::vector<platform::WindowEvent> pollEvents() override {
        return {}; // SPEC-0006 Red stub
    }
};

} // namespace

Expected<std::unique_ptr<platform::WindowSystem>>
createSdlWindowSystem(const SdlWindowSystemOptions& options) {
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        return makeUnexpected(ErrorCode::Unavailable, "SDL video subsystem is already initialized");
    }
    if (options.videoDriver.empty()) {
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, options.videoDriver.c_str());
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return sdlFailure(ErrorCode::Unavailable, "cannot initialize SDL video");
    }
    return std::make_unique<SdlWindowSystem>();
}

Expected<std::uint32_t> sdlWindowId(const platform::WindowSystem& /*system*/,
                                    platform::WindowId /*id*/) {
    return std::uint32_t{0}; // SPEC-0006 Red stub
}

} // namespace coil::platform_sdl3
