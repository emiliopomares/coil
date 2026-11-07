#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/testing/fake_clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace coil::platform::testing {

namespace {

std::unexpected<Error> notFound(WindowId id) {
    return makeUnexpected(ErrorCode::NotFound, std::format("no window with id {}", id.value));
}

std::unexpected<Error> invalidSize(WindowSize size) {
    return makeUnexpected(
        ErrorCode::InvalidArgument,
        std::format("window size must be positive, got {}x{}", size.width, size.height));
}

/// Each bound has positive dimensions, and min <= max in both dimensions (SPEC-0011 rule 1).
bool validLimits(const WindowSizeLimits& limits) {
    const auto positive = [](WindowSize size) { return size.width > 0 && size.height > 0; };
    if ((limits.min && !positive(*limits.min)) || (limits.max && !positive(*limits.max))) {
        return false;
    }
    if (limits.min && limits.max) {
        return limits.min->width <= limits.max->width && limits.min->height <= limits.max->height;
    }
    return true;
}

std::unexpected<Error> invalidLimits() {
    return makeUnexpected(ErrorCode::InvalidArgument,
                          "window size limits need positive bounds with min <= max");
}

/// `size` with each dimension clamped to the limits (SPEC-0011 rule 4).
WindowSize clamp(WindowSize size, const WindowSizeLimits& limits) {
    if (limits.min) {
        size.width = std::max(size.width, limits.min->width);
        size.height = std::max(size.height, limits.min->height);
    }
    if (limits.max) {
        size.width = std::min(size.width, limits.max->width);
        size.height = std::min(size.height, limits.max->height);
    }
    return size;
}

/// Queues `event`, replacing a pending event of the same type for the same window: at most one is
/// pending per window, and the newest goes to the end (SPEC-0010 rule 9, SPEC-0013 rule 4).
template <typename Event>
void queueReplacing(std::vector<WindowEvent>& events, Event event) {
    std::erase_if(events, [&event](const WindowEvent& pending) {
        const auto* same = std::get_if<Event>(&pending);
        return same != nullptr && same->window == event.window;
    });
    events.emplace_back(event);
}

/// Pixel size for a logical size at `scale`, rounded up as SDL does when it derives pixels from a
/// density (SPEC-0013 rule 7).
WindowSize toPixelSize(WindowSize size, float scale) {
    const auto toPixels = [scale](int units) {
        return static_cast<int>(std::ceil(static_cast<float>(units) * scale));
    };
    return WindowSize{.width = toPixels(size.width), .height = toPixels(size.height)};
}

constexpr Pixel kOpaqueBlack{.r = 0, .g = 0, .b = 0, .a = 255};

/// `pixel` composited over opaque black, rounded to nearest (SPEC-0022 rule 1).
Pixel overBlack(Pixel pixel) {
    const auto scaled = [alpha = pixel.a](std::uint8_t channel) {
        return static_cast<std::uint8_t>(((channel * alpha) + 127) / 255);
    };
    return Pixel{.r = scaled(pixel.r), .g = scaled(pixel.g), .b = scaled(pixel.b), .a = 255};
}

/// What a window of `pixels` shows for `image`: pixel for pixel from the top-left, composited over
/// black, cropped, and black where the image does not reach (SPEC-0022 rule 1).
Image showOnBlack(WindowSize pixels, const Image& image) {
    Image frame{static_cast<std::uint32_t>(pixels.width),
                static_cast<std::uint32_t>(pixels.height)};
    auto out = frame.pixels().begin();
    for (std::uint32_t y = 0; y < frame.height(); ++y) {
        for (std::uint32_t x = 0; x < frame.width(); ++x) {
            *out++ = image.pixel(x, y).transform(overBlack).value_or(kOpaqueBlack);
        }
    }
    return frame;
}

/// Makes `frame` opaque black if the window's pixel size no longer matches it (SPEC-0022 rule 4).
void fitFrame(Image& frame, WindowSize size, float scale) {
    const WindowSize pixels = toPixelSize(size, scale);
    if (std::cmp_not_equal(frame.width(), pixels.width) ||
        std::cmp_not_equal(frame.height(), pixels.height)) {
        frame = showOnBlack(pixels, Image{});
    }
}

} // namespace

FakeWindowSystem::FakeWindowSystem()
    : m_clock{&m_ownClock} {}

FakeWindowSystem::FakeWindowSystem(FakeClock& clock)
    : m_clock{&clock} {}

auto* FakeWindowSystem::find(this auto& self, WindowId id) {
    const auto it = std::ranges::find(self.m_windows, id, &Window::id);
    return it == self.m_windows.end() ? nullptr : &*it;
}

Expected<WindowId> FakeWindowSystem::createWindow(const WindowDesc& desc) {
    if (desc.size.width <= 0 || desc.size.height <= 0) {
        return invalidSize(desc.size);
    }
    if (!validLimits(desc.limits)) {
        return invalidLimits();
    }
    if (clamp(desc.size, desc.limits) != desc.size) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              std::format("window size {}x{} is outside its limits",
                                          desc.size.width, desc.size.height));
    }
    const WindowId id{.value = m_nextId++};
    m_windows.push_back(Window{.id = id,
                               .title = desc.title,
                               .size = desc.size,
                               .resizable = desc.resizable,
                               .limits = desc.limits,
                               .frame = showOnBlack(desc.size, Image{})});
    return id;
}

