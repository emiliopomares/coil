#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>

namespace coil::platform::testing {

// SPEC-0008 Red stubs: time is fixed at zero and never moves.

FakeClock::FakeClock(TimePoint /*start*/) {}

Clock::TimePoint FakeClock::now() const {
    return m_now;
}

Expected<void> FakeClock::advance(Duration /*step*/) {
    return {};
}

} // namespace coil::platform::testing
