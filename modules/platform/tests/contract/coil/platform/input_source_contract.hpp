#pragma once

// Contract suite every InputSource implementation must pass (SPEC-0026). An implementation is a
// WindowSystem that is also the InputSource for its windows (SPEC-0026 Q1). To run it for `Impl`,
// in a test source inside namespace coil::platform:
//   template <> struct InputSourceContractTraits<Impl> {
//       static std::unique_ptr<WindowSystem> make();
//       static InputSource& input(WindowSystem& system);
//       // Each simulates a device event for window `id`, outside the port.
//       static void move(WindowSystem& system, WindowId id, Point position);
//       static void press(WindowSystem& system, WindowId id, PointerButton button, Point position);
//       static void release(WindowSystem& system, WindowId id, PointerButton button, Point
//       position); static void scroll(WindowSystem& system, WindowId id, Point position, float dx,
//       float dy); static void requestClose(WindowSystem& system, WindowId id);
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, InputSourceContract, ::testing::Types<Impl>);

#include <coil/foundation/geometry.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>

#include <gtest/gtest.h>

#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace coil::platform {

/// Specialize for each implementation under contract (see the header comment).
template <typename Impl>
struct InputSourceContractTraits;

inline std::ostream& operator<<(std::ostream& os, Point point) {
    return os << "(" << point.x << ", " << point.y << ")";
}

inline const char* toString(PointerButton button) {
    switch (button) {
    case PointerButton::Primary:
        return "Primary";
    case PointerButton::Secondary:
        return "Secondary";
    case PointerButton::Middle:
        return "Middle";
    case PointerButton::Back:
        return "Back";
    case PointerButton::Forward:
        return "Forward";
    }
    return "?";
}

// Readable failure messages (test-only; GoogleTest uses operator<<). Timestamps are not printed.
inline std::ostream& operator<<(std::ostream& os, const RawInputEvent& event) {
    if (const auto* moved = std::get_if<PointerMoved>(&event)) {
        return os << "PointerMoved{" << moved->window << ", " << moved->position << "}";
    }
    if (const auto* pressed = std::get_if<PointerPressed>(&event)) {
        return os << "PointerPressed{" << pressed->window << ", " << toString(pressed->button)
                  << ", " << pressed->position << "}";
    }
    if (const auto* released = std::get_if<PointerReleased>(&event)) {
        return os << "PointerReleased{" << released->window << ", " << toString(released->button)
                  << ", " << released->position << "}";
    }
    if (const auto* scrolled = std::get_if<PointerScrolled>(&event)) {
        return os << "PointerScrolled{" << scrolled->window << ", " << scrolled->position << ", "
                  << scrolled->deltaX << ", " << scrolled->deltaY << "}";
    }
    return os << "RawInputEvent{unknown}";
}

/// `events` with every timestamp and pointer id cleared, for comparing what happened, not when or
/// with which device.
inline std::vector<RawInputEvent> withoutTimeOrDevice(std::vector<RawInputEvent> events) {
    for (RawInputEvent& event : events) {
        std::visit(
            [](auto& alternative) {
                alternative.time = {};
                alternative.pointer = {};
            },
            event);
    }
    return events;
}

/// The timestamp of any raw input event.
inline InputTimestamp timeOf(const RawInputEvent& event) {
    return std::visit([](const auto& alternative) { return alternative.time; }, event);
}

template <typename Impl>
class InputSourceContract : public ::testing::Test {
protected:
    using Traits = InputSourceContractTraits<Impl>;

    WindowSystem& system() { return *m_system; }

    InputSource& input() { return Traits::input(*m_system); }

    WindowId create(std::string title) {
        const Expected<WindowId> result = m_system->createWindow(
            WindowDesc{.title = std::move(title), .size = {.width = 100, .height = 80}});
        EXPECT_TRUE(succeeds(result));
        return result.value_or(WindowId{});
    }

    /// The next batch of input with timestamps and pointer ids cleared.
    std::vector<RawInputEvent> poll() { return withoutTimeOrDevice(input().pollInput()); }

private:
    std::unique_ptr<WindowSystem> m_system = Traits::make();
};

TYPED_TEST_SUITE_P(InputSourceContract);

TYPED_TEST_P(InputSourceContract, IN1_NothingHappened) {
    this->create("A");
    EXPECT_EQ(this->poll(), std::vector<RawInputEvent>{});
}

