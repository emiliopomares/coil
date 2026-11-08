#include <coil/foundation/error.hpp>
#include <coil/tasks/async_result.hpp>
#include <coil/tasks/testing/manual_promise.hpp>
#include <coil/tasks/testing/manual_ui_dispatcher.hpp>
#include <coil/tasks/ui_dispatcher.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using coil::Error;
using coil::ErrorCode;
using coil::Expected;
using coil::tasks::AsyncResult;
using coil::tasks::makeAsync;
using coil::tasks::UiTask;
using coil::tasks::testing::ManualPromise;
using coil::tasks::testing::ManualUiDispatcher;

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

/// The error code of `outcome`, or nullopt for success.
std::optional<ErrorCode> codeOf(const Expected<int>& outcome) {
    if (outcome.has_value()) {
        return std::nullopt;
    }
    return outcome.error().code();
}

TEST(Then, AC4_ContinuationRunsOnDrain) {
    ManualUiDispatcher ui;
    ManualPromise<int> source;
    int calls = 0;
    AsyncResult<std::string> chained =
        source.takeResult().then(ui, [&calls](Expected<int> outcome) {
            ++calls;
            return std::format("got {}", outcome.value_or(-1));
        });
    EXPECT_FALSE(chained.isReady());
    EXPECT_EQ(ui.pending(), 0U) << "posted before the outcome was known";

    ASSERT_TRUE(source.complete(5));
    EXPECT_EQ(calls, 0) << "the continuation ran inline";
    EXPECT_EQ(ui.pending(), 1U);
    EXPECT_FALSE(chained.isReady());

    EXPECT_EQ(ui.runPending(), 1U);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(givesValue(chained, std::string{"got 5"}));

    // An already ready source: posted at once, still not run inline.
    int readyCalls = 0;
    AsyncResult<int> doubled =
        AsyncResult<int>::ready(21).then(ui, [&readyCalls](Expected<int> outcome) {
            ++readyCalls;
            return outcome.value_or(0) * 2;
        });
    EXPECT_EQ(ui.pending(), 1U);
    EXPECT_EQ(readyCalls, 0) << "the continuation ran inline";
    EXPECT_FALSE(doubled.isReady());
    EXPECT_EQ(ui.runPending(), 1U);
    EXPECT_EQ(readyCalls, 1);
    EXPECT_TRUE(givesValue(doubled, 42));
}

TEST(Then, AC5_ErrorsAndReturnShapes) {
    ManualUiDispatcher ui;
    std::vector<std::optional<ErrorCode>> seen;
    const auto record = [&seen](Expected<int> outcome) { seen.push_back(codeOf(outcome)); };

    ManualPromise<int> failing;
    AsyncResult<void> afterFailure = failing.takeResult().then(ui, record);
    ManualPromise<int> cancelling;
    AsyncResult<void> afterCancel = cancelling.takeResult().then(ui, record);
    AsyncResult<void> afterAbandon = AsyncResult<void>::ready();
    {
        ManualPromise<int> abandoned;
        afterAbandon = abandoned.takeResult().then(ui, record);
    }
    ASSERT_TRUE(failing.fail(Error{ErrorCode::NotFound, "missing"}));
    ASSERT_TRUE(cancelling.fail(Error{ErrorCode::Cancelled}));
    EXPECT_EQ(ui.runPending(), 3U);
    EXPECT_EQ(seen, (std::vector<std::optional<ErrorCode>>{ErrorCode::Internal, ErrorCode::NotFound,
                                                           ErrorCode::Cancelled}));
    EXPECT_TRUE(givesSuccess(afterFailure));
    EXPECT_TRUE(givesSuccess(afterCancel));
    EXPECT_TRUE(givesSuccess(afterAbandon));

    // Return shapes: Expected<U> is unwrapped; an error in it fails the chain.
    AsyncResult<std::string> rejected =
        AsyncResult<int>::ready(1).then(ui, [](Expected<int> /*outcome*/) -> Expected<std::string> {
            return std::unexpected(Error{ErrorCode::InvalidArgument, "bad"});
        });
    AsyncResult<std::string> accepted =
        AsyncResult<int>::ready(1).then(ui, [](Expected<int> outcome) -> Expected<std::string> {
            return std::format("ok {}", outcome.value_or(0));
        });
    AsyncResult<int> fromVoid = AsyncResult<void>::ready().then(
        ui, [](Expected<void> outcome) { return outcome.has_value() ? 1 : 0; });
    static_assert(std::is_same_v<decltype(rejected), AsyncResult<std::string>>);
    EXPECT_EQ(ui.runPending(), 3U);
    EXPECT_TRUE(givesError(rejected, ErrorCode::InvalidArgument));
    EXPECT_TRUE(givesValue(accepted, std::string{"ok 1"}));
    EXPECT_TRUE(givesValue(fromVoid, 1));
}

