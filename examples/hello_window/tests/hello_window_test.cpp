#include "hello_window.hpp"

#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <sstream>
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
using coil::platform::WindowSizeLimits;
using coil::platform::WindowSystem;
using coil::platform::testing::FakeWindowSystem;
using hello_window::kIdleTimeout;
using hello_window::kMinSize;
using hello_window::runHelloWindow;

/// The scale line written once the window exists, for an 800x600 window at scale 1 (SPEC-0014).
constexpr std::string_view kStartLine = "hello_window: 800x600 pixels at scale 1\n";

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

    [[nodiscard]] Expected<bool> resizable(WindowId id) const override {
        return m_inner.resizable(id);
    }

    [[nodiscard]] Expected<WindowSizeLimits> sizeLimits(WindowId id) const override {
        return m_inner.sizeLimits(id);
    }

    [[nodiscard]] Expected<void> setSizeLimits(WindowId id, WindowSizeLimits limits) override {
        return m_inner.setSizeLimits(id, limits);
    }

    [[nodiscard]] Expected<WindowSize> pixelSize(WindowId id) const override {
        return m_inner.pixelSize(id);
    }

    [[nodiscard]] Expected<float> displayScale(WindowId id) const override {
        return m_inner.displayScale(id);
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

    [[nodiscard]] Expected<bool> resizable(WindowId id) const override { return notFound(id); }

    [[nodiscard]] Expected<WindowSizeLimits> sizeLimits(WindowId id) const override {
        return notFound(id);
    }

    [[nodiscard]] Expected<void> setSizeLimits(WindowId id, WindowSizeLimits /*limits*/) override {
        return notFound(id);
    }

    [[nodiscard]] Expected<WindowSize> pixelSize(WindowId id) const override {
        return notFound(id);
    }

    [[nodiscard]] Expected<float> displayScale(WindowId id) const override { return notFound(id); }

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

/// Resizes a window of an SDL window system directly, as a user drag would (outside the port).
void resizeOnSdl(WindowSystem& sdl, WindowId id, WindowSize size) {
    const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
    ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
    SDL_Window* window = SDL_GetWindowFromID(*sdlId);
    ASSERT_NE(window, nullptr) << SDL_GetError();
    ASSERT_TRUE(SDL_SetWindowSize(window, size.width, size.height)) << SDL_GetError();
}

/// The program's window on `system`, which must be the only one (else a test failure, id 0).
WindowId onlyWindow(const WindowSystem& system) {
    const auto ids = system.windows();
    EXPECT_EQ(ids.size(), 1U);
    return ids.empty() ? WindowId{} : ids.front();
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

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
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

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
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

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(observed.eventCalls(), 2);
    ASSERT_TRUE(otherWindow.has_value());
    EXPECT_EQ(fake.windows(), std::vector<WindowId>{otherWindow.value_or(WindowId{})});
}

TEST(HelloWindow, AC4_FailsWhenWindowCannotBeCreated) {
    FailingWindowSystem failing;
    ObservedWindowSystem observed{failing, [](WindowId /*id*/) {}};

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 1);
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

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(observed.createCalls(), 1);
    EXPECT_TRUE(system.windows().empty());
}

// SPEC-0012: resizable window and size log (R<n> = SPEC-0012 AC-<n>).

TEST(HelloWindow, R1_WindowIsResizableWithMinimumSize) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    bool checked = false;
    observed.setBeforeEvents([&](int call) {
        if (call != 1) {
            return;
        }
        const WindowId program = onlyWindow(fake);
        const auto resizable = fake.resizable(program);
        ASSERT_TRUE(resizable.has_value());
        EXPECT_TRUE(*resizable) << "the window is not resizable";
        const auto limits = fake.sizeLimits(program);
        ASSERT_TRUE(limits.has_value());
        EXPECT_TRUE(*limits == (WindowSizeLimits{.min = kMinSize}))
            << "the limits are not exactly {min 320x240, no max}";
        checked = true;
        EXPECT_TRUE(fake.requestClose(program).has_value());
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_TRUE(checked) << "runHelloWindow never asked for events";
}

TEST(HelloWindow, R2_LogsEachResizeOfItsWindow) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    observed.setBeforeEvents([&](int call) {
        const WindowId program = onlyWindow(fake);
        if (call == 1) {
            EXPECT_TRUE(fake.userResize(program, {.width = 1024, .height = 768}).has_value());
        } else if (call == 2) {
            EXPECT_TRUE(fake.userResize(program, {.width = 640, .height = 480}).has_value());
        } else if (call == 3) {
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(),
              std::string{kStartLine} +
                  "hello_window: resized to 1024x768\nhello_window: resized to 640x480\n");
}

TEST(HelloWindow, R3_IgnoresResizesOfOtherWindows) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    std::optional<WindowId> programWindow;
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            programWindow = onlyWindow(fake);
            const auto other =
                fake.createWindow({.title = "Other", .size = {.width = 64, .height = 64}});
            ASSERT_TRUE(other.has_value());
            EXPECT_TRUE(fake.userResize(*other, {.width = 500, .height = 400}).has_value());
        } else if (call == 2 && programWindow) {
            EXPECT_TRUE(fake.requestClose(*programWindow).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), kStartLine);
}

