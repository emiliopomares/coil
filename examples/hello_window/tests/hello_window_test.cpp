#include "hello_window.hpp"

#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>

#include <gtest/gtest.h>

#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using coil::ErrorCode;
using coil::Expected;
using coil::makeUnexpected;
using coil::platform::Clock;
using coil::platform::WindowDesc;
using coil::platform::WindowEvent;
using coil::platform::WindowId;
using coil::platform::WindowSize;
using coil::platform::WindowSystem;
using coil::platform::testing::FakeWindowSystem;
using hello_window::kIdleTimeout;
using hello_window::runHelloWindow;

/// After this many event calls the program is considered stuck (SPEC-0007 §6 loop guard).
constexpr int kMaxEventCalls = 100;

/// Test double around a real WindowSystem: forwards every call, counts event calls, records wait
/// timeouts, and runs a test callback (the "user") before forwarding each event call.
class ObservedWindowSystem final : public WindowSystem {
public:
    using CloseRequester = std::function<void(WindowId)>;
    using BeforeEvents = std::function<void(int call)>;

    /// `requestClose` simulates the OS close request for a window of `inner`; used by the loop
    /// guard.
    ObservedWindowSystem(WindowSystem& inner, CloseRequester requestClose)
        : m_inner(inner)
        , m_requestClose(std::move(requestClose)) {}

    /// Runs before each event call, with the 1-based number of that call.
    void setBeforeEvents(BeforeEvents beforeEvents) { m_beforeEvents = std::move(beforeEvents); }

    [[nodiscard]] int createCalls() const { return m_createCalls; }

    [[nodiscard]] int eventCalls() const { return m_eventCalls; }

    [[nodiscard]] int pollCalls() const { return m_pollCalls; }

    [[nodiscard]] const std::vector<Clock::Duration>& waitTimeouts() const {
        return m_waitTimeouts;
    }

    [[nodiscard]] Expected<WindowId> createWindow(const WindowDesc& desc) override {
        ++m_createCalls;
        return m_inner.createWindow(desc);
    }

    [[nodiscard]] Expected<void> destroyWindow(WindowId id) override {
        return m_inner.destroyWindow(id);
    }

    [[nodiscard]] std::vector<WindowId> windows() const override { return m_inner.windows(); }

    [[nodiscard]] Expected<std::string> title(WindowId id) const override {
        return m_inner.title(id);
    }

    [[nodiscard]] Expected<void> setTitle(WindowId id, std::string_view title) override {
        return m_inner.setTitle(id, title);
    }

    [[nodiscard]] Expected<WindowSize> size(WindowId id) const override { return m_inner.size(id); }

    [[nodiscard]] Expected<void> setSize(WindowId id, WindowSize size) override {
        return m_inner.setSize(id, size);
    }

    [[nodiscard]] std::vector<WindowEvent> pollEvents() override {
        ++m_pollCalls;
        beforeEventCall();
        return m_inner.pollEvents();
    }

    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration timeout) override {
        m_waitTimeouts.push_back(timeout);
        beforeEventCall();
        return m_inner.waitEvents(timeout);
    }

private:
    void beforeEventCall() {
        ++m_eventCalls;
        if (m_beforeEvents) {
            m_beforeEvents(m_eventCalls);
        }
        if (m_eventCalls >= kMaxEventCalls) {
            if (m_eventCalls == kMaxEventCalls) {
                ADD_FAILURE() << "loop guard: still running after " << kMaxEventCalls
                              << " event calls";
            }
            for (const WindowId id : m_inner.windows()) {
                m_requestClose(id);
            }
        }
    }

    WindowSystem& m_inner;
    CloseRequester m_requestClose;
    BeforeEvents m_beforeEvents;
    int m_createCalls = 0;
    int m_eventCalls = 0;
    int m_pollCalls = 0;
    std::vector<Clock::Duration> m_waitTimeouts;
};

/// WindowSystem whose createWindow always fails; everything else reports "no such window".
class FailingWindowSystem final : public WindowSystem {
public:
    [[nodiscard]] Expected<WindowId> createWindow(const WindowDesc& /*desc*/) override {
        return makeUnexpected(ErrorCode::Unavailable, "no display (test double)");
    }

    [[nodiscard]] Expected<void> destroyWindow(WindowId id) override { return notFound(id); }

    [[nodiscard]] std::vector<WindowId> windows() const override { return {}; }

    [[nodiscard]] Expected<std::string> title(WindowId id) const override { return notFound(id); }

    [[nodiscard]] Expected<void> setTitle(WindowId id, std::string_view /*title*/) override {
        return notFound(id);
    }

    [[nodiscard]] Expected<WindowSize> size(WindowId id) const override { return notFound(id); }

