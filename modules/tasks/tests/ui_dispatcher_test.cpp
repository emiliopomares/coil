#include <coil/tasks/testing/manual_ui_dispatcher.hpp>
#include <coil/tasks/ui_dispatcher.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

using coil::tasks::UiTask;
using coil::tasks::testing::ManualUiDispatcher;

TEST(UiTask, AC1_HoldsMoveOnlyCallables) {
    int result = 0;
    auto owned = std::make_unique<int>(41);
    UiTask task{[&result, owned = std::move(owned)] { result = *owned + 1; }};
    EXPECT_TRUE(static_cast<bool>(task));
    UiTask moved = std::move(task);
    ASSERT_TRUE(static_cast<bool>(moved));
    moved();
    EXPECT_EQ(result, 42);

    EXPECT_FALSE(static_cast<bool>(UiTask{}));

    const auto witness = std::make_shared<int>(0);
    {
        const UiTask unrun{[witness] { ++*witness; }};
        EXPECT_EQ(witness.use_count(), 2);
    }
    EXPECT_EQ(witness.use_count(), 1) << "destroying the task kept its captures";
    EXPECT_EQ(*witness, 0);
}

TEST(ManualUiDispatcher, AC2_RunsInOrderOnDemand) {
    ManualUiDispatcher ui;
    std::vector<int> order;
    ui.post(UiTask{[&order] { order.push_back(1); }});
    ui.post(UiTask{[&order, &ui] {
        order.push_back(2);
        ui.post(UiTask{[&order] { order.push_back(4); }});
    }});
    ui.post(UiTask{[&order] { order.push_back(3); }});
    EXPECT_TRUE(order.empty()) << "post ran a task";
    EXPECT_EQ(ui.pending(), 3U);

    EXPECT_EQ(ui.runPending(), 3U);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3})) << "a task posted while draining ran too early";
    EXPECT_EQ(ui.pending(), 1U);

    EXPECT_EQ(ui.runPending(), 1U);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(ui.pending(), 0U);
    EXPECT_EQ(ui.runPending(), 0U);
}

TEST(ManualUiDispatcher, AC3_DestructionDropsTasks) {
    const auto witness = std::make_shared<int>(0);
    {
        ManualUiDispatcher ui;
        // Control: a drained task does run, so "not run" below is a drop, not a dead dispatcher.
        ui.post(UiTask{[witness] { ++*witness; }});
        ASSERT_EQ(ui.runPending(), 1U);
        ASSERT_EQ(*witness, 1);

        ui.post(UiTask{[witness] { ++*witness; }});
        ui.post(UiTask{[witness] { ++*witness; }});
        EXPECT_EQ(witness.use_count(), 3);
    }
    EXPECT_EQ(witness.use_count(), 1) << "pending tasks were not destroyed";
    EXPECT_EQ(*witness, 1) << "a pending task ran on destruction";
}

} // namespace
