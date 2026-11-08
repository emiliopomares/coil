#pragma once

#include <coil/foundation/assert.hpp>
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

    /// Queues `task` (rule 1). Precondition: `task` is not empty.
    void post(UiTask task) override {
        COIL_EXPECTS(static_cast<bool>(task));
        const std::scoped_lock lock{m_mutex};
        m_tasks.push_back(std::move(task));
    }

    /// Tasks queued and not yet run.
    [[nodiscard]] std::size_t pending() const {
        const std::scoped_lock lock{m_mutex};
        return m_tasks.size();
    }

    /// Runs, in order, the tasks queued when the call starts; tasks they post wait for the next
    /// call. Returns how many ran. Precondition: not called from inside a task it is running.
    [[nodiscard]] std::size_t runPending() {
        COIL_EXPECTS(!m_running);
        std::deque<UiTask> batch;
        {
            const std::scoped_lock lock{m_mutex};
            batch.swap(m_tasks);
        }
        m_running = true;
        // No lock is held while tasks run, so they (and other threads) can post.
        for (UiTask& task : batch) {
            task();
        }
        m_running = false;
        return batch.size();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<UiTask> m_tasks; ///< Guarded by m_mutex.
    bool m_running = false;     ///< Inside runPending(); only touched by the draining thread.
};

} // namespace coil::tasks::testing
