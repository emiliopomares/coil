#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <vector>

namespace coil::platform {

template <>
struct WindowSystemContractTraits<testing::FakeWindowSystem> {
    static std::unique_ptr<WindowSystem> make() {
        return std::make_unique<testing::FakeWindowSystem>();
    }

    static void requestClose(WindowSystem& system, WindowId id) {
        auto& fake = dynamic_cast<testing::FakeWindowSystem&>(system);
        ASSERT_TRUE(succeeds(fake.requestClose(id)));
    }
};

using FakeImplementations = ::testing::Types<testing::FakeWindowSystem>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fake, WindowSystemContract, FakeImplementations);

namespace {

TEST(FakeWindowSystem, AC6_RequestCloseOnUnknownIdIsNotFound) {
    testing::FakeWindowSystem fake;
    EXPECT_TRUE(failsWith(fake.requestClose(WindowId{.value = 999}), ErrorCode::NotFound));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});
}

using std::chrono::milliseconds;
using std::chrono::seconds;

WindowDesc windowDesc() {
    return {.title = "A", .size = {.width = 100, .height = 100}};
}

TEST(FakeWindowSystem, AC5_WaitAdvancesClockByTimeout) {
    testing::FakeClock clock;
    testing::FakeWindowSystem windows{clock};
    const Clock::TimePoint start = clock.now();

    EXPECT_EQ(windows.waitEvents(milliseconds{100}), std::vector<WindowEvent>{});
    EXPECT_EQ(clock.now() - start, milliseconds{100});
    EXPECT_EQ(windows.waitEvents(milliseconds{0}), std::vector<WindowEvent>{});
    EXPECT_EQ(windows.waitEvents(milliseconds{-5}), std::vector<WindowEvent>{});
    EXPECT_EQ(clock.now() - start, milliseconds{100});
}

TEST(FakeWindowSystem, AC6_PendingEventsReturnWithoutWaiting) {
    testing::FakeClock clock;
    testing::FakeWindowSystem windows{clock};
    int hookCalls = 0;
    windows.setWaitHook([&hookCalls] { ++hookCalls; });
    const Expected<WindowId> id = windows.createWindow(windowDesc());
    ASSERT_TRUE(succeeds(id));
    ASSERT_TRUE(succeeds(windows.requestClose(*id)));
    const Clock::TimePoint start = clock.now();

    EXPECT_EQ(windows.waitEvents(milliseconds{100}),
              std::vector<WindowEvent>{CloseRequested{.window = *id}});
    EXPECT_EQ(clock.now(), start);
    EXPECT_EQ(hookCalls, 0);
}

TEST(FakeWindowSystem, AC7_WaitHookCanDeliverAnEvent) {
    testing::FakeClock clock;
    testing::FakeWindowSystem windows{clock};
    const Expected<WindowId> id = windows.createWindow(windowDesc());
    ASSERT_TRUE(succeeds(id));
    int hookCalls = 0;
    windows.setWaitHook([&] {
        ++hookCalls;
        EXPECT_TRUE(succeeds(windows.requestClose(*id)));
    });
    const Clock::TimePoint start = clock.now();

    EXPECT_EQ(windows.waitEvents(milliseconds{100}),
              std::vector<WindowEvent>{CloseRequested{.window = *id}});
    EXPECT_EQ(clock.now(), start);
    EXPECT_EQ(hookCalls, 1);
}

TEST(FakeWindowSystem, AC8_UsesInjectedOrOwnClock) {
    testing::FakeClock external{Clock::TimePoint{} + seconds{1}};
    testing::FakeWindowSystem withExternal{external};
    EXPECT_EQ(&withExternal.clock(), &external);
    EXPECT_EQ(withExternal.waitEvents(milliseconds{20}), std::vector<WindowEvent>{});
    EXPECT_EQ(external.now(), Clock::TimePoint{} + seconds{1} + milliseconds{20});

    testing::FakeWindowSystem withOwn;
    EXPECT_EQ(withOwn.clock().now(), Clock::TimePoint{});
    EXPECT_EQ(withOwn.waitEvents(milliseconds{30}), std::vector<WindowEvent>{});
    EXPECT_EQ(withOwn.clock().now(), Clock::TimePoint{} + milliseconds{30});
}

} // namespace

} // namespace coil::platform
