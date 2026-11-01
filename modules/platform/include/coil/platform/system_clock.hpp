#pragma once

#include <coil/platform/clock.hpp>

namespace coil::platform {

/// Real monotonic time (std::chrono::steady_clock). Thread-safe.
class SystemClock final : public Clock {
public:
    [[nodiscard]] TimePoint now() const override;
};

} // namespace coil::platform
