#pragma once

#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace coil::platform::testing {

/// In-memory WindowSystem for tests. No OS resources; deterministic ids starting at 1.
class FakeWindowSystem final : public WindowSystem {
public:
    FakeWindowSystem() = default;
    ~FakeWindowSystem() override = default;

    FakeWindowSystem(const FakeWindowSystem&) = delete;
    FakeWindowSystem& operator=(const FakeWindowSystem&) = delete;
    FakeWindowSystem(FakeWindowSystem&&) = delete;
    FakeWindowSystem& operator=(FakeWindowSystem&&) = delete;

    [[nodiscard]] Expected<WindowId> createWindow(const WindowDesc& desc) override;
    [[nodiscard]] Expected<void> destroyWindow(WindowId id) override;
    [[nodiscard]] std::vector<WindowId> windows() const override;
    [[nodiscard]] Expected<std::string> title(WindowId id) const override;
    [[nodiscard]] Expected<void> setTitle(WindowId id, std::string_view title) override;
    [[nodiscard]] Expected<WindowSize> size(WindowId id) const override;
    [[nodiscard]] std::vector<WindowEvent> pollEvents() override;

    /// Test control: queues CloseRequested for `id`, as if the OS had sent it.
    /// Fails with NotFound if `id` is not a live window.
    [[nodiscard]] Expected<void> requestClose(WindowId id);

private:
    struct Window {
        WindowId id;
        std::string title;
        WindowSize size;
    };

    /// The live window with `id`, or nullptr.
    [[nodiscard]] auto* find(this auto& self, WindowId id);

    std::vector<Window> m_windows; ///< Live windows, in creation order.
    std::uint64_t m_nextId = 1;    ///< Next id to issue; ids are never reused.
};

} // namespace coil::platform::testing
