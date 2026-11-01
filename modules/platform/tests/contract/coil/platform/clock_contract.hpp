#pragma once

// Contract suite every Clock implementation must pass (SPEC-0008).
// To run it for an implementation `Impl`, in a test source inside namespace coil::platform:
//   template <> struct ClockContractTraits<Impl> {
//       static std::unique_ptr<Clock> make();
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, ClockContract, ::testing::Types<Impl>);

#include <coil/platform/clock.hpp>

#include <gtest/gtest.h>

#include <memory>

namespace coil::platform {

/// Specialize for each implementation under contract; `make()` returns a fresh clock.
template <typename Impl>
struct ClockContractTraits;

template <typename Impl>
class ClockContract : public ::testing::Test {
protected:
    [[nodiscard]] const Clock& clock() const { return *m_clock; }

private:
    std::unique_ptr<Clock> m_clock = ClockContractTraits<Impl>::make();
};

TYPED_TEST_SUITE_P(ClockContract);

TYPED_TEST_P(ClockContract, CL1_NowNeverDecreases) {
    const Clock& clock = this->clock();
    Clock::TimePoint previous = clock.now();
    constexpr int kReadings = 1000;
    for (int reading = 0; reading < kReadings; ++reading) {
        const Clock::TimePoint current = clock.now();
        ASSERT_GE(current, previous) << "reading " << reading << " went backwards";
        previous = current;
    }
}

REGISTER_TYPED_TEST_SUITE_P(ClockContract, CL1_NowNeverDecreases);

} // namespace coil::platform
