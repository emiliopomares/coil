#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/foundation/image.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/input_source_contract.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <expected>
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

    /// Reads the SDL window surface back, pixel by pixel.
    static Expected<Image> framebuffer(const WindowSystem& system, WindowId id) {
        const auto sdlId = platform_sdl3::sdlWindowId(system, id);
        if (!sdlId.has_value()) {
            return std::unexpected(sdlId.error());
        }
        SDL_Surface* surface = SDL_GetWindowSurface(SDL_GetWindowFromID(*sdlId));
        if (surface == nullptr) {
            return makeUnexpected(ErrorCode::Internal,
                                  std::format("no window surface: {}", SDL_GetError()));
        }
        Image image{static_cast<std::uint32_t>(surface->w), static_cast<std::uint32_t>(surface->h)};
        auto it = image.pixels().begin();
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                Pixel& pixel = *it++;
                if (!SDL_ReadSurfacePixel(surface, x, y, &pixel.r, &pixel.g, &pixel.b, &pixel.a)) {
                    return makeUnexpected(ErrorCode::Internal,
                                          std::format("cannot read pixel: {}", SDL_GetError()));
                }
            }
        }
        return image;
    }
};

using Sdl3Implementations = ::testing::Types<Sdl3>;
INSTANTIATE_TYPED_TEST_SUITE_P(Sdl3, WindowSystemContract, Sdl3Implementations);

namespace {

/// SDL's window id for `id`, or 0 (and a test failure).
SDL_WindowID sdlIdOf(const WindowSystem& system, WindowId id) {
    const auto sdlId = platform_sdl3::sdlWindowId(system, id);
    EXPECT_TRUE(succeeds(sdlId));
    return sdlId.value_or(0);
}

/// SDL's button number for a role.
Uint8 sdlButton(PointerButton button) {
    switch (button) {
    case PointerButton::Primary:
        return SDL_BUTTON_LEFT;
    case PointerButton::Secondary:
        return SDL_BUTTON_RIGHT;
    case PointerButton::Middle:
        return SDL_BUTTON_MIDDLE;
    case PointerButton::Back:
        return SDL_BUTTON_X1;
    case PointerButton::Forward:
        return SDL_BUTTON_X2;
    }
    return 0;
}

constexpr SDL_MouseID kSdlMouse = 1;

void push(SDL_Event event) {
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
}

void pushButton(SDL_WindowID window, Uint8 button, bool down, Point position) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = window;
    event.button.which = kSdlMouse;
    event.button.button = button;
    event.button.down = down;
    event.button.x = position.x;
    event.button.y = position.y;
    push(event);
}

void pushWheel(SDL_WindowID window, Point position, float dx, float dy,
               SDL_MouseWheelDirection direction) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.windowID = window;
    event.wheel.which = kSdlMouse;
    event.wheel.x = dx;
    event.wheel.y = dy;
    event.wheel.direction = direction;
    event.wheel.mouse_x = position.x;
    event.wheel.mouse_y = position.y;
    push(event);
}

void pushMotion(SDL_WindowID window, Point position) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = window;
    event.motion.which = kSdlMouse;
    event.motion.x = position.x;
    event.motion.y = position.y;
    push(event);
}

