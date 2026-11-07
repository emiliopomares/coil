#include "intents.hpp"

#include <coil/core/intent.hpp>
#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Printers live in Image's namespace, where GoogleTest finds them by ADL.
namespace coil::foundation {
inline namespace {

std::ostream& operator<<(std::ostream& os, Pixel pixel) {
    return os << "Pixel{" << static_cast<int>(pixel.r) << ", " << static_cast<int>(pixel.g) << ", "
              << static_cast<int>(pixel.b) << ", " << static_cast<int>(pixel.a) << "}";
}

} // namespace
} // namespace coil::foundation

namespace {

using coil::ErrorCode;
using coil::Expected;
using coil::Image;
using coil::makeUnexpected;
using coil::Pixel;
using coil::Point;
using coil::core::Intent;
using coil::platform::Clock;
using coil::platform::Key;
using coil::platform::KeyModifiers;
using coil::platform::KeyPressed;
using coil::platform::KeyReleased;
using coil::platform::PointerButton;
using coil::platform::PointerId;
using coil::platform::RawInputEvent;
using coil::platform::WindowDesc;
using coil::platform::WindowEvent;
using coil::platform::WindowId;
using coil::platform::WindowSize;
using coil::platform::WindowSizeLimits;
using coil::platform::WindowSystem;
using coil::platform::testing::FakeWindowSystem;
using intents::describe;
using intents::runIntents;

/// After this many waits the program is considered stuck (loop guard).
constexpr int kMaxWaits = 100;

/// The background, written out from SPEC-0029 §4.1 (not taken from the example).
constexpr Pixel kBackground{.r = 0x1E, .g = 0x1E, .b = 0x24, .a = 255};

/// The log split into lines (without their newlines).
std::vector<std::string> linesOf(const std::string& log) {
    std::vector<std::string> lines;
    std::istringstream stream{log};
    for (std::string line; std::getline(stream, line);) {
        lines.push_back(line);
    }
    return lines;
}

struct RunResult {
    int exitCode = -1;
    std::vector<std::string> lines;
};

/// Runs the example on `fake`. `script(wait, program, logSoFar)` runs each time the program would
/// block in a wait (1-based count), with the program's window (the first one created). A stuck
/// program is closed after kMaxWaits waits, with a test failure.
RunResult runScripted(
    FakeWindowSystem& fake,
    const std::function<void(int wait, WindowId program, const std::string& logSoFar)>& script) {
    int waits = 0;
    std::ostringstream log;
    fake.setWaitHook([&] {
        ++waits;
        const std::vector<WindowId> ids = fake.windows();
        if (ids.empty()) {
            return;
        }
        if (waits >= kMaxWaits) {
            if (waits == kMaxWaits) {
                ADD_FAILURE() << "loop guard: still running after " << kMaxWaits << " waits";
            }
            EXPECT_TRUE(fake.requestClose(ids.front()).has_value());
            return;
        }
        script(waits, ids.front(), log.str());
    });
    const int exitCode = runIntents(fake, fake, log);
    fake.setWaitHook({});
    return {.exitCode = exitCode, .lines = linesOf(log.str())};
}

void inject(FakeWindowSystem& fake, RawInputEvent event) {
    EXPECT_TRUE(fake.injectInput(std::move(event)).has_value());
}

void close(FakeWindowSystem& fake, WindowId program) {
    EXPECT_TRUE(fake.requestClose(program).has_value());
}

/// Expects the window's framebuffer to be `width` x `height`, every pixel kBackground.
void expectBackground(const FakeWindowSystem& fake, WindowId id, std::uint32_t width,
                      std::uint32_t height) {
    const Expected<Image> frame = fake.framebuffer(id);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->width(), width);
    EXPECT_EQ(frame->height(), height);
    std::size_t wrong = 0;
    for (const Pixel pixel : frame->pixels()) {
        if (pixel != kBackground) {
            ++wrong;
        }
    }
    EXPECT_EQ(wrong, 0U) << "pixels that are not the background; first pixel "
                         << frame->pixel(0, 0).value_or(Pixel{});
}

