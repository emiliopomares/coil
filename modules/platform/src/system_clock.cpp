#include <coil/platform/clock.hpp>
#include <coil/platform/system_clock.hpp>

#include <chrono>

namespace coil::platform {

Clock::TimePoint SystemClock::now() const {
    return std::chrono::steady_clock::now();
}

} // namespace coil::platform