Expected<void> FakeWindowSystem::destroyWindow(WindowId id) {
    const auto removed =
        std::erase_if(m_windows, [id](const Window& window) { return window.id == id; });
    if (removed == 0) {
        return notFound(id);
    }
    return {};
}

std::vector<WindowId> FakeWindowSystem::windows() const {
    std::vector<WindowId> ids;
    ids.reserve(m_windows.size());
    for (const Window& window : m_windows) {
        ids.push_back(window.id);
    }
    return ids;
}

Expected<std::string> FakeWindowSystem::title(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->title;
}

Expected<void> FakeWindowSystem::setTitle(WindowId id, std::string_view title) {
    Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    window->title = title;
    return {};
}

Expected<WindowSize> FakeWindowSystem::size(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->size;
}

Expected<void> FakeWindowSystem::setSize(WindowId id, WindowSize size) {
    Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    if (size.width <= 0 || size.height <= 0) {
        return invalidSize(size);
    }
    resize(*window, clamp(size, window->limits));
    return {};
}

void FakeWindowSystem::resize(Window& window, WindowSize size) {
    if (window.size == size) {
        return;
    }
    window.size = size;
    fitFrame(window.frame, window.size, window.scale);
    queueReplacing(m_events, Resized{.window = window.id, .size = size});
}

Expected<bool> FakeWindowSystem::resizable(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->resizable;
}

Expected<WindowSizeLimits> FakeWindowSystem::sizeLimits(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->limits;
}

Expected<void> FakeWindowSystem::setSizeLimits(WindowId id, WindowSizeLimits limits) {
    Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    if (!validLimits(limits)) {
        return invalidLimits();
    }
    window->limits = limits;
    resize(*window, clamp(window->size, limits));
    return {};
}

Expected<WindowSize> FakeWindowSystem::pixelSize(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return toPixelSize(window->size, window->scale);
}

Expected<float> FakeWindowSystem::displayScale(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->scale;
}

Expected<void> FakeWindowSystem::presentImage(WindowId id, const Image& image) {
    Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    window->frame = showOnBlack(toPixelSize(window->size, window->scale), image);
    return {};
}

Expected<Image> FakeWindowSystem::framebuffer(WindowId id) const {
    const Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    return window->frame;
}

Expected<void> FakeWindowSystem::changeDisplayScale(WindowId id, float scale) {
    Window* window = find(id);
    if (window == nullptr) {
        return notFound(id);
    }
    if (!std::isfinite(scale) || scale <= 0.0F) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              std::format("display scale must be finite and > 0, got {}", scale));
    }
    if (window->scale != scale) {
        window->scale = scale;
        fitFrame(window->frame, window->size, window->scale);
        queueReplacing(m_events, ScaleChanged{.window = id, .scale = scale});
    }
    return {};
}

std::vector<WindowEvent> FakeWindowSystem::pollEvents() {
    std::vector<WindowEvent> events = std::exchange(m_events, {});
    // Events for windows destroyed since they were queued are dropped (SPEC-0006 rule 4).
    std::erase_if(events, [this](const WindowEvent& event) {
        const WindowId window =
            std::visit([](const auto& alternative) { return alternative.window; }, event);
        return find(window) == nullptr;
    });
    return events;
}

Expected<void> FakeWindowSystem::requestClose(WindowId id) {
    if (find(id) == nullptr) {
        return notFound(id);
    }
    m_events.emplace_back(CloseRequested{.window = id});
    return {};
}

Expected<void> FakeWindowSystem::userResize(WindowId id, WindowSize size) {
    return setSize(id, size); // Same rules as setSize (SPEC-0010 rule 8).
}

std::vector<WindowEvent> FakeWindowSystem::waitEvents(Clock::Duration timeout) {
    std::vector<WindowEvent> events = pollEvents();
    if (!events.empty() || timeout <= Clock::Duration::zero()) {
        return events;
    }
    // The fake would block here: let the test deliver an event "during" the wait (SPEC-0009 rule
    // 5).
    if (m_waitHook) {
        m_waitHook();
        events = pollEvents();
        if (!events.empty()) {
            return events;
        }
    }
    // Cannot fail: timeout is positive here.
    [[maybe_unused]] const Expected<void> advanced = m_clock->advance(timeout);
    return events;
}

std::vector<RawInputEvent> FakeWindowSystem::pollInput() {
    std::vector<RawInputEvent> input = std::exchange(m_input, {});
    // Input for windows destroyed since it was injected is dropped (SPEC-0026 rule 5).
    std::erase_if(input, [this](const RawInputEvent& event) {
        return find(std::visit([](const auto& alternative) { return alternative.window; },
                               event)) == nullptr;
    });
    return input;
}

Expected<void> FakeWindowSystem::injectInput(RawInputEvent event) {
    const WindowId window =
        std::visit([](const auto& alternative) { return alternative.window; }, event);
    if (find(window) == nullptr) {
        return notFound(window);
    }
    m_input.push_back(std::move(event));
    return {};
}

void FakeWindowSystem::setWaitHook(std::function<void()> hook) {
    m_waitHook = std::move(hook);
}

const FakeClock& FakeWindowSystem::clock() const {
    return *m_clock;
}

} // namespace coil::platform::testing