KeyPressed press(WindowId window, Key key, KeyModifiers modifiers = {}) {
    return KeyPressed{.window = window, .key = key, .modifiers = modifiers};
}

/// I1's script: checks the first frame, changes the scale, checks again, resizes, then closes.
void scaleThenResize(FakeWindowSystem& fake, int wait, WindowId program,
                     const std::string& logSoFar, bool& checked) {
    if (wait == 1) {
        EXPECT_EQ(logSoFar, "intents: window 640x400 at scale 1\n");
        expectBackground(fake, program, 640, 400);
        EXPECT_TRUE(fake.changeDisplayScale(program, 2.0F).has_value());
    } else if (wait == 2) {
        expectBackground(fake, program, 1280, 800);
        EXPECT_TRUE(fake.userResize(program, WindowSize{.width = 500, .height = 300}).has_value());
    } else {
        checked = true;
        close(fake, program);
    }
}

TEST(Intents, I1_WindowLineAndBackground) {
    FakeWindowSystem fake;
    bool checked = false;
    const RunResult run =
        runScripted(fake, [&](int wait, WindowId program, const std::string& logSoFar) {
            scaleThenResize(fake, wait, program, logSoFar, checked);
        });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(checked);
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: window 640x400 at scale 2",
                             "intents: window 500x300 at scale 2",
                         }));
}

TEST(Intents, I2_KeysPrintTheirIntents) {
    FakeWindowSystem fake;
    bool keptRunning = false;
    const RunResult run = runScripted(fake, [&](int wait, WindowId program, const std::string&) {
        if (wait == 1) {
            for (const Key key : {Key::Enter, Key::Space, Key::Escape, Key::Tab}) {
                inject(fake, press(program, key));
            }
            inject(fake, press(program, Key::Tab, KeyModifiers{.shift = true}));
            for (const Key key : {Key::Up, Key::Down, Key::Left, Key::Right, Key::A}) {
                inject(fake, press(program, key));
            }
            inject(fake, KeyReleased{.window = program, .key = Key::Enter});
            inject(fake, KeyReleased{.window = program, .key = Key::Escape});
        } else {
            keptRunning = true;
            close(fake, program);
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(keptRunning) << "the program stopped after Escape";
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: Activate",
                             "intents: Activate",
                             "intents: Cancel",
                             "intents: Navigate Next",
                             "intents: Navigate Previous",
                             "intents: Navigate Up",
                             "intents: Navigate Down",
                             "intents: Navigate Left",
                             "intents: Navigate Right",
                         }));
}

TEST(Intents, I3_PointerAndScrollPrintInDips) {
    FakeWindowSystem fake;
    constexpr PointerId kMouse{.value = 1};
    constexpr Point kAt{.x = 10.5F, .y = 20.0F};
    const RunResult run = runScripted(fake, [&](int wait, WindowId program, const std::string&) {
        if (wait == 1) {
            inject(fake, coil::platform::PointerMoved{
                             .window = program, .pointer = kMouse, .position = kAt});
            inject(fake, coil::platform::PointerPressed{.window = program,
                                                        .pointer = kMouse,
                                                        .button = PointerButton::Primary,
                                                        .position = kAt});
            inject(fake, coil::platform::PointerReleased{.window = program,
                                                         .pointer = kMouse,
                                                         .button = PointerButton::Secondary,
                                                         .position = kAt});
            inject(fake, coil::platform::PointerScrolled{.window = program,
                                                         .pointer = kMouse,
                                                         .position = kAt,
                                                         .deltaX = 0.0F,
                                                         .deltaY = -1.5F});
        } else {
            close(fake, program);
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: PointerMove 1 at (10.5, 20)",
                             "intents: PointerDown 1 Primary at (10.5, 20)",
                             "intents: PointerUp 1 Secondary at (10.5, 20)",
                             "intents: Scroll 0, -1.5 at (10.5, 20)",
                         }));
}

