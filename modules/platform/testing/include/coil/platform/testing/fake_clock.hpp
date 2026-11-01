#pragma once

#include <coil/foundation/error.hpp>
#include <coil/platform/clock.hpp>

namespace coil::platform::testing {

/// Clock for tests: time stands still until the test calls advance().
class FakeClock final : public Clock {
public:
    explicit FakeClock(TimePoint start = TimePoint{});

    [[nodiscard]] TimePoint now() const override;

    /// Moves time forward by `step`. Fails with InvalidArgument (time unchanged) if `step` is
    /// negative.
    [[nodiscard]] Expected<void> advance(Duration step);

private:
    TimePoint m_now;
};

} // namespace coil::platform::testing
