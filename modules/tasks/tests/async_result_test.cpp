#include <coil/foundation/error.hpp>
#include <coil/tasks/async_result.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using coil::Error;
using coil::ErrorCode;
using coil::Expected;
using coil::tasks::AsyncPromise;
using coil::tasks::AsyncResult;
using coil::tasks::makeAsync;

/// The result's outcome is `expected`; takes it.
template <typename T>
::testing::AssertionResult givesValue(AsyncResult<T>& result, const T& expected) {
    std::optional<Expected<T>> outcome = result.take();
    if (!outcome.has_value()) {
        return ::testing::AssertionFailure() << "no outcome";
    }
    if (!outcome->has_value()) {
        return ::testing::AssertionFailure()
               << "failed with " << std::format("{}", outcome->error());
    }
    if (!(**outcome == expected)) {
        return ::testing::AssertionFailure() << "a different value";
    }
    return ::testing::AssertionSuccess();
}

/// The void result's outcome is success; takes it.
::testing::AssertionResult givesSuccess(AsyncResult<void>& result) {
    std::optional<Expected<void>> outcome = result.take();
    if (!outcome.has_value()) {
        return ::testing::AssertionFailure() << "no outcome";
    }
    if (!outcome->has_value()) {
        return ::testing::AssertionFailure()
               << "failed with " << std::format("{}", outcome->error());
    }
    return ::testing::AssertionSuccess();
}

/// The result's outcome is an error with `code`; takes it.
template <typename T>
::testing::AssertionResult givesError(AsyncResult<T>& result, ErrorCode code) {
    std::optional<Expected<T>> outcome = result.take();
    if (!outcome.has_value()) {
        return ::testing::AssertionFailure() << "no outcome";
    }
    if (outcome->has_value()) {
        return ::testing::AssertionFailure() << "succeeded, expected " << std::format("{}", code);
    }
    if (outcome->error().code() != code) {
        return ::testing::AssertionFailure()
               << "failed with " << std::format("{}", outcome->error()) << ", expected "
               << std::format("{}", code);
    }
    return ::testing::AssertionSuccess();
}

/// A type without a default constructor (AC-1).
struct NoDefault {
    explicit NoDefault(int number)
        : value(number) {}

    int value;

    friend bool operator==(const NoDefault&, const NoDefault&) = default;
};

TEST(AsyncResult, AC1_ReadyAndFailedResults) {
    AsyncResult<int> ready = AsyncResult<int>::ready(42);
    EXPECT_TRUE(ready.isReady());
    EXPECT_TRUE(givesValue(ready, 42));
    EXPECT_FALSE(ready.take().has_value()) << "the outcome was taken twice";
    EXPECT_TRUE(ready.isReady());

    AsyncResult<int> failed = AsyncResult<int>::failed(Error{ErrorCode::NotFound});
    EXPECT_TRUE(failed.isReady());
    EXPECT_TRUE(givesError(failed, ErrorCode::NotFound));

    AsyncResult<void> done = AsyncResult<void>::ready();
    EXPECT_TRUE(done.isReady());
    EXPECT_TRUE(givesSuccess(done));

    AsyncResult<std::unique_ptr<int>> owned =
        AsyncResult<std::unique_ptr<int>>::ready(std::make_unique<int>(3));
    const std::optional<Expected<std::unique_ptr<int>>> pointer = owned.take();
    if (!pointer.has_value() || !pointer->has_value() || **pointer == nullptr) {
        ADD_FAILURE() << "no pointer";
    } else {
        EXPECT_EQ(***pointer, 3);
    }

    AsyncResult<NoDefault> noDefault = AsyncResult<NoDefault>::ready(NoDefault{5});
    EXPECT_TRUE(givesValue(noDefault, NoDefault{5}));
}

TEST(AsyncResult, AC2_PromiseCompletesResult) {
    auto pair = makeAsync<int>();
    EXPECT_FALSE(pair.result.isReady());
    EXPECT_FALSE(pair.result.take().has_value());
    EXPECT_TRUE(pair.promise.complete(7));
    EXPECT_TRUE(pair.result.isReady());
    EXPECT_TRUE(givesValue(pair.result, 7));

    auto failing = makeAsync<int>();
    EXPECT_TRUE(failing.promise.fail(Error{ErrorCode::NotFound}));
    EXPECT_TRUE(givesError(failing.result, ErrorCode::NotFound));

    auto done = makeAsync<void>();
    EXPECT_FALSE(done.result.isReady());
    EXPECT_TRUE(done.promise.complete());
    EXPECT_TRUE(givesSuccess(done.result));

    auto failedVoid = makeAsync<void>();
    EXPECT_TRUE(failedVoid.promise.fail(Error{ErrorCode::Unavailable}));
    EXPECT_TRUE(givesError(failedVoid.result, ErrorCode::Unavailable));
}

