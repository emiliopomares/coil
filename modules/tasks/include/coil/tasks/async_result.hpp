#pragma once

#include <coil/foundation/assert.hpp>
#include <coil/foundation/error.hpp>

#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace coil::tasks {

template <typename T>
class AsyncResult;
template <typename T>
class AsyncPromise;
template <typename T>
struct AsyncPair;

namespace testing {
template <typename T>
class ManualPromise;
} // namespace testing

namespace detail {

/// The error an abandoned promise leaves behind (rule 6).
[[nodiscard]] Error abandonedError();

/// State shared by one promise and one result (SPEC-0024 §4.4). Guarded by `mutex`.
template <typename T>
struct SharedState {
    mutable std::mutex mutex;
    std::optional<Expected<T>> outcome; ///< Set once (rule 3).
    bool taken = false;                 ///< take() has moved the outcome out (rule 4).
    std::stop_source stop;              ///< Requested by cancel() or a dropped result.
    /// Built up front, so that abandoning (in a destructor) cannot fail.
    Error abandoned = abandonedError();

    /// Whether the outcome is known (set, or set and taken).
    [[nodiscard]] bool known() const {
        const std::scoped_lock lock{mutex};
        return outcome.has_value() || taken;
    }

    /// Sets the outcome if it is not known yet; returns whether it did (rule 3).
    [[nodiscard]] bool settle(Expected<T> value) {
        const std::scoped_lock lock{mutex};
        if (outcome.has_value() || taken) {
            return false;
        }
        outcome.emplace(std::move(value));
        return true;
    }

    /// Sets the abandoned error if the outcome is not known yet (rule 6).
    void abandon() noexcept {
        const std::scoped_lock lock{mutex};
        if (!outcome.has_value() && !taken) {
            outcome.emplace(std::unexpected(std::move(abandoned)));
        }
    }
};

/// A shared state that already holds `value` (rule 2).
template <typename T>
[[nodiscard]] std::shared_ptr<SharedState<T>> settledState(Expected<T> value) {
    auto state = std::make_shared<SharedState<T>>();
    state->outcome.emplace(std::move(value));
    return state;
}

} // namespace detail

/// The consumer side of an asynchronous operation: a value (or error) that arrives later
/// (ADR-0014). Move-only and single-consumer. Read by polling; it never blocks.
template <typename T>
class AsyncResult {
public:
    /// An already completed result, with no promise (rule 2).
    template <typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] static AsyncResult ready(U value) {
        return AsyncResult{detail::settledState<T>(Expected<T>{std::move(value)})};
    }

    /// An already successful AsyncResult<void>, with no promise (rule 2).
    template <typename U = T>
        requires std::is_void_v<U>
    [[nodiscard]] static AsyncResult ready() {
        return AsyncResult{detail::settledState<T>(Expected<T>{})};
    }

    /// An already failed result, with no promise (rule 2).
    [[nodiscard]] static AsyncResult failed(Error error) {
        return AsyncResult{detail::settledState<T>(std::unexpected(std::move(error)))};
    }

    AsyncResult(const AsyncResult&) = delete;
    AsyncResult& operator=(const AsyncResult&) = delete;
    AsyncResult(AsyncResult&& other) noexcept = default;

    /// Drops the current operation as the destructor does, then takes over `other`'s.
    AsyncResult& operator=(AsyncResult&& other) noexcept {
        if (this != &other) {
            release();
            m_state = std::move(other.m_state);
        }
        return *this;
    }

    /// Requests stop if the outcome is not known yet (rule 7).
    ~AsyncResult() { release(); }

    /// Whether the outcome is known: completed, failed, cancelled or abandoned.
    [[nodiscard]] bool isReady() const {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->known();
    }

    /// Moves the outcome out, once (rule 4). nullopt while not ready, and after it was taken.
    [[nodiscard]] std::optional<Expected<T>> take() {
        COIL_EXPECTS(m_state != nullptr);
        const std::scoped_lock lock{m_state->mutex};
        if (m_state->taken || !m_state->outcome.has_value()) {
            return std::nullopt;
        }
        m_state->taken = true;
        return std::exchange(m_state->outcome, std::nullopt);
    }

    /// Asks the producer to stop. If the outcome is not known yet, it becomes Cancelled (rule 5).
    void cancel() {
        COIL_EXPECTS(m_state != nullptr);
        if (m_state->settle(std::unexpected(Error{ErrorCode::Cancelled}))) {
            // Outside the lock: stop callbacks may complete the promise.
            m_state->stop.request_stop();
        }
    }

