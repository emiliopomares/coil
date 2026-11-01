#include <coil/foundation/error.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>

#include <algorithm>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace coil::platform::testing {

namespace {

std::unexpected<Error> notFound(WindowId id) {
    return makeUnexpected(ErrorCode::NotFound, std::format("no window with id {}", id.value));
}

} // namespace

auto* FakeWindowSystem::find(this auto& self, WindowId id) {
    const auto it = std::ranges::find(self.m_windows, id, &Window::id);
    return it == self.m_windows.end() ? nullptr : &*it;
}

Expected<WindowId> FakeWindowSystem::createWindow(const WindowDesc& desc) {
    if (desc.size.width <= 0 || desc.size.height <= 0) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              std::format("window size must be positive, got {}x{}",
                                          desc.size.width, desc.size.height));
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

} // namespace coil::platform::testing