TEST(Intents, I4_TextPrintsUnchanged) {
    FakeWindowSystem fake;
    const RunResult run = runScripted(fake, [&](int wait, WindowId program, const std::string&) {
        if (wait == 1) {
            inject(fake, coil::platform::TextEntered{.window = program, .text = "\xC3\xA9"});
            inject(fake, coil::platform::TextEntered{.window = program, .text = "ab"});
        } else {
            close(fake, program);
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: TextInput \"\xC3\xA9\"",
                             "intents: TextInput \"ab\"",
                         }));
}

TEST(Intents, I5_DescribeEveryAlternative) {
    using coil::core::NavigationDirection;
    constexpr WindowId kWindow{.value = 3};
    constexpr PointerId kPen{.value = 42};
    constexpr Point kAt{.x = 1.0F, .y = 2.25F};
    const std::vector<std::pair<Intent, std::string_view>> table{
        {coil::core::Activate{.window = kWindow}, "intents: Activate"},
        {coil::core::Cancel{.window = kWindow}, "intents: Cancel"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Up},
         "intents: Navigate Up"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Down},
         "intents: Navigate Down"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Left},
         "intents: Navigate Left"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Right},
         "intents: Navigate Right"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Next},
         "intents: Navigate Next"},
        {coil::core::Navigate{.window = kWindow, .direction = NavigationDirection::Previous},
         "intents: Navigate Previous"},
        {coil::core::Scroll{.window = kWindow, .position = kAt, .deltaX = 2.0F, .deltaY = 0.25F},
         "intents: Scroll 2, 0.25 at (1, 2.25)"},
        {coil::core::PointerMove{.window = kWindow, .pointer = kPen, .position = kAt},
         "intents: PointerMove 42 at (1, 2.25)"},
        {coil::core::PointerDown{
             .window = kWindow, .pointer = kPen, .button = PointerButton::Primary, .position = kAt},
         "intents: PointerDown 42 Primary at (1, 2.25)"},
        {coil::core::PointerDown{.window = kWindow,
                                 .pointer = kPen,
                                 .button = PointerButton::Secondary,
                                 .position = kAt},
         "intents: PointerDown 42 Secondary at (1, 2.25)"},
        {coil::core::PointerDown{
             .window = kWindow, .pointer = kPen, .button = PointerButton::Middle, .position = kAt},
         "intents: PointerDown 42 Middle at (1, 2.25)"},
        {coil::core::PointerUp{
             .window = kWindow, .pointer = kPen, .button = PointerButton::Back, .position = kAt},
         "intents: PointerUp 42 Back at (1, 2.25)"},
        {coil::core::PointerUp{
             .window = kWindow, .pointer = kPen, .button = PointerButton::Forward, .position = kAt},
         "intents: PointerUp 42 Forward at (1, 2.25)"},
        {coil::core::TextInput{.window = kWindow, .text = "x y"}, "intents: TextInput \"x y\""},
    };
    for (const auto& [intent, expected] : table) {
        EXPECT_EQ(describe(intent), expected);
    }
}

/// I6, first half: input and a close request in the same iteration.
void expectCloseAfterTheBatch() {
    FakeWindowSystem fake;
    const RunResult run =
        runScripted(fake, [&](int /*wait*/, WindowId program, const std::string& /*log*/) {
            inject(fake, press(program, Key::Enter));
            close(fake, program);
        });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: Activate",
                         }));
    EXPECT_TRUE(fake.windows().empty());
}

/// I6's second script: input for another window (ignored) and for the program (printed).
void inputForTwoWindows(FakeWindowSystem& fake, int wait, WindowId program) {
    if (wait != 1) {
        close(fake, program);
        return;
    }
    const Expected<WindowId> other =
        fake.createWindow(WindowDesc{.title = "other", .size = {.width = 100, .height = 100}});
    ASSERT_TRUE(other.has_value());
    inject(fake, press(*other, Key::Enter));
    inject(fake, coil::platform::TextEntered{.window = *other, .text = "z"});
    inject(fake, press(program, Key::Escape));
}