private:
    template <typename U>
    friend AsyncPair<U> makeAsync();

    explicit AsyncResult(std::shared_ptr<detail::SharedState<T>> state) noexcept
        : m_state(std::move(state)) {}

    /// Lets go of the operation; nobody waits for it any more if it is unfinished (rule 7).
    void release() noexcept {
        if (m_state != nullptr && !m_state->known()) {
            m_state->stop.request_stop();
        }
        m_state.reset();
    }

    std::shared_ptr<detail::SharedState<T>> m_state;
};

/// The producer side of an asynchronous operation (ADR-0014). Move-only. complete(), fail(), the
/// destructor and stopToken() may be called from any thread (rule 8).
template <typename T>
class AsyncPromise {
public:
    AsyncPromise(const AsyncPromise&) = delete;
    AsyncPromise& operator=(const AsyncPromise&) = delete;
    AsyncPromise(AsyncPromise&& other) noexcept = default;

    /// Abandons the current operation as the destructor does, then takes over `other`'s.
    AsyncPromise& operator=(AsyncPromise&& other) noexcept {
        if (this != &other) {
            release();
            m_state = std::move(other.m_state);
        }
        return *this;
    }

    /// Abandons the operation if the outcome is not known yet (rule 6).
    ~AsyncPromise() { release(); }

    /// Sets the value if the outcome is not known yet. Returns whether this call set it (rule 3).
    template <typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] bool complete(U value) {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->settle(Expected<T>{std::move(value)});
    }

    /// Sets success for AsyncPromise<void> if the outcome is not known yet (rule 3).
    template <typename U = T>
        requires std::is_void_v<U>
    [[nodiscard]] bool complete() {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->settle(Expected<T>{});
    }

    /// Sets the error if the outcome is not known yet. Returns whether this call set it (rule 3).
    [[nodiscard]] bool fail(Error error) {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->settle(std::unexpected(std::move(error)));
    }

    /// Signalled when the consumer cancels or drops the result (rules 5, 7).
    [[nodiscard]] std::stop_token stopToken() const {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->stop.get_token();
    }

private:
    template <typename U>
    friend AsyncPair<U> makeAsync();
    friend class testing::ManualPromise<T>;

    explicit AsyncPromise(std::shared_ptr<detail::SharedState<T>> state) noexcept
        : m_state(std::move(state)) {}

    /// Whether the outcome is known (for ManualPromise::settled()).
    [[nodiscard]] bool settled() const {
        COIL_EXPECTS(m_state != nullptr);
        return m_state->known();
    }

    /// Lets go of the operation, leaving an error if it is unfinished (rule 6).
    void release() noexcept {
        if (m_state != nullptr) {
            m_state->abandon();
        }
        m_state.reset();
    }

    std::shared_ptr<detail::SharedState<T>> m_state;
};

/// A connected promise/result pair: the producer keeps the promise, the consumer the result.
template <typename T>
struct AsyncPair {
    AsyncPromise<T> promise;
    AsyncResult<T> result;
};

/// Creates a connected pair whose result is not ready (rule 1).
template <typename T>
[[nodiscard]] AsyncPair<T> makeAsync() {
    const auto state = std::make_shared<detail::SharedState<T>>();
    return AsyncPair<T>{.promise = AsyncPromise<T>{state}, .result = AsyncResult<T>{state}};
}

} // namespace coil::tasks
