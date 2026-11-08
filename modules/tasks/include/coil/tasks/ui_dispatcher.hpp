#pragma once

#include <coil/foundation/assert.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace coil::tasks {

/// A move-only, type-erased void() callable (SPEC-0030 Q1). std::move_only_function is not
/// allowed (CLAUDE.md section 5), and std::function needs copyable callables.
class UiTask {
public:
    /// An empty task.
    UiTask() noexcept = default;

    /// Takes ownership of `callable`, which may be move-only. Explicit, so every conversion to a
    /// task is visible: post(UiTask{[] { ... }}) (SPEC-0030 Amendment A).
    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, UiTask> && std::invocable<std::decay_t<F>&>)
    explicit UiTask(F&& callable)
        : m_callable(std::make_unique<Holder<std::decay_t<F>>>(std::forward<F>(callable))) {}

    UiTask(UiTask&&) noexcept = default;
    UiTask& operator=(UiTask&&) noexcept = default;
    UiTask(const UiTask&) = delete;
    UiTask& operator=(const UiTask&) = delete;
    ~UiTask() = default;

    /// Whether it holds a callable.
    [[nodiscard]] explicit operator bool() const noexcept { return m_callable != nullptr; }

    /// Runs the callable. Precondition: not empty.
    void operator()() {
        COIL_EXPECTS(m_callable != nullptr);
        m_callable->call();
    }

private:
    class Callable {
    public:
        Callable() = default;
        Callable(const Callable&) = delete;
        Callable& operator=(const Callable&) = delete;
        Callable(Callable&&) = delete;
        Callable& operator=(Callable&&) = delete;
        virtual ~Callable() = default;
        virtual void call() = 0;
    };

    template <typename F>
    class Holder final : public Callable {
    public:
        explicit Holder(F callable)
            : m_function(std::move(callable)) {}

        // Red-phase stub (SPEC-0030): does not run the callable.
        void call() override { static_cast<void>(&m_function); }

    private:
        F m_function;
    };

    std::unique_ptr<Callable> m_callable;
};

/// Port: runs tasks on the UI thread (ADR-0010). Implemented by the app loop; tests use
/// testing::ManualUiDispatcher.
class UiDispatcher {
public:
    /// Pure, so the port has no data and implementations define their own.
    virtual ~UiDispatcher() = 0;
    UiDispatcher(const UiDispatcher&) = delete;
    UiDispatcher& operator=(const UiDispatcher&) = delete;
    UiDispatcher(UiDispatcher&&) = delete;
    UiDispatcher& operator=(UiDispatcher&&) = delete;

    /// Queues `task` to run later on the UI thread, after the tasks posted before it. Callable from
    /// any thread; never runs `task` before returning. Precondition: `task` is not empty.
    virtual void post(UiTask task) = 0;

protected:
    UiDispatcher() = default;
};

} // namespace coil::tasks