TEST(HelloWindow, R4_LogsTheClampedSize) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    observed.setBeforeEvents([&](int call) {
        const WindowId program = onlyWindow(fake);
        if (call == 1) {
            EXPECT_TRUE(fake.userResize(program, {.width = 100, .height = 100}).has_value());
        } else if (call == 2) {
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), std::string{kStartLine} + "hello_window: resized to 320x240\n");
}

TEST(HelloWindow, R5_LogsResizesOnSdl) {
    auto sdl = coil::platform_sdl3::createSdlWindowSystem({.videoDriver = "dummy"});
    ASSERT_TRUE(sdl.has_value()) << std::format("{}", sdl.error());
    WindowSystem& system = **sdl;
    ObservedWindowSystem observed{system, closeOnSdl(system)};
    observed.setBeforeEvents([&](int call) {
        const WindowId program = onlyWindow(system);
        if (call == 1) {
            resizeOnSdl(system, program, {.width = 1024, .height = 768});
        } else if (call == 2) {
            closeOnSdl(system)(program);
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), std::string{kStartLine} + "hello_window: resized to 1024x768\n");
}

// SPEC-0014: pixel size and scale lines (S<n> = SPEC-0014 AC-<n>).

TEST(HelloWindow, S1_PrintsPixelSizeAndScaleAtStart) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    std::ostringstream log;
    bool checked = false;
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            EXPECT_EQ(log.str(), kStartLine);
            checked = true;
            EXPECT_TRUE(fake.requestClose(onlyWindow(fake)).has_value());
        }
    });

    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_TRUE(checked) << "runHelloWindow never asked for events";
}

TEST(HelloWindow, S2_PrintsScaleChanges) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    observed.setBeforeEvents([&](int call) {
        const WindowId program = onlyWindow(fake);
        if (call == 1) {
            EXPECT_TRUE(fake.changeDisplayScale(program, 2.0F).has_value());
        } else if (call == 2) {
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), std::string{kStartLine} + "hello_window: 1600x1200 pixels at scale 2\n");
}

TEST(HelloWindow, S3_IgnoresScaleChangesOfOtherWindows) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    std::optional<WindowId> programWindow;
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            programWindow = onlyWindow(fake);
            const auto other =
                fake.createWindow({.title = "Other", .size = {.width = 64, .height = 64}});
            ASSERT_TRUE(other.has_value());
            EXPECT_TRUE(fake.changeDisplayScale(*other, 2.0F).has_value());
        } else if (call == 2 && programWindow) {
            EXPECT_TRUE(fake.requestClose(*programWindow).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), kStartLine);
}

TEST(HelloWindow, S4_LinesFollowEventOrder) {
    FakeWindowSystem fake;
    ObservedWindowSystem observed{fake, closeOnFake(fake)};
    observed.setBeforeEvents([&](int call) {
        const WindowId program = onlyWindow(fake);
        if (call == 1) {
            EXPECT_TRUE(fake.userResize(program, {.width = 1024, .height = 768}).has_value());
            EXPECT_TRUE(fake.changeDisplayScale(program, 1.5F).has_value());
        } else if (call == 2) {
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_EQ(log.str(), std::string{kStartLine} + "hello_window: resized to 1024x768\n"
                                                   "hello_window: 1536x1152 pixels at scale 1.5\n");
}

TEST(HelloWindow, S5_PrintsScaleOnSdl) {
    auto sdl = coil::platform_sdl3::createSdlWindowSystem({.videoDriver = "dummy"});
    ASSERT_TRUE(sdl.has_value()) << std::format("{}", sdl.error());
    WindowSystem& system = **sdl;
    ObservedWindowSystem observed{system, closeOnSdl(system)};
    observed.setBeforeEvents([&](int call) {
        if (call == 1) {
            closeOnSdl(system)(onlyWindow(system));
        }
    });

    std::ostringstream log;
    EXPECT_EQ(runHelloWindow(observed, log), 0);
    EXPECT_TRUE(log.str().starts_with(kStartLine)) << log.str();
}

} // namespace
