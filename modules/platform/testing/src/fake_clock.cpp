#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>
#include <coil/platform/testing/fake_clock.hpp>

#include <format>

namespace coil::platform::testing {

FakeClock::FakeClock(TimePoint start)
    : m_now{start} {}

Clock::TimePoint FakeClock::now() const {
    return m_now;
}

Expected<void> FakeClock::advance(Duration step) {
    if (step < Duration::zero()) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              std::format("time cannot go backwards (step {})", step));
    }
    m_now += step;
    return {};
}

} // namespace coil::platform::testing
