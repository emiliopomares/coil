#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>

#include <algorithm>
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
    const WindowId id{.value = m_nextId++};
    m_windows.push_back(Window{.id = id, .title = desc.title, .size = desc.size});
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
    if (window->size != size) {
        window->size = size;
        // A pending Resized for this window is superseded by the new one (SPEC-0010 rule 9).
        std::erase_if(m_events, [id](const WindowEvent& event) {
            const auto* resized = std::get_if<Resized>(&event);
            return resized != nullptr && resized->window == id;
        });
        m_events.emplace_back(Resized{.window = id, .size = size});
    }
    return {};
}

Expected<bool> FakeWindowSystem::resizable(WindowId /*id*/) const {
    return false; // Red stub (SPEC-0011).
}

Expected<WindowSizeLimits> FakeWindowSystem::sizeLimits(WindowId /*id*/) const {
    return WindowSizeLimits{}; // Red stub (SPEC-0011).
}

Expected<void> FakeWindowSystem::setSizeLimits(WindowId /*id*/, WindowSizeLimits /*limits*/) {
    return {}; // Red stub (SPEC-0011).
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

void FakeWindowSystem::setWaitHook(std::function<void()> hook) {
    m_waitHook = std::move(hook);
}

const FakeClock& FakeWindowSystem::clock() const {
    return *m_clock;
}

} // namespace coil::platform::testing
