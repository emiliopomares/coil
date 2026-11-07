#include "boxes.hpp"

#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render_software/software_render_backend.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <ostream>
#include <span>
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

using boxes::runBoxes;
using coil::ErrorCode;
using coil::Expected;
using coil::Image;
using coil::makeUnexpected;
using coil::Pixel;
using coil::platform::Clock;
using coil::platform::WindowDesc;
using coil::platform::WindowEvent;
using coil::platform::WindowId;
using coil::platform::WindowSize;
using coil::platform::WindowSizeLimits;
using coil::platform::WindowSystem;
using coil::platform::testing::FakeWindowSystem;
using coil::render_software::SoftwareRenderBackend;

/// After this many waits the program is considered stuck (loop guard).
constexpr int kMaxWaits = 100;

// The scene's colours as bytes, written out from SPEC-0023 §4.2 (not taken from the example).
constexpr Pixel kBackground{.r = 0x1E, .g = 0x1E, .b = 0x24, .a = 255};
constexpr Pixel kHeader{.r = 0x3B, .g = 0x82, .b = 0xF6, .a = 255};
constexpr Pixel kSidebar{.r = 0x10, .g = 0xB9, .b = 0x81, .a = 255};
constexpr Pixel kTopPanel{.r = 0xF5, .g = 0x9E, .b = 0x0B, .a = 255};
constexpr Pixel kBottomPanel{.r = 0xEF, .g = 0x44, .b = 0x44, .a = 255};
constexpr Pixel kFooter{.r = 0x8B, .g = 0x5C, .b = 0xF6, .a = 255};

/// A box of the scene, in DIPs.
struct Box {
    std::string_view name;
    Pixel colour;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// SPEC-0023 §4.2 table.
constexpr std::array<Box, 5> kSceneAt800x600{{
    {.name = "header", .colour = kHeader, .x = 8, .y = 8, .width = 784, .height = 48},
    {.name = "sidebar", .colour = kSidebar, .x = 8, .y = 64, .width = 160, .height = 488},
    {.name = "top panel", .colour = kTopPanel, .x = 176, .y = 64, .width = 616, .height = 160},
    {.name = "bottom panel",
     .colour = kBottomPanel,
     .x = 176,
     .y = 232,
     .width = 616,
     .height = 320},
    {.name = "footer", .colour = kFooter, .x = 8, .y = 560, .width = 784, .height = 32},
}};

constexpr std::array<Box, 5> kSceneAt400x300{{
    {.name = "header", .colour = kHeader, .x = 8, .y = 8, .width = 384, .height = 48},
    {.name = "sidebar", .colour = kSidebar, .x = 8, .y = 64, .width = 160, .height = 188},
    {.name = "top panel", .colour = kTopPanel, .x = 176, .y = 64, .width = 216, .height = 60},
    {.name = "bottom panel",
     .colour = kBottomPanel,
     .x = 176,
     .y = 132,
     .width = 216,
     .height = 120},
    {.name = "footer", .colour = kFooter, .x = 8, .y = 260, .width = 384, .height = 32},
}};

void probe(const Image& frame, int x, int y, Pixel expected, std::string_view what) {
    const std::optional<Pixel> actual =
        (x < 0 || y < 0)
            ? std::nullopt
            : frame.pixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    EXPECT_EQ(actual, std::optional<Pixel>{expected}) << what << " at (" << x << ", " << y << ")";
}

/// Checks each box's centre, first and last pixels, and the background just outside it, at an
/// integer display `scale`.
void expectScene(const Image& frame, std::span<const Box> scene, int scale) {
    for (const Box& box : scene) {
        const int left = box.x * scale;
        const int top = box.y * scale;
        const int right = ((box.x + box.width) * scale) - 1;
        const int bottom = ((box.y + box.height) * scale) - 1;
        const std::string name{box.name};
        probe(frame, (left + right) / 2, (top + bottom) / 2, box.colour, name + " centre");
        probe(frame, left, top, box.colour, name + " first pixel");
        probe(frame, right, bottom, box.colour, name + " last pixel");
        probe(frame, left - 1, top, kBackground, "left of " + name);
        probe(frame, left, top - 1, kBackground, "above " + name);
        probe(frame, right + 1, bottom, kBackground, "right of " + name);
        probe(frame, right, bottom + 1, kBackground, "below " + name);
    }
}

/// What the fake shows for `id`; 0x0 (and a test failure) if it cannot be read.
Image framebufferOf(const FakeWindowSystem& fake, WindowId id) {
    Expected<Image> frame = fake.framebuffer(id);
    EXPECT_TRUE(frame.has_value());
    return std::move(frame).value_or(Image{});
}

struct RunResult {
    int exitCode = -1;
    std::string log;
};

/// Runs the example on `fake`. `script(wait, program)` runs each time the program would block in a
/// wait (1-based count), with the program's window (the first one created). A stuck program is
/// closed after kMaxWaits waits, with a test failure.
RunResult runScripted(FakeWindowSystem& fake, SoftwareRenderBackend& renderer,
                      const std::function<void(int wait, WindowId program)>& script) {
    int waits = 0;
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
        script(waits, ids.front());
    });
    std::ostringstream log;
    const int exitCode = runBoxes(fake, renderer, log);
    fake.setWaitHook({});
    return {.exitCode = exitCode, .log = log.str()};
}

