#pragma once

#include <coil/foundation/assert.hpp>
#include <coil/foundation/error.hpp>

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

/// State shared by one promise and one result (SPEC-0024 §4.4). Guarded by `mutex`.
template <typename T>
struct SharedState {
    std::mutex mutex;
    std::optional<Expected<T>> outcome; ///< Set once (rule 3).
    bool taken = false;                 ///< take() has moved the outcome out (rule 4).
    std::stop_source stop;              ///< Requested by cancel() or a dropped result.
};

/// The error an abandoned promise leaves behind (rule 6).
[[nodiscard]] Error abandonedError();

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
        static_cast<void>(value);
        return AsyncResult{std::make_shared<detail::SharedState<T>>()}; // Red stub (SPEC-0024)
    }

    /// An already successful AsyncResult<void>, with no promise (rule 2).
    template <typename U = T>
        requires std::is_void_v<U>
    [[nodiscard]] static AsyncResult ready() {
        return AsyncResult{std::make_shared<detail::SharedState<T>>()}; // Red stub (SPEC-0024)
    }

    /// An already failed result, with no promise (rule 2).
    [[nodiscard]] static AsyncResult failed(Error error) {
        const Error ignored{std::move(error)};
        static_cast<void>(ignored);
        return AsyncResult{std::make_shared<detail::SharedState<T>>()}; // Red stub (SPEC-0024)
    }

    AsyncResult(const AsyncResult&) = delete;
    AsyncResult& operator=(const AsyncResult&) = delete;
    AsyncResult(AsyncResult&& other) noexcept = default;
    AsyncResult& operator=(AsyncResult&& other) noexcept = default;
    /// Requests stop if the outcome is not known yet (rule 7).
    ~AsyncResult() = default;

    /// Whether the outcome is known: completed, failed, cancelled or abandoned.
    [[nodiscard]] bool isReady() const {
        COIL_EXPECTS(m_state != nullptr);
        return false; // Red stub (SPEC-0024)
    }

    /// Moves the outcome out, once (rule 4). nullopt while not ready, and after it was taken.
    [[nodiscard]] std::optional<Expected<T>> take() {
        COIL_EXPECTS(m_state != nullptr);
        return std::nullopt; // Red stub (SPEC-0024)
    }

    /// Asks the producer to stop. If the outcome is not known yet, it becomes Cancelled (rule 5).
    void cancel() {
        COIL_EXPECTS(m_state != nullptr); // Red stub (SPEC-0024)
    }

private:
    template <typename U>
    friend AsyncPair<U> makeAsync();

    explicit AsyncResult(std::shared_ptr<detail::SharedState<T>> state) noexcept
        : m_state(std::move(state)) {}

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
    AsyncPromise& operator=(AsyncPromise&& other) noexcept = default;
    /// Abandons the operation if the outcome is not known yet (rule 6).
    ~AsyncPromise() = default;

    /// Sets the value if the outcome is not known yet. Returns whether this call set it (rule 3).
    template <typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] bool complete(U value) {
        COIL_EXPECTS(m_state != nullptr);
        static_cast<void>(value);
        return false; // Red stub (SPEC-0024)
    }

    /// Sets success for AsyncPromise<void> if the outcome is not known yet (rule 3).
    template <typename U = T>
        requires std::is_void_v<U>
    [[nodiscard]] bool complete() {
        COIL_EXPECTS(m_state != nullptr);
        return false; // Red stub (SPEC-0024)
    }

    /// Sets the error if the outcome is not known yet. Returns whether this call set it (rule 3).
    [[nodiscard]] bool fail(Error error) {
        COIL_EXPECTS(m_state != nullptr);
        const Error ignored{std::move(error)};
        static_cast<void>(ignored);
        return false; // Red stub (SPEC-0024)
    }

    /// Signalled when the consumer cancels or drops the result (rules 5, 7).
    [[nodiscard]] std::stop_token stopToken() const {
        COIL_EXPECTS(m_state != nullptr);
        return {}; // Red stub (SPEC-0024)
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
        return false; // Red stub (SPEC-0024)
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