/// Every Key and the SDL keycode that produces it (SPEC-0027 AC-5), written out from the spec.
constexpr std::array<std::pair<SDL_Keycode, Key>, 67> kKeyTable{{
    {SDLK_A, Key::A},
    {SDLK_B, Key::B},
    {SDLK_C, Key::C},
    {SDLK_D, Key::D},
    {SDLK_E, Key::E},
    {SDLK_F, Key::F},
    {SDLK_G, Key::G},
    {SDLK_H, Key::H},
    {SDLK_I, Key::I},
    {SDLK_J, Key::J},
    {SDLK_K, Key::K},
    {SDLK_L, Key::L},
    {SDLK_M, Key::M},
    {SDLK_N, Key::N},
    {SDLK_O, Key::O},
    {SDLK_P, Key::P},
    {SDLK_Q, Key::Q},
    {SDLK_R, Key::R},
    {SDLK_S, Key::S},
    {SDLK_T, Key::T},
    {SDLK_U, Key::U},
    {SDLK_V, Key::V},
    {SDLK_W, Key::W},
    {SDLK_X, Key::X},
    {SDLK_Y, Key::Y},
    {SDLK_Z, Key::Z},
    {SDLK_0, Key::Digit0},
    {SDLK_1, Key::Digit1},
    {SDLK_2, Key::Digit2},
    {SDLK_3, Key::Digit3},
    {SDLK_4, Key::Digit4},
    {SDLK_5, Key::Digit5},
    {SDLK_6, Key::Digit6},
    {SDLK_7, Key::Digit7},
    {SDLK_8, Key::Digit8},
    {SDLK_9, Key::Digit9},
    {SDLK_F1, Key::F1},
    {SDLK_F2, Key::F2},
    {SDLK_F3, Key::F3},
    {SDLK_F4, Key::F4},
    {SDLK_F5, Key::F5},
    {SDLK_F6, Key::F6},
    {SDLK_F7, Key::F7},
    {SDLK_F8, Key::F8},
    {SDLK_F9, Key::F9},
    {SDLK_F10, Key::F10},
    {SDLK_F11, Key::F11},
    {SDLK_F12, Key::F12},
    {SDLK_RETURN, Key::Enter},
    {SDLK_ESCAPE, Key::Escape},
    {SDLK_BACKSPACE, Key::Backspace},
    {SDLK_TAB, Key::Tab},
    {SDLK_SPACE, Key::Space},
    {SDLK_LEFT, Key::Left},
    {SDLK_RIGHT, Key::Right},
    {SDLK_UP, Key::Up},
    {SDLK_DOWN, Key::Down},
    {SDLK_HOME, Key::Home},
    {SDLK_END, Key::End},
    {SDLK_PAGEUP, Key::PageUp},
    {SDLK_PAGEDOWN, Key::PageDown},
    {SDLK_INSERT, Key::Insert},
    {SDLK_DELETE, Key::Delete},
    {SDLK_LSHIFT, Key::Shift},
    {SDLK_LCTRL, Key::Control},
    {SDLK_LALT, Key::Alt},
    {SDLK_LGUI, Key::Super},
}};

SDL_Keycode sdlKeyOf(Key key) {
    for (const auto& [sdl, coil] : kKeyTable) {
        if (coil == key) {
            return sdl;
        }
    }
    ADD_FAILURE() << "no SDL keycode for key " << static_cast<int>(key);
    return SDLK_UNKNOWN;
}

void pushKey(SDL_WindowID window, SDL_Keycode key, SDL_Keymod mod, bool down, bool repeat) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = window;
    event.key.key = key;
    event.key.mod = mod;
    event.key.down = down;
    event.key.repeat = repeat;
    push(event);
}

void pushText(SDL_WindowID window, const char* utf8) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = window;
    event.text.text = utf8;
    push(event);
}

} // namespace

/// Pushes real SDL mouse events, as SDL would for a device.
template <>
struct InputSourceContractTraits<Sdl3> {
    static std::unique_ptr<WindowSystem> make() { return WindowSystemContractTraits<Sdl3>::make(); }

    static InputSource& input(WindowSystem& system) {
        auto source = platform_sdl3::inputSourceOf(system);
        if (!source.has_value()) {
            ADD_FAILURE() << std::format("no input source: {}", source.error());
            std::abort();
        }
        return source->get();
    }

    static void move(WindowSystem& system, WindowId id, Point position) {
        pushMotion(sdlIdOf(system, id), position);
    }

    static void press(WindowSystem& system, WindowId id, PointerButton button, Point position) {
        pushButton(sdlIdOf(system, id), sdlButton(button), true, position);
    }

    static void release(WindowSystem& system, WindowId id, PointerButton button, Point position) {
        pushButton(sdlIdOf(system, id), sdlButton(button), false, position);
    }

    static void scroll(WindowSystem& system, WindowId id, Point position, float dx, float dy) {
        pushWheel(sdlIdOf(system, id), position, dx, dy, SDL_MOUSEWHEEL_NORMAL);
    }

    static void requestClose(WindowSystem& system, WindowId id) {
        WindowSystemContractTraits<Sdl3>::requestClose(system, id);
    }

    static void key(WindowSystem& system, WindowId id, Key key, KeyModifiers modifiers, bool down,
                    bool repeat) {
        SDL_Keymod mod = SDL_KMOD_NONE;
        mod |= modifiers.shift ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE;
        mod |= modifiers.control ? SDL_KMOD_LCTRL : SDL_KMOD_NONE;
        mod |= modifiers.alt ? SDL_KMOD_LALT : SDL_KMOD_NONE;
        mod |= modifiers.super ? SDL_KMOD_LGUI : SDL_KMOD_NONE;
        pushKey(sdlIdOf(system, id), sdlKeyOf(key), mod, down, repeat);
    }

    static void text(WindowSystem& system, WindowId id, const char* utf8) {
        pushText(sdlIdOf(system, id), utf8);
    }
};

