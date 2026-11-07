#include <coil/core/input_mapper.hpp>
#include <coil/core/intent.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>

namespace coil::core {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, const Intent& intent) {
    return os << "Intent{index " << intent.index() << ", window "
              << std::visit([](const auto& alternative) { return alternative.window.value; },
                            intent)
              << "}";
}

} // namespace
} // namespace coil::core

namespace {

using coil::Point;
using coil::core::Activate;
using coil::core::Bindings;
using coil::core::Cancel;
using coil::core::InputMapper;
using coil::core::Intent;
using coil::core::IntentAction;
using coil::core::KeyChord;
using coil::core::Navigate;
using coil::core::NavigationDirection;
using coil::platform::Key;
using coil::platform::KeyModifiers;
using coil::platform::KeyPressed;
using coil::platform::KeyReleased;
using coil::platform::PointerButton;
using coil::platform::PointerId;
using coil::platform::WindowId;

constexpr WindowId kWindow{.value = 7};
constexpr KeyModifiers kShift{.shift = true};

KeyPressed press(Key key, KeyModifiers modifiers = {}, bool repeat = false) {
    return KeyPressed{.window = kWindow, .key = key, .modifiers = modifiers, .repeat = repeat};
}

/// SPEC-0028 rule 2, written out: each default chord and the intent it gives.
const std::array<std::pair<KeyChord, Intent>, 8> kDefaults{{
    {KeyChord{.key = Key::Enter}, Activate{.window = kWindow}},
    {KeyChord{.key = Key::Space}, Activate{.window = kWindow}},
    {KeyChord{.key = Key::Escape}, Cancel{.window = kWindow}},
    {KeyChord{.key = Key::Tab},
     Navigate{.window = kWindow, .direction = NavigationDirection::Next}},
    {KeyChord{.key = Key::Tab, .modifiers = kShift},
     Navigate{.window = kWindow, .direction = NavigationDirection::Previous}},
    {KeyChord{.key = Key::Up}, Navigate{.window = kWindow, .direction = NavigationDirection::Up}},
    {KeyChord{.key = Key::Down},
     Navigate{.window = kWindow, .direction = NavigationDirection::Down}},
    {KeyChord{.key = Key::Left},
     Navigate{.window = kWindow, .direction = NavigationDirection::Left}},
}};

TEST(InputMapper, AC1_DefaultKeyBindings) {
    const InputMapper mapper;
    for (const auto& [chord, intent] : kDefaults) {
        EXPECT_EQ(mapper.map(press(chord.key, chord.modifiers)), std::vector<Intent>{intent})
            << "key " << static_cast<int>(chord.key);
    }
    EXPECT_EQ(mapper.map(press(Key::Right)),
              (std::vector<Intent>{
                  Navigate{.window = kWindow, .direction = NavigationDirection::Right}}));
}

TEST(InputMapper, AC2_ExactChordsAndReleases) {
    const InputMapper mapper;
    // Control: the plain chord is bound, so the absences below are rejections, not a dead mapper.
    ASSERT_EQ(mapper.map(press(Key::Enter)), std::vector<Intent>{Activate{.window = kWindow}});
    EXPECT_EQ(mapper.map(press(Key::Enter, KeyModifiers{.control = true})), std::vector<Intent>{});
    EXPECT_EQ(mapper.map(press(Key::Escape, kShift)), std::vector<Intent>{});
    EXPECT_EQ(mapper.map(press(Key::A)), std::vector<Intent>{});
    for (const Key key : {Key::Enter, Key::Tab}) {
        EXPECT_EQ(mapper.map(KeyReleased{.window = kWindow, .key = key}), std::vector<Intent>{});
    }
}

TEST(InputMapper, AC3_OnlyNavigationRepeats) {
    const InputMapper mapper;
    EXPECT_EQ(
        mapper.map(press(Key::Down, {}, true)),
        (std::vector<Intent>{Navigate{.window = kWindow, .direction = NavigationDirection::Down}}));
    EXPECT_EQ(
        mapper.map(press(Key::Tab, {}, true)),
        (std::vector<Intent>{Navigate{.window = kWindow, .direction = NavigationDirection::Next}}));
    for (const Key key : {Key::Enter, Key::Space, Key::Escape}) {
        EXPECT_EQ(mapper.map(press(key, {}, true)), std::vector<Intent>{})
            << "key " << static_cast<int>(key);
    }
}

TEST(InputMapper, AC4_PointerAndScroll) {
    const InputMapper mapper;
    constexpr PointerId kMouse{.value = 3};
    constexpr Point kAt{.x = 3.0F, .y = 4.0F};
    EXPECT_EQ(mapper.map(coil::platform::PointerMoved{
                  .window = kWindow, .pointer = kMouse, .position = kAt}),
              (std::vector<Intent>{
                  coil::core::PointerMove{.window = kWindow, .pointer = kMouse, .position = kAt}}));
    EXPECT_EQ(mapper.map(coil::platform::PointerPressed{.window = kWindow,
                                                        .pointer = kMouse,
                                                        .button = PointerButton::Secondary,
                                                        .position = kAt}),
              (std::vector<Intent>{coil::core::PointerDown{.window = kWindow,
                                                           .pointer = kMouse,
                                                           .button = PointerButton::Secondary,
                                                           .position = kAt}}));
    EXPECT_EQ(mapper.map(coil::platform::PointerReleased{.window = kWindow,
                                                         .pointer = kMouse,
                                                         .button = PointerButton::Secondary,
                                                         .position = kAt}),
              (std::vector<Intent>{coil::core::PointerUp{.window = kWindow,
                                                         .pointer = kMouse,
                                                         .button = PointerButton::Secondary,
                                                         .position = kAt}}));
    EXPECT_EQ(mapper.map(coil::platform::PointerScrolled{.window = kWindow,
                                                         .pointer = kMouse,
                                                         .position = kAt,
                                                         .deltaX = -1.5F,
                                                         .deltaY = 2.0F}),
              (std::vector<Intent>{coil::core::Scroll{
                  .window = kWindow, .position = kAt, .deltaX = -1.5F, .deltaY = 2.0F}}));
}

TEST(InputMapper, AC5_Text) {
    const InputMapper mapper;
    EXPECT_EQ(mapper.map(coil::platform::TextEntered{.window = kWindow, .text = "\xC3\xA9"}),
              (std::vector<Intent>{coil::core::TextInput{.window = kWindow, .text = "\xC3\xA9"}}));
}

TEST(InputMapper, AC6_Rebinding) {
    Bindings bindings = Bindings::defaults();
    bindings.bind(KeyChord{.key = Key::F2}, IntentAction::Activate);
    bindings.unbind(KeyChord{.key = Key::Space});
    bindings.bind(KeyChord{.key = Key::Enter}, IntentAction::Cancel);
    const Bindings before = bindings;
    bindings.unbind(KeyChord{.key = Key::F9});
    EXPECT_EQ(bindings, before) << "unbinding an unbound chord changed the bindings";

    InputMapper mapper{bindings};
    EXPECT_EQ(mapper.map(press(Key::F2)), std::vector<Intent>{Activate{.window = kWindow}});
    EXPECT_EQ(mapper.map(press(Key::Space)), std::vector<Intent>{});
    EXPECT_EQ(mapper.map(press(Key::Enter)), std::vector<Intent>{Cancel{.window = kWindow}});

    mapper.setBindings(Bindings{});
    EXPECT_EQ(mapper.bindings(), Bindings{});
    EXPECT_EQ(mapper.map(press(Key::Enter)), std::vector<Intent>{});
    EXPECT_EQ(mapper.map(press(Key::Tab)), std::vector<Intent>{});
    EXPECT_EQ(mapper.map(coil::platform::TextEntered{.window = kWindow, .text = "a"}),
              (std::vector<Intent>{coil::core::TextInput{.window = kWindow, .text = "a"}}));
    EXPECT_EQ(mapper.map(coil::platform::PointerMoved{.window = kWindow}).size(), 1U);
    EXPECT_EQ(mapper.map(coil::platform::PointerScrolled{.window = kWindow, .deltaY = 1.0F}).size(),
              1U);
}

TEST(Bindings, AC7_Defaults) {
    const Bindings defaults = Bindings::defaults();
    const std::array<std::pair<KeyChord, IntentAction>, 9> table{{
        {KeyChord{.key = Key::Enter}, IntentAction::Activate},
        {KeyChord{.key = Key::Space}, IntentAction::Activate},
        {KeyChord{.key = Key::Escape}, IntentAction::Cancel},
        {KeyChord{.key = Key::Tab}, IntentAction::NavigateNext},
        {KeyChord{.key = Key::Tab, .modifiers = kShift}, IntentAction::NavigatePrevious},
        {KeyChord{.key = Key::Up}, IntentAction::NavigateUp},
        {KeyChord{.key = Key::Down}, IntentAction::NavigateDown},
        {KeyChord{.key = Key::Left}, IntentAction::NavigateLeft},
        {KeyChord{.key = Key::Right}, IntentAction::NavigateRight},
    }};
    for (const auto& [chord, action] : table) {
        EXPECT_EQ(defaults.find(chord), std::optional<IntentAction>{action})
            << "key " << static_cast<int>(chord.key);
    }
    EXPECT_EQ(defaults.find(KeyChord{.key = Key::A}), std::nullopt);
    EXPECT_EQ(defaults.find(KeyChord{.key = Key::Enter, .modifiers = kShift}), std::nullopt);

    Bindings rebuilt;
    for (const auto& [chord, action] : table) {
        rebuilt.bind(chord, action);
    }
    EXPECT_EQ(defaults, rebuilt) << "the defaults hold more than the table";
    EXPECT_EQ(Bindings::defaults(), Bindings::defaults());
}

} // namespace
