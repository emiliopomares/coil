#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/clock_contract.hpp>
#include <coil/platform/system_clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <memory>

namespace coil::platform {

template <>
struct ClockContractTraits<testing::FakeClock> {
    static std::unique_ptr<Clock> make() { return std::make_unique<testing::FakeClock>(); }
};

template <>
struct ClockContractTraits<SystemClock> {
    static std::unique_ptr<Clock> make() { return std::make_unique<SystemClock>(); }
};

using FakeClocks = ::testing::Types<testing::FakeClock>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fake, ClockContract, FakeClocks);

using SystemClocks = ::testing::Types<SystemClock>;
INSTANTIATE_TYPED_TEST_SUITE_P(System, ClockContract, SystemClocks);

namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

TEST(FakeClock, AC2_StartsAtStartAndStandsStill) {
    const testing::FakeClock defaultClock;
    EXPECT_EQ(defaultClock.now(), Clock::TimePoint{});
    EXPECT_EQ(defaultClock.now(), defaultClock.now());

    const Clock::TimePoint start = Clock::TimePoint{} + seconds{42};
    const testing::FakeClock startedClock{start};
    EXPECT_EQ(startedClock.now(), start);
    EXPECT_EQ(startedClock.now(), startedClock.now());
}

TEST(FakeClock, AC3_AdvanceMovesByExactlyTheStep) {
    testing::FakeClock clock;
    const Clock::TimePoint start = clock.now();

    ASSERT_TRUE(clock.advance(milliseconds{5}).has_value());
    EXPECT_EQ(clock.now() - start, milliseconds{5});
    ASSERT_TRUE(clock.advance(milliseconds{0}).has_value());
    EXPECT_EQ(clock.now() - start, milliseconds{5});
    ASSERT_TRUE(clock.advance(milliseconds{10}).has_value());
    EXPECT_EQ(clock.now() - start, milliseconds{15});
}

TEST(FakeClock, AC4_NegativeAdvanceIsRejected) {
    testing::FakeClock clock;
    const Clock::TimePoint before = clock.now();

    const Expected<void> result = clock.advance(milliseconds{-1});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(clock.now(), before);
}

TEST(SystemClock, AC5_TimeMoves) {
    const SystemClock clock;
    const Clock::TimePoint first = clock.now();
    bool moved = false;
    constexpr int kMaxReadings = 10'000'000;
    for (int reading = 0; reading < kMaxReadings && !moved; ++reading) {
        moved = clock.now() > first;
    }
    EXPECT_TRUE(moved) << std::format("no change after {} readings", kMaxReadings);
}

TEST(SystemClock, AC6_TracksSteadyClock) {
    const SystemClock clock;
    const auto before = std::chrono::steady_clock::now();
    const Clock::TimePoint reading = clock.now();
    const auto after = std::chrono::steady_clock::now();
    EXPECT_LE(before, reading);
    EXPECT_LE(reading, after);
}

} // namespace

} // namespace coil::platform