TEST(Then, AC6_CancellationReachesTheSource) {
    ManualUiDispatcher ui;
    int calls = 0;
    const auto count = [&calls](Expected<int> outcome) {
        ++calls;
        return outcome.value_or(0);
    };
    {
        // Control: then alone does not cancel; once the continuation ran, cancelling changes
        // nothing.
        ManualPromise<int> source;
        AsyncResult<int> chained = source.takeResult().then(ui, count);
        EXPECT_FALSE(source.cancelRequested()) << "then cancelled the source";
        ASSERT_TRUE(source.complete(4));
        EXPECT_EQ(ui.runPending(), 1U);
        EXPECT_EQ(calls, 1);
        chained.cancel();
        EXPECT_TRUE(givesValue(chained, 4));
    }
    {
        ManualPromise<int> source;
        AsyncResult<int> chained = source.takeResult().then(ui, count);
        chained.cancel();
        EXPECT_TRUE(source.cancelRequested()) << "cancelling the chain did not reach the source";
        EXPECT_TRUE(givesError(chained, ErrorCode::Cancelled));
        [[maybe_unused]] const bool late = source.complete(3);
        [[maybe_unused]] const std::size_t ran = ui.runPending();
        EXPECT_EQ(calls, 1) << "the continuation ran after cancellation";
    }
    {
        ManualPromise<int> source;
        {
            const AsyncResult<int> dropped = source.takeResult().then(ui, count);
        }
        EXPECT_TRUE(source.cancelRequested()) << "dropping the chain did not reach the source";
        [[maybe_unused]] const bool late = source.complete(3);
        [[maybe_unused]] const std::size_t ran = ui.runPending();
        EXPECT_EQ(calls, 1) << "the continuation ran after its result was dropped";
    }
}

TEST(Then, AC7_DroppedTaskAbandonsTheChain) {
    const auto witness = std::make_shared<int>(0);
    const auto touch = [witness](Expected<int> outcome) {
        ++*witness;
        return outcome.value_or(0);
    };
    ManualPromise<int> source;
    AsyncResult<int> chained = AsyncResult<int>::ready(0);
    {
        ManualUiDispatcher ui;
        // Control: the same continuation runs when the dispatcher is drained.
        AsyncResult<int> control = AsyncResult<int>::ready(1).then(ui, touch);
        ASSERT_EQ(ui.runPending(), 1U);
        ASSERT_EQ(*witness, 1);
        EXPECT_TRUE(givesValue(control, 1));

        chained = source.takeResult().then(ui, touch);
        ASSERT_TRUE(source.complete(2));
        EXPECT_EQ(ui.pending(), 1U);
    }
    EXPECT_EQ(*witness, 1) << "the dropped continuation ran";
    EXPECT_EQ(witness.use_count(), 2) << "the dropped continuation's captures were kept";
    EXPECT_TRUE(givesError(chained, ErrorCode::Internal));
}

TEST(Then, AC8_CompletesFromAnotherThread) {
    ManualUiDispatcher ui;
    auto pair = makeAsync<int>();
    std::thread::id ranOn;
    AsyncResult<int> chained = std::move(pair.result).then(ui, [&ranOn](Expected<int> outcome) {
        ranOn = std::this_thread::get_id();
        return outcome.value_or(0) + 1;
    });
    std::thread worker{
        [promise = std::move(pair.promise)] mutable { EXPECT_TRUE(promise.complete(7)); }};
    worker.join();
    EXPECT_EQ(ui.pending(), 1U);
    EXPECT_EQ(ui.runPending(), 1U);
    EXPECT_EQ(ranOn, std::this_thread::get_id()) << "the continuation ran off the draining thread";
    EXPECT_TRUE(givesValue(chained, 8));

    // Concurrent posts from several threads while this one drains: each task runs exactly once.
    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;
    std::vector<int> runs(static_cast<std::size_t>(kThreads * kPerThread), 0);
    std::atomic<int> finished{0};
    std::vector<std::thread> posters;
    posters.reserve(kThreads);
    for (int thread = 0; thread < kThreads; ++thread) {
        posters.emplace_back([&ui, &runs, &finished, thread] {
            for (int i = 0; i < kPerThread; ++i) {
                const std::size_t index =
                    (static_cast<std::size_t>(thread) * kPerThread) + static_cast<std::size_t>(i);
                ui.post(UiTask{[&runs, index] { ++runs.at(index); }});
            }
            finished.fetch_add(1);
        });
    }
    std::size_t ran = 0;
    while (finished.load() < kThreads) {
        ran += ui.runPending();
    }
    for (std::thread& poster : posters) {
        poster.join();
    }
    ran += ui.runPending();
    EXPECT_EQ(ran, static_cast<std::size_t>(kThreads * kPerThread));
    EXPECT_EQ(runs, std::vector<int>(runs.size(), 1));
}

} // namespace