INSTANTIATE_TYPED_TEST_SUITE_P(Sdl3, InputSourceContract, Sdl3Implementations);

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

TEST(SdlWindowSystem, SD3_WindowsUseHighPixelDensity) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto fixed =
        (*system)->createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    const auto resizable = (*system)->createWindow(
        {.title = "B", .size = {.width = 100, .height = 80}, .resizable = true});
    for (const auto& id : {fixed, resizable}) {
        ASSERT_TRUE(succeeds(id));
        const auto sdlId = sdlWindowId(**system, *id);
        ASSERT_TRUE(succeeds(sdlId));
        SDL_Window* window = SDL_GetWindowFromID(*sdlId);
        ASSERT_NE(window, nullptr) << SDL_GetError();
        EXPECT_NE(SDL_GetWindowFlags(window) & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0U);
    }
}

TEST(SdlWindowSystem, SD2_UnchangedScaleEventsAreDropped) {
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

    // The dummy driver's scale stays 1.0: an "unchanged" scale is not a change (rule 4).
    EXPECT_EQ((*system)->pollEvents(), std::vector<WindowEvent>{});
    SDL_DestroyWindow(foreign);
}

TEST(SdlInputSource, SI1_SdlSpecifics) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto id = (*system)->createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    ASSERT_TRUE(succeeds(id));
    const auto source = platform_sdl3::inputSourceOf(**system);
    ASSERT_TRUE(succeeds(source));
    const SDL_WindowID window = sdlIdOf(**system, *id);
    const Point at{.x = 3.0F, .y = 4.0F};

    pushWheel(window, at, 0.5F, 1.0F, SDL_MOUSEWHEEL_FLIPPED);
    pushButton(window, SDL_BUTTON_X2 + 1, true, at);
    SDL_Window* foreign = SDL_CreateWindow("Foreign", 64, 64, 0);
    ASSERT_NE(foreign, nullptr) << SDL_GetError();
    pushMotion(SDL_GetWindowID(foreign), at);
    EXPECT_EQ(withoutTimeOrDevice(source->get().pollInput()),
              (std::vector<RawInputEvent>{PointerScrolled{
                  .window = *id, .position = at, .deltaX = -0.5F, .deltaY = -1.0F}}));
    SDL_DestroyWindow(foreign);

    testing::FakeWindowSystem fake;
    EXPECT_TRUE(failsWith(platform_sdl3::inputSourceOf(fake), ErrorCode::InvalidArgument));
}

// SPEC-0027: SDL keyboard specifics (SK<n>).

TEST(SdlInputSource, SK1_KeyTable) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto id = (*system)->createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    ASSERT_TRUE(succeeds(id));
    const auto source = platform_sdl3::inputSourceOf(**system);
    ASSERT_TRUE(succeeds(source));
    const SDL_WindowID window = sdlIdOf(**system, *id);

    std::vector<RawInputEvent> expected;
    for (const auto& [sdl, key] : kKeyTable) {
        pushKey(window, sdl, SDL_KMOD_NONE, true, false);
        expected.emplace_back(KeyPressed{.window = *id, .key = key});
    }
    // Right-hand modifiers count as the same keys (Q3).
    for (const auto& [sdl, key] :
         {std::pair{SDLK_RSHIFT, Key::Shift}, std::pair{SDLK_RCTRL, Key::Control},
          std::pair{SDLK_RALT, Key::Alt}, std::pair{SDLK_RGUI, Key::Super}}) {
        pushKey(window, sdl, SDL_KMOD_NONE, true, false);
        expected.emplace_back(KeyPressed{.window = *id, .key = key});
    }
    pushKey(window, SDLK_VOLUMEUP, SDL_KMOD_NONE, true, false); // not in the enum: dropped
    pushKey(0, SDLK_A, SDL_KMOD_NONE, true, false);             // no focused window: dropped
    pushText(window, "");                                       // empty text: dropped
    EXPECT_EQ(withoutTimeOrDevice(source->get().pollInput()), expected);
}

TEST(SdlInputSource, SK2_TextInputIsStarted) {
    auto system = createSdlWindowSystem(headless());
    ASSERT_TRUE(system.has_value()) << std::format("{}", system.error());
    const auto id = (*system)->createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    ASSERT_TRUE(succeeds(id));
    SDL_Window* window = SDL_GetWindowFromID(sdlIdOf(**system, *id));
    ASSERT_NE(window, nullptr) << SDL_GetError();
    EXPECT_TRUE(SDL_TextInputActive(window));
}

} // namespace

} // namespace coil::platform
