#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
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

std::unexpected<Error> invalidSize(platform::WindowSize size) {
    return makeUnexpected(
        ErrorCode::InvalidArgument,
        std::format("window size must be positive, got {}x{}", size.width, size.height));
}

/// Positive timeout in whole milliseconds, rounded up and clamped to SDL's Sint32 range
/// (SPEC-0009 rule 7). Callers never pass zero or negative values.
Sint32 toSdlTimeout(platform::Clock::Duration timeout) {
    const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(timeout).count();
    return static_cast<Sint32>(
        std::min<std::int64_t>(milliseconds, std::numeric_limits<Sint32>::max()));
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

    /// The live window owning `handle`, or nullptr.
    [[nodiscard]] const Window* findByHandle(const SDL_Window* handle) const {
        const auto it = std::ranges::find(m_windows, handle,
                                          [](const Window& window) { return window.handle.get(); });
        return it == m_windows.end() ? nullptr : &*it;
    }

    /// Appends the Coil event for `event`, if it maps to one for a live window (SPEC-0006 rules 4,
    /// 5).
    void translate(const SDL_Event& event, std::vector<platform::WindowEvent>& events) const {
        if (event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.type != SDL_EVENT_WINDOW_RESIZED) {
            return;
        }
        const Window* window = findByHandle(SDL_GetWindowFromEvent(&event));
        if (window == nullptr) {
            return;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            events.emplace_back(platform::CloseRequested{.window = window->id});
        } else {
            // SDL reports the new logical size in data1/data2 (SPEC-0010 rule 7).
            events.emplace_back(platform::Resized{
                .window = window->id,
                .size = {.width = event.window.data1, .height = event.window.data2}});
        }
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
            return invalidSize(desc.size);
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

    [[nodiscard]] Expected<void> setSize(platform::WindowId id,
                                         platform::WindowSize size) override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        if (size.width <= 0 || size.height <= 0) {
            return invalidSize(size);
        }
        // SDL reports the change as SDL_EVENT_WINDOW_RESIZED, and sends none for an unchanged size.
        if (!SDL_SetWindowSize(window->handle.get(), size.width, size.height)) {
            return sdlFailure(ErrorCode::Internal, "SDL_SetWindowSize failed");
        }
        return {};
    }

    [[nodiscard]] Expected<bool> resizable(platform::WindowId /*id*/) const override {
        return false; // Red stub (SPEC-0011).
    }

    [[nodiscard]] Expected<platform::WindowSizeLimits>
    sizeLimits(platform::WindowId /*id*/) const override {
        return platform::WindowSizeLimits{}; // Red stub (SPEC-0011).
    }

    [[nodiscard]] Expected<void> setSizeLimits(platform::WindowId /*id*/,
                                               platform::WindowSizeLimits /*limits*/) override {
        return {}; // Red stub (SPEC-0011).
    }

    [[nodiscard]] std::vector<platform::WindowEvent> pollEvents() override {
        std::vector<platform::WindowEvent> events;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            translate(event, events);
        }
        return events;
    }

    [[nodiscard]] std::vector<platform::WindowEvent>
    waitEvents(platform::Clock::Duration timeout) override {
        std::vector<platform::WindowEvent> events = pollEvents();
        if (!events.empty() || timeout <= platform::Clock::Duration::zero()) {
            return events;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, toSdlTimeout(timeout))) {
            translate(event, events);
            while (SDL_PollEvent(&event)) {
                translate(event, events);
            }
        }
        return events;
    }

    /// SDL's id for a live window (SPEC-0006 sdlWindowId).
    [[nodiscard]] Expected<std::uint32_t> sdlWindowId(platform::WindowId id) const {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        const SDL_WindowID sdlId = SDL_GetWindowID(window->handle.get());
        if (sdlId == 0) {
            return sdlFailure(ErrorCode::Internal, "SDL_GetWindowID failed");
        }
        return sdlId;
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

Expected<std::uint32_t> sdlWindowId(const platform::WindowSystem& system, platform::WindowId id) {
    const auto* sdlSystem = dynamic_cast<const SdlWindowSystem*>(&system);
    if (sdlSystem == nullptr) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              "window system was not created by createSdlWindowSystem");
    }
    return sdlSystem->sdlWindowId(id);
}

} // namespace coil::platform_sdl3
