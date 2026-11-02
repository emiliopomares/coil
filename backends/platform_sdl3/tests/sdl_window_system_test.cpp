#include <coil/foundation/error.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <utility>
#include <vector>

namespace {

using coil::ErrorCode;
using coil::platform::WindowDesc;
using coil::platform::WindowId;
using coil::platform_sdl3::createSdlWindowSystem;
using coil::platform_sdl3::sdlWindowId;
using coil::platform_sdl3::SdlWindowSystemOptions;

/// Headless options: SDL's dummy video driver needs no display and no graphics library.
SdlWindowSystemOptions headless() {
    return {.videoDriver = "dummy"};
}

TEST(SdlWindowSystem, AC1_CreatesWithHeadlessDriver) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    ASSERT_NE(*system, nullptr);
    EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0U);
    EXPECT_TRUE((*system)->windows().empty());
}

TEST(SdlWindowSystem, AC3_UnknownDriverIsUnavailable) {
    const auto system = createSdlWindowSystem({.videoDriver = "no-such-driver"});
    ASSERT_FALSE(system.has_value());
    EXPECT_EQ(system.error().code(), ErrorCode::Unavailable);
    EXPECT_FALSE(system.error().message().empty());
    EXPECT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0U);
}

TEST(SdlWindowSystem, AC4_SecondInstanceIsUnavailableUntilFirstIsDestroyed) {
    auto first = createSdlWindowSystem(headless());
    ASSERT_TRUE(first.has_value()) << std::format("{}", first.error());

    const auto second = createSdlWindowSystem(headless());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code(), ErrorCode::Unavailable);

    first->reset();
    const auto third = createSdlWindowSystem(headless());
    EXPECT_TRUE(third.has_value());
}

TEST(SdlWindowSystem, AC5_DestructionClosesWindowsAndShutsDownVideo) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    ASSERT_TRUE(
        (*system)->createWindow(WindowDesc{.title = "A", .size = {.width = 64, .height = 64}}));
    ASSERT_TRUE(
        (*system)->createWindow(WindowDesc{.title = "B", .size = {.width = 64, .height = 64}}));
    EXPECT_EQ((*system)->windows().size(), 2U);
    EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0U);

    system->reset();
    EXPECT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0U);
}

TEST(SdlWindowSystem, AC7_SdlWindowIdMatchesSdl) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto id =
        (*system)->createWindow(WindowDesc{.title = "A", .size = {.width = 64, .height = 64}});
    ASSERT_TRUE(id.has_value());

    const auto sdlId = sdlWindowId(**system, *id);
    ASSERT_TRUE(sdlId.has_value()) << std::format("{}", sdlId.error());
    EXPECT_NE(*sdlId, 0U);
    SDL_Window* window = SDL_GetWindowFromID(*sdlId);
    ASSERT_NE(window, nullptr);
    EXPECT_STREQ(SDL_GetWindowTitle(window), "A");

    const auto unknown = sdlWindowId(**system, WindowId{.value = 999});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code(), ErrorCode::NotFound);

    const coil::platform::testing::FakeWindowSystem fake;
    const auto notSdl = sdlWindowId(fake, WindowId{.value = 1});
    ASSERT_FALSE(notSdl.has_value());
    EXPECT_EQ(notSdl.error().code(), ErrorCode::InvalidArgument);
}

} // namespace

namespace coil::platform {
namespace {

/// Tag naming the SDL3 adapter in the contract suite (the implementation class is private).
struct Sdl3 {};

} // namespace

template <>
struct WindowSystemContractTraits<Sdl3> {
    static std::unique_ptr<WindowSystem> make() {
        auto system = platform_sdl3::createSdlWindowSystem(headless());
        if (!system.has_value()) {
            ADD_FAILURE() << std::format("cannot create SDL window system: {}", system.error());
            std::abort();
        }
        return std::move(*system);
    }

    /// Simulates the OS: pushes a real SDL close-request event for the window.
    static void requestClose(WindowSystem& system, WindowId id) {
        const auto sdlId = platform_sdl3::sdlWindowId(system, id);
        ASSERT_TRUE(succeeds(sdlId));
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        event.window.windowID = *sdlId;
        ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
    }

    /// Simulates a resize from outside the port: resizes the SDL window directly. SDL then reports
    /// it as SDL_EVENT_WINDOW_RESIZED, as it does when the user drags an edge.
    static void userResize(WindowSystem& system, WindowId id, WindowSize size) {
        const auto sdlId = platform_sdl3::sdlWindowId(system, id);
        ASSERT_TRUE(succeeds(sdlId));
        SDL_Window* window = SDL_GetWindowFromID(*sdlId);
        ASSERT_NE(window, nullptr) << SDL_GetError();
        ASSERT_TRUE(SDL_SetWindowSize(window, size.width, size.height)) << SDL_GetError();
    }
};

using Sdl3Implementations = ::testing::Types<Sdl3>;
INSTANTIATE_TYPED_TEST_SUITE_P(Sdl3, WindowSystemContract, Sdl3Implementations);

namespace {

/// Pushes a display-scale-changed event for an SDL window, as SDL would on a display change.
void pushScaleChanged(SDL_WindowID sdlId) {
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED;
    event.window.windowID = sdlId;
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
}

TEST(SdlWindowSystem, SD1_DummyDriverHasScaleOne) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const WindowSize size{.width = 100, .height = 80};
    const auto id = (*system)->createWindow({.title = "A", .size = size});
    ASSERT_TRUE(succeeds(id));
    EXPECT_EQ((*system)->displayScale(*id), 1.0F);
    EXPECT_EQ((*system)->pixelSize(*id), size);
}

TEST(SdlWindowSystem, SD2_ScaleEventsAreTranslatedOnce) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto id = (*system)->createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    ASSERT_TRUE(succeeds(id));
    const auto sdlId = sdlWindowId(**system, *id);
    ASSERT_TRUE(succeeds(sdlId));

    SDL_Window* foreign = SDL_CreateWindow("Foreign", 64, 64, 0);
    ASSERT_NE(foreign, nullptr) << SDL_GetError();
    pushScaleChanged(*sdlId);
    pushScaleChanged(*sdlId);
    pushScaleChanged(SDL_GetWindowID(foreign));

    EXPECT_EQ((*system)->pollEvents(),
              (std::vector<WindowEvent>{ScaleChanged{.window = *id, .scale = 1.0F}}));
    SDL_DestroyWindow(foreign);
}

} // namespace

} // namespace coil::platform
