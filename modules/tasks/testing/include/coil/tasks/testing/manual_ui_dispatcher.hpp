#pragma once

#include <coil/tasks/ui_dispatcher.hpp>

#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace coil::tasks::testing {

/// A UiDispatcher that tests drain on demand (ADR-0014). post() and pending() may be called from
/// any thread; runPending() runs tasks on the calling thread (SPEC-0030 rule 2).
class ManualUiDispatcher final : public UiDispatcher {
public:
    ManualUiDispatcher() = default;
    ManualUiDispatcher(const ManualUiDispatcher&) = delete;
    ManualUiDispatcher& operator=(const ManualUiDispatcher&) = delete;
    ManualUiDispatcher(ManualUiDispatcher&&) = delete;
    ManualUiDispatcher& operator=(ManualUiDispatcher&&) = delete;

    /// Destroys pending tasks without running them (rule 3).
    ~ManualUiDispatcher() override = default;

    // Red-phase stub (SPEC-0030): drops the task.
    void post(UiTask task) override {
        const std::scoped_lock lock{m_mutex};
        const UiTask dropped{std::move(task)};
    }

    /// Tasks queued and not yet run.
    [[nodiscard]] std::size_t pending() const {
        const std::scoped_lock lock{m_mutex};
        return 0; // Red-phase stub.
    }

    /// Runs, in order, the tasks queued when the call starts; tasks they post wait for the next
    /// call. Returns how many ran. Precondition: not called from inside a task it is running.
    [[nodiscard]] std::size_t runPending() {
        const std::scoped_lock lock{m_mutex};
        return m_tasks.size() * 0; // Red-phase stub.
    }

private:
    mutable std::mutex m_mutex;
    std::deque<UiTask> m_tasks; ///< Guarded by m_mutex.
};

} // namespace coil::tasks::testing
