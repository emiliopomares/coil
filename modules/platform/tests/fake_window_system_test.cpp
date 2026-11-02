#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
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

    static void userResize(WindowSystem& system, WindowId id, WindowSize size) {
        auto& fake = dynamic_cast<testing::FakeWindowSystem&>(system);
        ASSERT_TRUE(succeeds(fake.userResize(id, size)));
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

TEST(FakeWindowSystem, RS7_UserResizeValidatesLikeSetSize) {
    testing::FakeWindowSystem fake;
    const WindowSize initial{.width = 100, .height = 100};
    const auto id = fake.createWindow({.title = "A", .size = initial});
    ASSERT_TRUE(succeeds(id));

    EXPECT_TRUE(failsWith(fake.userResize(WindowId{.value = 999}, {.width = 200, .height = 150}),
                          ErrorCode::NotFound));
    EXPECT_TRUE(
        failsWith(fake.userResize(*id, {.width = 0, .height = 150}), ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        failsWith(fake.userResize(*id, {.width = 200, .height = -1}), ErrorCode::InvalidArgument));
    EXPECT_TRUE(succeeds(fake.userResize(*id, initial)));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});
    EXPECT_EQ(fake.size(*id), initial);
}

using std::chrono::milliseconds;
using std::chrono::seconds;

WindowDesc windowDesc() {
    return {.title = "A", .size = {.width = 100, .height = 100}};
}

TEST(FakeWindowSystem, FS1_DisplayScaleChangesPixelSize) {
    testing::FakeWindowSystem fake;
    const WindowSize size{.width = 100, .height = 80};
    const auto id = fake.createWindow({.title = "A", .size = size});
    ASSERT_TRUE(succeeds(id));
    EXPECT_EQ(fake.displayScale(*id), 1.0F);
    EXPECT_EQ(fake.pixelSize(*id), size);

    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*id, 2.0F)));
    EXPECT_EQ(fake.pollEvents(),
              (std::vector<WindowEvent>{ScaleChanged{.window = *id, .scale = 2.0F}}));
    EXPECT_EQ(fake.displayScale(*id), 2.0F);
    EXPECT_EQ(fake.pixelSize(*id), (WindowSize{.width = 200, .height = 160}));

    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*id, 2.0F)));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});

    const auto odd = fake.createWindow({.title = "B", .size = {.width = 101, .height = 80}});
    ASSERT_TRUE(succeeds(odd));
    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*odd, 1.5F)));
    EXPECT_EQ(fake.pixelSize(*odd), (WindowSize{.width = 152, .height = 120}));
    EXPECT_EQ(fake.pollEvents(),
              (std::vector<WindowEvent>{ScaleChanged{.window = *odd, .scale = 1.5F}}));

    for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()}) {
        EXPECT_TRUE(failsWith(fake.changeDisplayScale(*id, invalid), ErrorCode::InvalidArgument))
            << invalid;
    }
    EXPECT_TRUE(
        failsWith(fake.changeDisplayScale(WindowId{.value = 999}, 2.0F), ErrorCode::NotFound));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});
    EXPECT_EQ(fake.displayScale(*id), 2.0F);
}

TEST(FakeWindowSystem, FS2_ScaleChangesAreCoalescedAndDropped) {
    testing::FakeWindowSystem fake;
    const auto a = fake.createWindow({.title = "A", .size = {.width = 100, .height = 80}});
    const auto b = fake.createWindow({.title = "B", .size = {.width = 100, .height = 80}});
    ASSERT_TRUE(succeeds(a));
    ASSERT_TRUE(succeeds(b));

    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*a, 2.0F)));
    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*a, 3.0F)));
    EXPECT_EQ(fake.pollEvents(),
              (std::vector<WindowEvent>{ScaleChanged{.window = *a, .scale = 3.0F}}));

    ASSERT_TRUE(succeeds(fake.changeDisplayScale(*b, 2.0F)));
    ASSERT_TRUE(succeeds(fake.destroyWindow(*b)));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});
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