TEST(AsyncResult, AC3_FirstOutcomeWins) {
    auto pair = makeAsync<int>();
    EXPECT_TRUE(pair.promise.complete(1));
    EXPECT_FALSE(pair.promise.complete(2));
    EXPECT_FALSE(pair.promise.fail(Error{ErrorCode::Internal}));
    EXPECT_TRUE(givesValue(pair.result, 1));

    auto failing = makeAsync<int>();
    EXPECT_TRUE(failing.promise.fail(Error{ErrorCode::NotFound}));
    EXPECT_FALSE(failing.promise.complete(3));
    EXPECT_TRUE(givesError(failing.result, ErrorCode::NotFound));
}

TEST(AsyncResult, AC4_CancelRequestsStop) {
    auto pending = makeAsync<int>();
    EXPECT_FALSE(pending.promise.stopToken().stop_requested());
    pending.result.cancel();
    EXPECT_TRUE(pending.promise.stopToken().stop_requested());
    EXPECT_TRUE(pending.result.isReady());
    EXPECT_FALSE(pending.promise.complete(4));
    EXPECT_TRUE(givesError(pending.result, ErrorCode::Cancelled));

    auto completed = makeAsync<int>();
    EXPECT_TRUE(completed.promise.complete(5));
    completed.result.cancel();
    EXPECT_FALSE(completed.promise.stopToken().stop_requested());
    EXPECT_TRUE(givesValue(completed.result, 5));

    auto twice = makeAsync<int>();
    twice.result.cancel();
    twice.result.cancel();
    EXPECT_TRUE(givesError(twice.result, ErrorCode::Cancelled));
}

TEST(AsyncResult, AC5_DroppedSides) {
    auto abandoned = makeAsync<int>();
    {
        const AsyncPromise<int> dropped = std::move(abandoned.promise);
    }
    EXPECT_TRUE(abandoned.result.isReady());
    const std::optional<Expected<int>> outcome = abandoned.result.take();
    if (!outcome.has_value() || outcome->has_value()) {
        ADD_FAILURE() << "an abandoned promise did not leave an error";
    } else {
        EXPECT_EQ(outcome->error().code(), ErrorCode::Internal);
        EXPECT_EQ(outcome->error().message(), "promise abandoned");
    }

    auto unwanted = makeAsync<int>();
    {
        const AsyncResult<int> dropped = std::move(unwanted.result);
    }
    EXPECT_TRUE(unwanted.promise.stopToken().stop_requested());

    auto finished = makeAsync<int>();
    EXPECT_TRUE(finished.promise.complete(6));
    {
        const AsyncPromise<int> dropped = std::move(finished.promise);
    }
    EXPECT_TRUE(givesValue(finished.result, 6));

    auto delivered = makeAsync<int>();
    EXPECT_TRUE(delivered.promise.complete(8));
    {
        const AsyncResult<int> dropped = std::move(delivered.result);
    }
    EXPECT_FALSE(delivered.promise.stopToken().stop_requested());

    // Move assignment over a live side behaves like destroying it.
    auto overwrittenPromise = makeAsync<int>();
    auto replacementPromise = makeAsync<int>();
    overwrittenPromise.promise = std::move(replacementPromise.promise);
    EXPECT_TRUE(givesError(overwrittenPromise.result, ErrorCode::Internal));

    auto overwrittenResult = makeAsync<int>();
    auto replacementResult = makeAsync<int>();
    overwrittenResult.result = std::move(replacementResult.result);
    EXPECT_TRUE(overwrittenResult.promise.stopToken().stop_requested());
}

TEST(AsyncResult, AC6_CompletesFromAnotherThread) {
    auto pair = makeAsync<int>();
    std::thread worker{
        [promise = std::move(pair.promise)] mutable { EXPECT_TRUE(promise.complete(7)); }};
    worker.join();
    EXPECT_TRUE(givesValue(pair.result, 7));

    // Completion on a second thread while this one polls: no lost or torn outcomes.
    constexpr int kPairs = 1000;
    std::vector<AsyncPromise<int>> promises;
    std::vector<AsyncResult<int>> results;
    promises.reserve(kPairs);
    results.reserve(kPairs);
    for (int i = 0; i < kPairs; ++i) {
        auto next = makeAsync<int>();
        promises.push_back(std::move(next.promise));
        results.push_back(std::move(next.result));
    }
    std::atomic<bool> finished{false};
    std::thread producer{[&promises, &finished] {
        int value = 0;
        for (AsyncPromise<int>& promise : promises) {
            EXPECT_TRUE(promise.complete(value++));
        }
        finished.store(true);
    }};
    // Once seen ready, a result stays ready.
    std::vector<bool> seenReady(results.size(), false);
    while (!finished.load()) {
        auto seen = seenReady.begin();
        for (const AsyncResult<int>& result : results) {
            const bool ready = result.isReady();
            EXPECT_TRUE(ready || !*seen) << "a result stopped being ready";
            *seen = *seen || ready;
            ++seen;
        }
    }
    producer.join();
    int expected = 0;
    for (AsyncResult<int>& result : results) {
        EXPECT_TRUE(givesValue(result, expected)) << "pair " << expected;
        ++expected;
    }
}

} // namespace
