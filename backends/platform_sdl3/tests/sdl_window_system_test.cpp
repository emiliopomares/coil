#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_init.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <utility>

namespace {

using coil::ErrorCode;
using coil::platform::WindowDesc;
using coil::platform_sdl3::createSdlWindowSystem;
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
};

using Sdl3Implementations = ::testing::Types<Sdl3>;
INSTANTIATE_TYPED_TEST_SUITE_P(Sdl3, WindowSystemContract, Sdl3Implementations);

} // namespace coil::platform