TYPED_TEST_P(InputSourceContract, IN2_MoveIsReportedOnce) {
    const WindowId a = this->create("A");
    const Point position{.x = 10.5F, .y = 20.25F};
    TestFixture::Traits::move(this->system(), a, position);
    EXPECT_EQ(this->poll(),
              (std::vector<RawInputEvent>{PointerMoved{.window = a, .position = position}}));
    EXPECT_EQ(this->poll(), std::vector<RawInputEvent>{});
}

TYPED_TEST_P(InputSourceContract, IN3_ButtonsPressAndRelease) {
    const WindowId a = this->create("A");
    const Point at{.x = 5.0F, .y = 6.0F};
    std::vector<RawInputEvent> expected;
    for (const PointerButton button :
         {PointerButton::Primary, PointerButton::Secondary, PointerButton::Middle}) {
        TestFixture::Traits::press(this->system(), a, button, at);
        TestFixture::Traits::release(this->system(), a, button, at);
        expected.emplace_back(PointerPressed{.window = a, .button = button, .position = at});
        expected.emplace_back(PointerReleased{.window = a, .button = button, .position = at});
    }
    EXPECT_EQ(this->poll(), expected);
}

TYPED_TEST_P(InputSourceContract, IN4_ScrollDeltas) {
    const WindowId a = this->create("A");
    const Point at{.x = 7.0F, .y = 8.0F};
    TestFixture::Traits::scroll(this->system(), a, at, 0.0F, 1.0F);
    TestFixture::Traits::scroll(this->system(), a, at, -2.5F, 0.0F);
    EXPECT_EQ(this->poll(),
              (std::vector<RawInputEvent>{
                  PointerScrolled{.window = a, .position = at, .deltaX = 0.0F, .deltaY = 1.0F},
                  PointerScrolled{.window = a, .position = at, .deltaX = -2.5F, .deltaY = 0.0F}}));
}

TYPED_TEST_P(InputSourceContract, IN5_OrderWindowsAndDrops) {
    using Traits = TestFixture::Traits;
    const WindowId a = this->create("A");
    const WindowId b = this->create("B");
    const Point at{.x = 1.0F, .y = 2.0F};
    Traits::move(this->system(), a, at);
    Traits::press(this->system(), b, PointerButton::Primary, at);
    Traits::scroll(this->system(), a, at, 0.0F, -1.0F);
    const std::vector<RawInputEvent> events = this->input().pollInput();
    EXPECT_EQ(withoutTimeOrDevice(events),
              (std::vector<RawInputEvent>{
                  PointerMoved{.window = a, .position = at},
                  PointerPressed{.window = b, .button = PointerButton::Primary, .position = at},
                  PointerScrolled{.window = a, .position = at, .deltaX = 0.0F, .deltaY = -1.0F}}));
    for (auto event = events.begin(); event != events.end() && std::next(event) != events.end();
         ++event) {
        EXPECT_LE(timeOf(*event), timeOf(*std::next(event))) << "timestamps go back";
    }

    const WindowId gone = this->create("Gone");
    Traits::move(this->system(), gone, at);
    ASSERT_TRUE(succeeds(this->system().destroyWindow(gone)));
    EXPECT_EQ(this->poll(), std::vector<RawInputEvent>{});
}

TYPED_TEST_P(InputSourceContract, IN6_PortsShareTheQueue) {
    using Traits = TestFixture::Traits;
    const WindowId a = this->create("A");
    const Point at{.x = 3.0F, .y = 4.0F};
    const std::vector<RawInputEvent> move{PointerMoved{.window = a, .position = at}};
    const std::vector<WindowEvent> close{CloseRequested{.window = a}};

    Traits::requestClose(this->system(), a);
    Traits::move(this->system(), a, at);
    EXPECT_EQ(this->poll(), move) << "input first";
    EXPECT_EQ(this->system().pollEvents(), close) << "window events after input";

    Traits::requestClose(this->system(), a);
    Traits::move(this->system(), a, at);
    EXPECT_EQ(this->system().pollEvents(), close) << "window events first";
    EXPECT_EQ(this->poll(), move) << "input after window events";
}

REGISTER_TYPED_TEST_SUITE_P(InputSourceContract, IN1_NothingHappened, IN2_MoveIsReportedOnce,
                            IN3_ButtonsPressAndRelease, IN4_ScrollDeltas, IN5_OrderWindowsAndDrops,
                            IN6_PortsShareTheQueue);

} // namespace coil::platform
