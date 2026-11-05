#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
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
#include <variant>
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

/// Each bound has positive dimensions, and min <= max in both dimensions (SPEC-0011 rule 1).
bool validLimits(const platform::WindowSizeLimits& limits) {
    const auto positive = [](platform::WindowSize size) {
        return size.width > 0 && size.height > 0;
    };
    if ((limits.min && !positive(*limits.min)) || (limits.max && !positive(*limits.max))) {
        return false;
    }
    if (limits.min && limits.max) {
        return limits.min->width <= limits.max->width && limits.min->height <= limits.max->height;
    }
    return true;
}

bool withinLimits(platform::WindowSize size, const platform::WindowSizeLimits& limits) {
    if (limits.min && (size.width < limits.min->width || size.height < limits.min->height)) {
        return false;
    }
    return !limits.max || (size.width <= limits.max->width && size.height <= limits.max->height);
}

std::unexpected<Error> invalidLimits() {
    return makeUnexpected(ErrorCode::InvalidArgument,
                          "window size limits need positive bounds with min <= max");
}

/// Applies valid limits to an SDL window; 0x0 means "no bound" to SDL. SDL refuses a minimum above
/// the current maximum, so the maximum is cleared first (SPEC-0011 rule 8). SDL then clamps the
/// window's size, reporting any change as SDL_EVENT_WINDOW_RESIZED.
Expected<void> applyLimits(SDL_Window* window, const platform::WindowSizeLimits& limits) {
    const platform::WindowSize none{};
    const platform::WindowSize min = limits.min.value_or(none);
    const platform::WindowSize max = limits.max.value_or(none);
    if (!SDL_SetWindowMaximumSize(window, 0, 0) ||
        !SDL_SetWindowMinimumSize(window, min.width, min.height) ||
        !SDL_SetWindowMaximumSize(window, max.width, max.height)) {
        return sdlFailure(ErrorCode::Internal, "cannot set window size limits");
    }
    return {};
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
        platform::WindowSizeLimits limits; ///< As last set through the port.
        float reportedScale = 0.0F;        ///< Last scale reported: at creation or by ScaleChanged.
    };

    /// The live window with `id`, or nullptr.
    [[nodiscard]] auto* find(this auto& self, platform::WindowId id) {
        const auto it = std::ranges::find(self.m_windows, id, &Window::id);
        return it == self.m_windows.end() ? nullptr : &*it;
    }

    /// The live window owning `handle`, or nullptr.
    [[nodiscard]] auto* findByHandle(this auto& self, const SDL_Window* handle) {
        const auto it = std::ranges::find(self.m_windows, handle,
                                          [](const Window& window) { return window.handle.get(); });
        return it == self.m_windows.end() ? nullptr : &*it;
    }

    /// Appends the Coil event for `event`, if it maps to one for a live window (SPEC-0006 rules 4,
    /// 5).
    void translate(const SDL_Event& event, std::vector<platform::WindowEvent>& events) {
        if (event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.type != SDL_EVENT_WINDOW_RESIZED &&
            event.type != SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            return;
        }
        Window* window = findByHandle(SDL_GetWindowFromEvent(&event));
        if (window == nullptr) {
            return;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            events.emplace_back(platform::CloseRequested{.window = window->id});
        } else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            // The SDL event carries no value; a failed query (0.0) drops it (SPEC-0013 rule 8).
            // An unchanged scale is not a change (rule 4, amendment C): SDL also queues the scale
            // change that happens while a high-density window is being created.
            const float scale = SDL_GetWindowDisplayScale(window->handle.get());
            if (scale > 0.0F && scale != window->reportedScale) {
                window->reportedScale = scale;
                // SDL does not coalesce these: keep one per window, the newest last (rule 4).
                std::erase_if(events, [id = window->id](const platform::WindowEvent& pending) {
                    const auto* same = std::get_if<platform::ScaleChanged>(&pending);
                    return same != nullptr && same->window == id;
                });
                events.emplace_back(platform::ScaleChanged{.window = window->id, .scale = scale});
            }
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
        if (!validLimits(desc.limits)) {
            return invalidLimits();
        }
        if (!withinLimits(desc.size, desc.limits)) {
            return makeUnexpected(ErrorCode::InvalidArgument,
                                  std::format("window size {}x{} is outside its limits",
                                              desc.size.width, desc.size.height));
        }
        // Full-density back buffer where the display has one, e.g. Retina (SPEC-0013 rule 9).
        SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        SdlWindowPtr handle{
            SDL_CreateWindow(desc.title.c_str(), desc.size.width, desc.size.height, flags)};
        if (!handle) {
            return sdlFailure(ErrorCode::Internal, "SDL_CreateWindow failed");
        }
        if (desc.limits != platform::WindowSizeLimits{}) {
            if (auto applied = applyLimits(handle.get(), desc.limits); !applied) {
                return std::unexpected{std::move(applied.error())};
            }
        }
        const platform::WindowId id{.value = m_nextId++};
        const float scale = SDL_GetWindowDisplayScale(handle.get());
        m_windows.push_back(Window{
            .id = id, .handle = std::move(handle), .limits = desc.limits, .reportedScale = scale});
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

    [[nodiscard]] Expected<bool> resizable(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        return (SDL_GetWindowFlags(window->handle.get()) & SDL_WINDOW_RESIZABLE) != 0;
    }

    [[nodiscard]] Expected<platform::WindowSizeLimits>
    sizeLimits(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        return window->limits;
    }

    [[nodiscard]] Expected<void> setSizeLimits(platform::WindowId id,
                                               platform::WindowSizeLimits limits) override {
        Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        if (!validLimits(limits)) {
            return invalidLimits();
        }
        if (auto applied = applyLimits(window->handle.get(), limits); !applied) {
            return applied;
        }
        window->limits = limits;
        return {};
    }

    [[nodiscard]] Expected<platform::WindowSize> pixelSize(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        platform::WindowSize result;
        if (!SDL_GetWindowSizeInPixels(window->handle.get(), &result.width, &result.height)) {
            return sdlFailure(ErrorCode::Internal, "SDL_GetWindowSizeInPixels failed");
        }
        return result;
    }

    [[nodiscard]] Expected<float> displayScale(platform::WindowId id) const override {
        const Window* window = find(id);
        if (window == nullptr) {
            return notFound(id);
        }
        const float scale = SDL_GetWindowDisplayScale(window->handle.get());
        if (scale <= 0.0F) {
            return sdlFailure(ErrorCode::Internal, "SDL_GetWindowDisplayScale failed");
        }
        return scale;
    }

    [[nodiscard]] Expected<void> presentImage(platform::WindowId id, const Image& image) override {
        static_cast<void>(id);
        static_cast<void>(image);
        return {}; // Red stub (SPEC-0022)
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
