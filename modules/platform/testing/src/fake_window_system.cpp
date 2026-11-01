#include <coil/foundation/error.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace coil::platform::testing {

// SPEC-0003 Red stubs: compile, but store nothing and report success with neutral values.

Expected<WindowId> FakeWindowSystem::createWindow(const WindowDesc& /*desc*/) {
    return WindowId{};
}

Expected<void> FakeWindowSystem::destroyWindow(WindowId /*id*/) {
    return {};
}

std::vector<WindowId> FakeWindowSystem::windows() const {
    return {};
}

Expected<std::string> FakeWindowSystem::title(WindowId /*id*/) const {
    return std::string{};
}

Expected<void> FakeWindowSystem::setTitle(WindowId /*id*/, std::string_view /*title*/) {
    return {};
}

Expected<WindowSize> FakeWindowSystem::size(WindowId /*id*/) const {
    return WindowSize{};
}

} // namespace coil::platform::testing
