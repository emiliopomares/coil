#pragma once

#include <chrono>

namespace coil::platform {

/// Port: source of monotonic time. Inject it wherever code needs "now", so tests can control time.
class Clock {
public:
    using Duration = std::chrono::steady_clock::duration;
    using TimePoint = std::chrono::steady_clock::time_point;

    virtual ~Clock();

    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;
    Clock(Clock&&) = delete;
    Clock& operator=(Clock&&) = delete;

    /// Current time. Never decreases between calls on the same clock.
    [[nodiscard]] virtual TimePoint now() const = 0;

protected:
    Clock() = default;
};

} // namespace coil::platform
