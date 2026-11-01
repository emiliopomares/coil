#include <coil/platform/clock.hpp>
#include <coil/platform/system_clock.hpp>

namespace coil::platform {

Clock::TimePoint SystemClock::now() const {
    return TimePoint{}; // SPEC-0008 Red stub
}

} // namespace coil::platform
