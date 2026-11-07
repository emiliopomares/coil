#pragma once

#include <coil/foundation/assert.hpp>
#include <coil/foundation/error.hpp>
#include <coil/tasks/async_result.hpp>

#include <optional>
#include <type_traits>
#include <utility>

namespace coil::tasks::testing {

/// Test double for a producer (ADR-0014): the test holds it and decides when and how the operation
/// ends, and can observe the consumer (SPEC-0024 rule 10). Hands out its result once.
template <typename T>
class ManualPromise {
public:
    ManualPromise()
        : m_pair(makeAsync<T>()) {}

    /// The connected result. Precondition: called once.
    [[nodiscard]] AsyncResult<T> takeResult() {
        COIL_EXPECTS(!m_resultTaken);
        m_resultTaken = true;
        return std::move(m_pair.result);
    }

    template <typename U = T>
        requires(!std::is_void_v<U>)
    [[nodiscard]] bool complete(U value) {
        return m_pair.promise.complete(std::move(value));
    }

    template <typename U = T>
        requires std::is_void_v<U>
    [[nodiscard]] bool complete() {
        return m_pair.promise.complete();
    }

    [[nodiscard]] bool fail(Error error) { return m_pair.promise.fail(std::move(error)); }

    /// Whether the consumer cancelled or dropped the result.
    [[nodiscard]] bool cancelRequested() const {
        return m_pair.promise.stopToken().stop_requested();
    }

    /// Whether the outcome is known (completed, failed or cancelled).
    [[nodiscard]] bool settled() const { return m_pair.promise.settled(); }

private:
    AsyncPair<T> m_pair;
    bool m_resultTaken = false;
};

} // namespace coil::tasks::testing