    [[nodiscard]] Expected<void> setSize(WindowId id, WindowSize /*size*/) override {
        return notFound(id);
    }

    [[nodiscard]] std::vector<WindowEvent> pollEvents() override { return {}; }

    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration /*timeout*/) override {
        return {};
    }

private:
    [[nodiscard]] static std::unexpected<coil::Error> notFound(WindowId id) {
        return makeUnexpected(ErrorCode::NotFound, std::format("no window with id {}", id.value));
    }
};

/// Close requester for windows of a FakeWindowSystem.
ObservedWindowSystem::CloseRequester closeOnFake(FakeWindowSystem& fake) {
    return [&fake](WindowId id) { EXPECT_TRUE(fake.requestClose(id).has_value()); };
}

/// Close requester for windows of an SDL window system: pushes a real SDL close-request event.
ObservedWindowSystem::CloseRequester closeOnSdl(WindowSystem& sdl) {
    return [&sdl](WindowId id) {
        const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
        ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        event.window.windowID = *sdlId;
        ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
    };
}

TEST(HelloWindow, AC1_OpensOneTitledWindow) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    bool checked = false;
    observed.setBeforeEvents([&](int call) {
        if (call != 1) {
            return;
        }
        const auto ids = fake.windows();
        ASSERT_EQ(ids.size(), 1U);
        EXPECT_EQ(fake.title(ids.front()), "Coil");
        EXPECT_EQ(fake.size(ids.front()), (WindowSize{.width = 800, .height = 600}));
        checked = true;
        EXPECT_TRUE(fake.requestClose(ids.front()).has_value());
    });

    EXPECT_EQ(runHelloWindow(observed), 0);
    EXPECT_TRUE(checked) << "runHelloWindow never asked for events";
}

TEST(HelloWindow, AC2_WaitsUntilItsWindowIsClosed) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    observed.setBeforeEvents([&](int call) {
        if (call == 3) {
            for (const WindowId id : fake.windows()) {
                EXPECT_TRUE(fake.requestClose(id).has_value());
            }
        }
    });

    EXPECT_EQ(runHelloWindow(observed), 0);
    EXPECT_EQ(observed.eventCalls(), 3);
    EXPECT_EQ(observed.pollCalls(), 0);
    EXPECT_EQ(observed.waitTimeouts(), std::vector<Clock::Duration>(3, kIdleTimeout));
    EXPECT_EQ(fake.clock().now(), Clock::TimePoint{} + (2 * kIdleTimeout));
    EXPECT_TRUE(fake.windows().empty());
}

TEST(HelloWindow, AC3_IgnoresCloseRequestsForOtherWindows) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    std::optional<WindowId> programWindow;
    std::optional<WindowId> otherWindow;
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            const auto ids = fake.windows();
            ASSERT_EQ(ids.size(), 1U);
            programWindow = ids.front();
            const auto other = fake.createWindow(
                WindowDesc{.title = "Other", .size = {.width = 64, .height = 64}});
            ASSERT_TRUE(other.has_value());
            otherWindow = *other;
            EXPECT_TRUE(fake.requestClose(*otherWindow).has_value());
        } else if (call == 2 && programWindow) {
            EXPECT_TRUE(fake.requestClose(*programWindow).has_value());
        }
    });

    EXPECT_EQ(runHelloWindow(observed), 0);
    EXPECT_EQ(observed.eventCalls(), 2);
    ASSERT_TRUE(otherWindow.has_value());
    EXPECT_EQ(fake.windows(), std::vector<WindowId>{otherWindow.value_or(WindowId{})});
}

TEST(HelloWindow, AC4_FailsWhenWindowCannotBeCreated) {
    FailingWindowSystem failing;
    ObservedWindowSystem observed{failing, [](WindowId /*id*/) {}};

    EXPECT_EQ(runHelloWindow(observed), 1);
    EXPECT_EQ(observed.createCalls(), 1);
    EXPECT_EQ(observed.eventCalls(), 0);
}

TEST(HelloWindow, AC5_RunsOnSdlUntilClosed) {
    auto sdl = coil::platform_sdl3::createSdlWindowSystem({.videoDriver = "dummy"});
    ASSERT_TRUE(sdl.has_value()) << std::format("{}", sdl.error());
    WindowSystem& system = **sdl;
    ObservedWindowSystem observed{system, closeOnSdl(system)};
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            for (const WindowId id : system.windows()) {
                closeOnSdl(system)(id);
            }
        }
    });

    EXPECT_EQ(runHelloWindow(observed), 0);
    EXPECT_EQ(observed.createCalls(), 1);
    EXPECT_TRUE(system.windows().empty());
}

} // namespace
