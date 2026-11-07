#include <coil/foundation/error.hpp>
#include <coil/tasks/async_result.hpp>
#include <coil/tasks/testing/manual_promise.hpp>

#include <gtest/gtest.h>

#include <optional>

namespace {

using coil::Error;
using coil::ErrorCode;
using coil::Expected;
using coil::tasks::AsyncResult;
using coil::tasks::testing::ManualPromise;

TEST(ManualPromise, AC7_ObservesTheConsumer) {
    ManualPromise<int> completed;
    AsyncResult<int> result = completed.takeResult();
    EXPECT_FALSE(result.isReady());
    EXPECT_FALSE(completed.cancelRequested());
    EXPECT_FALSE(completed.settled());
    EXPECT_TRUE(completed.complete(3));
    EXPECT_TRUE(completed.settled());
    const std::optional<Expected<int>> value = result.take();
    if (!value.has_value()) {
        ADD_FAILURE() << "no outcome";
    } else {
        EXPECT_EQ(value->value_or(0), 3);
    }

    ManualPromise<int> cancelled;
    AsyncResult<int> cancelledResult = cancelled.takeResult();
    cancelledResult.cancel();
    EXPECT_TRUE(cancelled.cancelRequested());
    EXPECT_TRUE(cancelled.settled());

    ManualPromise<int> dropped;
    {
        const AsyncResult<int> unused = dropped.takeResult();
    }
    EXPECT_TRUE(dropped.cancelRequested());
    EXPECT_FALSE(dropped.settled());

    ManualPromise<void> failed;
    const AsyncResult<void> failedResult = failed.takeResult();
    EXPECT_TRUE(failed.fail(Error{ErrorCode::NotFound}));
    EXPECT_TRUE(failed.settled());
    EXPECT_FALSE(failed.cancelRequested());
    EXPECT_TRUE(failedResult.isReady());

    ManualPromise<void> succeeded;
    const AsyncResult<void> succeededResult = succeeded.takeResult();
    EXPECT_TRUE(succeeded.complete());
    EXPECT_TRUE(succeededResult.isReady());
}

} // namespace
