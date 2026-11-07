#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/testing/fake_clock.hpp>
#include <coil/platform/window_system.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace coil::platform::testing {

/// In-memory WindowSystem for tests. No OS resources; deterministic ids starting at 1. Also the
/// InputSource for its windows (SPEC-0026 Q1).
class FakeWindowSystem final : public WindowSystem, public InputSource {
public:
    /// Uses its own FakeClock, starting at the time point zero.
    FakeWindowSystem();
    /// Waits advance `clock`, which must outlive this object.
    explicit FakeWindowSystem(FakeClock& clock);
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
    [[nodiscard]] Expected<void> setSize(WindowId id, WindowSize size) override;
    [[nodiscard]] Expected<bool> resizable(WindowId id) const override;
    [[nodiscard]] Expected<WindowSizeLimits> sizeLimits(WindowId id) const override;
    [[nodiscard]] Expected<void> setSizeLimits(WindowId id, WindowSizeLimits limits) override;
    [[nodiscard]] Expected<WindowSize> pixelSize(WindowId id) const override;
    [[nodiscard]] Expected<float> displayScale(WindowId id) const override;
    [[nodiscard]] Expected<void> presentImage(WindowId id, const Image& image) override;
    [[nodiscard]] std::vector<WindowEvent> pollEvents() override;
    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration timeout) override;
    [[nodiscard]] std::vector<RawInputEvent> pollInput() override;

    /// Test control: queues `event` for pollInput(), as if a device produced it. Fails with
    /// NotFound (queuing nothing) if the event's window is not live.
    [[nodiscard]] Expected<void> injectInput(RawInputEvent event);

    /// Test control: queues CloseRequested for `id`, as if the OS had sent it.
    /// Fails with NotFound if `id` is not a live window.
    [[nodiscard]] Expected<void> requestClose(WindowId id);

    /// Test control: resizes `id` as if the user dragged its edge. Same rules and errors as
    /// setSize.
    [[nodiscard]] Expected<void> userResize(WindowId id, WindowSize size);

    /// Test control: changes `id`'s display scale as if it moved to another display. Fails with
    /// NotFound for an unknown window and InvalidArgument unless `scale` is finite and > 0.
    [[nodiscard]] Expected<void> changeDisplayScale(WindowId id, float scale);

    /// Test control: what the window shows, at its pixel size. Opaque black before any present and
    /// after each pixel-size change. Fails with NotFound for an unknown window.
    [[nodiscard]] Expected<Image> framebuffer(WindowId id) const;

    /// Test control: runs each time waitEvents would block (no pending events, timeout > 0), before
    /// time advances. If it queues events, waitEvents returns them without advancing time.
    void setWaitHook(std::function<void()> hook);

    /// The clock that waits advance.
    [[nodiscard]] const FakeClock& clock() const;

private:
    struct Window {
        WindowId id;
        std::string title;
        WindowSize size;
        bool resizable = false;
        WindowSizeLimits limits;
        float scale = 1.0F; ///< Display scale; pixels per DIP.
        Image frame;        ///< What the window shows, at its pixel size (SPEC-0022).
    };

    /// The live window with `id`, or nullptr.
    [[nodiscard]] auto* find(this auto& self, WindowId id);

    /// Sets the window's size (already valid and within its limits) and queues Resized if it
    /// changed, replacing any pending Resized for that window (SPEC-0010 rules 2, 4, 9).
    void resize(Window& window, WindowSize size);

    std::vector<Window> m_windows;      ///< Live windows, in creation order.
    std::vector<WindowEvent> m_events;  ///< Events not yet returned by pollEvents().
    std::vector<RawInputEvent> m_input; ///< Input not yet returned by pollInput().
    std::uint64_t m_nextId = 1;         ///< Next id to issue; ids are never reused.
    FakeClock m_ownClock;               ///< Used when no external clock is given.
    FakeClock* m_clock;                 ///< The clock waits advance (never null).
    std::function<void()> m_waitHook;   ///< Optional; see setWaitHook().
};

} // namespace coil::platform::testing