/// I6, second half: intents for other windows are not written.
void expectOtherWindowsIgnored() {
    FakeWindowSystem fake;
    const RunResult run =
        runScripted(fake, [&](int wait, WindowId program, const std::string& /*log*/) {
            inputForTwoWindows(fake, wait, program);
        });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_EQ(run.lines, (std::vector<std::string>{
                             "intents: window 640x400 at scale 1",
                             "intents: Cancel",
                         }));
}

TEST(Intents, I6_CloseAfterTheBatchAndOtherWindowsIgnored) {
    expectCloseAfterTheBatch();
    expectOtherWindowsIgnored();
}

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

    [[nodiscard]] Expected<void> presentImage(WindowId id, const Image& /*image*/) override {
        return notFound(id);
    }

    [[nodiscard]] std::vector<WindowEvent> pollEvents() override { return {}; }

    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration /*timeout*/) override {
        ++m_waits;
        return {};
    }

    [[nodiscard]] int waits() const { return m_waits; }

private:
    [[nodiscard]] static std::unexpected<coil::Error> notFound(WindowId id) {
        return makeUnexpected(ErrorCode::NotFound, std::format("no window with id {}", id.value));
    }

    int m_waits = 0;
};

/// Forwards to an SDL window system and runs `beforeWait(wait)` (1-based) before each wait.
class ScriptedSdl final : public WindowSystem {
public:
    ScriptedSdl(WindowSystem& inner, std::function<void(int wait)> beforeWait)
        : m_inner(inner)
        , m_beforeWait(std::move(beforeWait)) {}

    [[nodiscard]] Expected<WindowId> createWindow(const WindowDesc& desc) override {
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

    [[nodiscard]] Expected<void> presentImage(WindowId id, const Image& image) override {
        return m_inner.presentImage(id, image);
    }

    [[nodiscard]] std::vector<WindowEvent> pollEvents() override { return m_inner.pollEvents(); }

    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration timeout) override {
        m_beforeWait(++m_waits);
        return m_inner.waitEvents(timeout);
    }

private:
    WindowSystem& m_inner;
    std::function<void(int wait)> m_beforeWait;
    int m_waits = 0;
};

void pushSdl(SDL_Event& event) {
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
}

/// Pushes a real SDL Enter key-down event for a window of `sdl`.
void pushEnterOnSdl(const WindowSystem& sdl, WindowId id) {
    const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
    ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = *sdlId;
    event.key.key = SDLK_RETURN;
    event.key.down = true;
    pushSdl(event);
}

/// Pushes a real SDL close-request event for a window of `sdl`.
void closeOnSdl(const WindowSystem& sdl, WindowId id) {
    const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
    ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = *sdlId;
    pushSdl(event);
}

TEST(Intents, I7_FailureAndSdlDummyDriver) {
    {
        FailingWindowSystem failing;
        FakeWindowSystem input;
        std::ostringstream log;
        EXPECT_EQ(runIntents(failing, input, log), 1);
        EXPECT_EQ(log.str(), "");
        EXPECT_EQ(failing.waits(), 0);
    }
    {
        auto sdl = coil::platform_sdl3::createSdlWindowSystem({.videoDriver = "dummy"});
        ASSERT_TRUE(sdl.has_value()) << std::format("{}", sdl.error());
        WindowSystem& system = **sdl;
        auto input = coil::platform_sdl3::inputSourceOf(system);
        ASSERT_TRUE(input.has_value()) << std::format("{}", input.error());
        ScriptedSdl scripted{system, [&](int wait) {
                                 const std::vector<WindowId> ids = system.windows();
                                 if (ids.empty()) {
                                     return;
                                 }
                                 if (wait == 1) {
                                     pushEnterOnSdl(system, ids.front());
                                 }
                                 if (wait == 1 || wait >= kMaxWaits) {
                                     closeOnSdl(system, ids.front());
                                 }
                             }};
        std::ostringstream log;
        EXPECT_EQ(runIntents(scripted, input->get(), log), 0);
        const std::vector<std::string> lines = linesOf(log.str());
        ASSERT_FALSE(lines.empty());
        EXPECT_EQ(lines.back(), "intents: Activate");
        EXPECT_TRUE(system.windows().empty());
    }
}

} // namespace