TEST(Boxes, B1_FirstFrameShowsTheScene) {
    FakeWindowSystem fake;
    SoftwareRenderBackend renderer;
    bool checked = false;
    const RunResult run = runScripted(fake, renderer, [&](int wait, WindowId program) {
        if (wait != 1) {
            return;
        }
        const Image frame = framebufferOf(fake, program);
        EXPECT_EQ(frame.width(), 800U);
        EXPECT_EQ(frame.height(), 600U);
        expectScene(frame, kSceneAt800x600, 1);
        probe(frame, 4, 4, kBackground, "outer margin");
        probe(frame, 172, 300, kBackground, "gap between sidebar and content");
        probe(frame, 400, 228, kBackground, "gap between the panels");
        checked = true;
        EXPECT_TRUE(fake.requestClose(program).has_value());
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(checked) << "runBoxes never waited for events";
}

TEST(Boxes, B2_ResizeRedrawsAtTheNewLayout) {
    FakeWindowSystem fake;
    SoftwareRenderBackend renderer;
    bool checked = false;
    const RunResult run = runScripted(fake, renderer, [&](int wait, WindowId program) {
        if (wait == 1) {
            EXPECT_TRUE(fake.userResize(program, {.width = 400, .height = 300}).has_value());
        } else if (wait == 2) {
            const Image frame = framebufferOf(fake, program);
            EXPECT_EQ(frame.width(), 400U);
            EXPECT_EQ(frame.height(), 300U);
            expectScene(frame, kSceneAt400x300, 1);
            checked = true;
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(checked);
}

TEST(Boxes, B3_ScaleChangeRedrawsSharp) {
    FakeWindowSystem fake;
    SoftwareRenderBackend renderer;
    bool checked = false;
    const RunResult run = runScripted(fake, renderer, [&](int wait, WindowId program) {
        if (wait == 1) {
            EXPECT_TRUE(fake.changeDisplayScale(program, 2.0F).has_value());
        } else if (wait == 2) {
            const Image frame = framebufferOf(fake, program);
            EXPECT_EQ(frame.width(), 1600U);
            EXPECT_EQ(frame.height(), 1200U);
            expectScene(frame, kSceneAt800x600, 2);
            probe(frame, 16, 16, kHeader, "header first pixel");
            probe(frame, 1583, 111, kHeader, "header last pixel");
            checked = true;
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(checked);
}

TEST(Boxes, B4_OneFramePerChangingBatch) {
    FakeWindowSystem fake;
    SoftwareRenderBackend renderer;
    const RunResult run = runScripted(fake, renderer, [&](int wait, WindowId program) {
        switch (wait) {
        case 1: // Nothing happens: the wait times out with an empty batch.
            break;
        case 2: { // Another window's events.
            const auto other =
                fake.createWindow({.title = "Other", .size = {.width = 64, .height = 64}});
            ASSERT_TRUE(other.has_value());
            EXPECT_TRUE(fake.userResize(*other, {.width = 100, .height = 100}).has_value());
            EXPECT_TRUE(fake.changeDisplayScale(*other, 2.0F).has_value());
            break;
        }
        case 3:
            EXPECT_TRUE(fake.userResize(program, {.width = 640, .height = 480}).has_value());
            break;
        case 4: // A resize and a scale change in one batch: one frame.
            EXPECT_TRUE(fake.userResize(program, {.width = 1024, .height = 768}).has_value());
            EXPECT_TRUE(fake.changeDisplayScale(program, 1.5F).has_value());
            break;
        case 5: // A resize and a close request in one batch: the frame, then the exit.
            EXPECT_TRUE(fake.userResize(program, {.width = 700, .height = 500}).has_value());
            EXPECT_TRUE(fake.requestClose(program).has_value());
            break;
        default:
            break;
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_EQ(run.log, "boxes: frame 800x600 at scale 1\n"
                       "boxes: frame 640x480 at scale 1\n"
                       "boxes: frame 1536x1152 at scale 1.5\n"
                       "boxes: frame 1050x750 at scale 1.5\n");
}

TEST(Boxes, B5_CloseReleasesWindowAndTarget) {
    FakeWindowSystem fake;
    SoftwareRenderBackend renderer;
    const RunResult run = runScripted(fake, renderer, [&](int wait, WindowId program) {
        if (wait == 1) {
            EXPECT_TRUE(fake.requestClose(program).has_value());
        }
    });
    EXPECT_EQ(run.exitCode, 0);
    EXPECT_TRUE(fake.windows().empty());
    // A fresh SoftwareRenderBackend gives its first target id 1; the example creates one target.
    const auto front = renderer.frontBuffer(coil::render::RenderTargetId{.value = 1});
    ASSERT_FALSE(front.has_value());
    EXPECT_EQ(front.error().code(), ErrorCode::NotFound);
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

TEST(Boxes, B6_WindowFailureReturnsOne) {
    FailingWindowSystem failing;
    SoftwareRenderBackend renderer;
    std::ostringstream log;
    EXPECT_EQ(runBoxes(failing, renderer, log), 1);
    EXPECT_EQ(log.str(), "");
    EXPECT_EQ(failing.waits(), 0);
}

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

/// Pushes a real SDL close-request event for a window of `sdl`.
void closeOnSdl(const WindowSystem& sdl, WindowId id) {
    const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
    ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = *sdlId;
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
}

/// The pixel of an SDL window's surface at (x, y), or nullopt (and a test failure).
std::optional<Pixel> surfacePixel(const WindowSystem& sdl, WindowId id, int x, int y) {
    const auto sdlId = coil::platform_sdl3::sdlWindowId(sdl, id);
    EXPECT_TRUE(sdlId.has_value());
    if (!sdlId.has_value()) {
        return std::nullopt;
    }
    SDL_Surface* surface = SDL_GetWindowSurface(SDL_GetWindowFromID(*sdlId));
    EXPECT_NE(surface, nullptr) << SDL_GetError();
    Pixel pixel;
    if (surface == nullptr ||
        !SDL_ReadSurfacePixel(surface, x, y, &pixel.r, &pixel.g, &pixel.b, &pixel.a)) {
        ADD_FAILURE() << "cannot read the window surface: " << SDL_GetError();
        return std::nullopt;
    }
    return pixel;
}

TEST(Boxes, B7_RunsOnSdlDummyDriver) {
    auto sdl = coil::platform_sdl3::createSdlWindowSystem({.videoDriver = "dummy"});
    ASSERT_TRUE(sdl.has_value()) << std::format("{}", sdl.error());
    WindowSystem& system = **sdl;
    bool checked = false;
    ScriptedSdl scripted{system, [&](int wait) {
                             const std::vector<WindowId> ids = system.windows();
                             if (ids.empty()) {
                                 return;
                             }
                             if (wait == 1) {
                                 EXPECT_EQ(surfacePixel(system, ids.front(), 400, 32),
                                           std::optional<Pixel>{kHeader});
                                 EXPECT_EQ(surfacePixel(system, ids.front(), 4, 4),
                                           std::optional<Pixel>{kBackground});
                                 checked = true;
                             }
                             if (wait == 1 || wait >= kMaxWaits) {
                                 closeOnSdl(system, ids.front());
                             }
                         }};
    SoftwareRenderBackend renderer;
    std::ostringstream log;
    EXPECT_EQ(runBoxes(scripted, renderer, log), 0);
    EXPECT_TRUE(checked);
    EXPECT_TRUE(system.windows().empty());
}

} // namespace
