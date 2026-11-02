#pragma once

// Contract suite every WindowSystem implementation must pass (SPEC-0003).
// To run it for an implementation `Impl`, in a test source inside namespace coil::platform:
//   template <> struct WindowSystemContractTraits<Impl> {
//       static std::unique_ptr<WindowSystem> make();
//       // Simulates the OS asking to close `id` (SPEC-0006).
//       static void requestClose(WindowSystem& system, WindowId id);
//       // Simulates the user resizing `id` to `size`, outside the port (SPEC-0010).
//       static void userResize(WindowSystem& system, WindowId id, WindowSize size);
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, WindowSystemContract, ::testing::Types<Impl>);

#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace coil::platform {

/// Specialize for each implementation under contract; `make()` returns a fresh, empty system.
template <typename Impl>
struct WindowSystemContractTraits;

// Readable failure messages for the port's value types (test-only; GoogleTest uses operator<<).
inline std::ostream& operator<<(std::ostream& os, WindowId id) {
    return os << "WindowId{" << id.value << "}";
}

inline std::ostream& operator<<(std::ostream& os, WindowSize size) {
    return os << "WindowSize{" << size.width << ", " << size.height << "}";
}

inline std::ostream& operator<<(std::ostream& os, const std::optional<WindowSize>& bound) {
    if (bound.has_value()) {
        return os << *bound;
    }
    return os << "none";
}

inline std::ostream& operator<<(std::ostream& os, const WindowSizeLimits& limits) {
    return os << "WindowSizeLimits{min=" << limits.min << ", max=" << limits.max << "}";
}

inline std::ostream& operator<<(std::ostream& os, const WindowEvent& event) {
    if (const auto* close = std::get_if<CloseRequested>(&event)) {
        return os << "CloseRequested{" << close->window << "}";
    }
    if (const auto* resized = std::get_if<Resized>(&event)) {
        return os << "Resized{" << resized->window << ", " << resized->size << "}";
    }
    if (const auto* scaleChanged = std::get_if<ScaleChanged>(&event)) {
        return os << "ScaleChanged{" << scaleChanged->window << ", " << scaleChanged->scale << "}";
    }
    return os << "WindowEvent{unknown}";
}

template <typename T>
::testing::AssertionResult succeeds(const Expected<T>& result) {
    if (result.has_value()) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "failed with " << std::format("{}", result.error());
}

template <typename T>
::testing::AssertionResult failsWith(const Expected<T>& result, ErrorCode expected) {
    if (result.has_value()) {
        return ::testing::AssertionFailure()
               << "succeeded, expected failure " << std::format("{}", expected);
    }
    if (result.error().code() != expected) {
        return ::testing::AssertionFailure() << "failed with " << std::format("{}", result.error())
                                             << ", expected " << std::format("{}", expected);
    }
    return ::testing::AssertionSuccess();
}

template <typename Impl>
class WindowSystemContract : public ::testing::Test {
protected:
    WindowSystem& system() { return *m_system; }

    /// Creates a window that the test expects to succeed; returns WindowId{} on failure.
    WindowId create(std::string title, WindowSize size) {
        const Expected<WindowId> result =
            m_system->createWindow(WindowDesc{.title = std::move(title), .size = size});
        EXPECT_TRUE(succeeds(result));
        return result.value_or(WindowId{});
    }

    /// Creates a window from a full description; returns WindowId{} on failure.
    WindowId createWith(const WindowDesc& desc) {
        const Expected<WindowId> result = m_system->createWindow(desc);
        EXPECT_TRUE(succeeds(result));
        return result.value_or(WindowId{});
    }

    /// Size limits of a window; empty limits (and a test failure) if the query fails.
    WindowSizeLimits limitsOf(WindowId id) {
        const Expected<WindowSizeLimits> result = m_system->sizeLimits(id);
        EXPECT_TRUE(succeeds(result));
        return result.value_or(WindowSizeLimits{});
    }

    /// Simulates the OS asking to close `id`, in the implementation's own way (SPEC-0006).
    void requestClose(WindowId id) {
        WindowSystemContractTraits<Impl>::requestClose(*m_system, id);
    }

    /// Simulates the user resizing `id`, in the implementation's own way (SPEC-0010).
    void userResize(WindowId id, WindowSize size) {
        WindowSystemContractTraits<Impl>::userResize(*m_system, id, size);
    }

    /// Size of a window; 0x0 (and a test failure) if the query fails.
    WindowSize sizeOf(WindowId id) {
        const Expected<WindowSize> result = m_system->size(id);
        EXPECT_TRUE(succeeds(result));
        return result.value_or(WindowSize{});
    }

    /// Title of a window, or a marker describing the error.
    std::string titleOf(WindowId id) {
        const Expected<std::string> result = m_system->title(id);
        return result.has_value() ? *result : std::format("<error: {}>", result.error());
    }

private:
    std::unique_ptr<WindowSystem> m_system = WindowSystemContractTraits<Impl>::make();
};

TYPED_TEST_SUITE_P(WindowSystemContract);

TYPED_TEST_P(WindowSystemContract, AC1_CreateReturnsValidIdAndListsWindow) {
    WindowSystem& system = this->system();
    const Expected<WindowId> id =
        system.createWindow(WindowDesc{.title = "Coil", .size = {.width = 800, .height = 600}});
    ASSERT_TRUE(succeeds(id));
    EXPECT_NE(id->value, 0U);
    EXPECT_EQ(system.windows(), std::vector<WindowId>{*id});
}

TYPED_TEST_P(WindowSystemContract, AC2_ReportsTitleAndSizeFromDesc) {
    const WindowSystem& system = this->system();
    const WindowId plain = this->create("Coil", {.width = 800, .height = 600});
    const WindowId unicode = this->create("Coïl — 窓", {.width = 320, .height = 200});

    EXPECT_EQ(this->titleOf(plain), "Coil");
    EXPECT_EQ(this->titleOf(unicode), "Coïl — 窓");

    const Expected<WindowSize> plainSize = system.size(plain);
    ASSERT_TRUE(succeeds(plainSize));
    EXPECT_EQ(*plainSize, (WindowSize{.width = 800, .height = 600}));
}

TYPED_TEST_P(WindowSystemContract, AC3_SetTitleChangesOnlyThatWindow) {
    WindowSystem& system = this->system();
    const WindowId first = this->create("First", {.width = 100, .height = 100});
    const WindowId second = this->create("Second", {.width = 100, .height = 100});

    ASSERT_TRUE(succeeds(system.setTitle(first, "Renamed")));
    EXPECT_EQ(this->titleOf(first), "Renamed");
    EXPECT_EQ(this->titleOf(second), "Second");

    ASSERT_TRUE(succeeds(system.setTitle(first, "")));
    EXPECT_EQ(this->titleOf(first), "");
}

TYPED_TEST_P(WindowSystemContract, AC4_RejectsNonPositiveSize) {
    WindowSystem& system = this->system();
    const std::array<WindowSize, 4> invalidSizes{{
        {.width = 0, .height = 600},
        {.width = 800, .height = 0},
        {.width = -1, .height = 600},
        {.width = 800, .height = -1},
    }};
    for (const WindowSize size : invalidSizes) {
        EXPECT_TRUE(failsWith(system.createWindow(WindowDesc{.title = "Bad", .size = size}),
                              ErrorCode::InvalidArgument))
            << ::testing::PrintToString(size);
    }
    EXPECT_TRUE(system.windows().empty());
}

TYPED_TEST_P(WindowSystemContract, AC5_DestroyedWindowIsNotFound) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowId b = this->create("B", {.width = 100, .height = 100});

    ASSERT_TRUE(succeeds(system.destroyWindow(a)));
    EXPECT_EQ(system.windows(), std::vector<WindowId>{b});

    EXPECT_TRUE(failsWith(system.destroyWindow(a), ErrorCode::NotFound));
    EXPECT_TRUE(failsWith(system.title(a), ErrorCode::NotFound));
    EXPECT_TRUE(failsWith(system.setTitle(a, "x"), ErrorCode::NotFound));
    EXPECT_TRUE(failsWith(system.size(a), ErrorCode::NotFound));

    EXPECT_EQ(this->titleOf(b), "B");
}

TYPED_TEST_P(WindowSystemContract, AC6_UnknownIdsAreNotFound) {
    WindowSystem& system = this->system();
    for (const WindowId id : {WindowId{}, WindowId{.value = 999}}) {
        EXPECT_TRUE(failsWith(system.destroyWindow(id), ErrorCode::NotFound)) << id.value;
        EXPECT_TRUE(failsWith(system.title(id), ErrorCode::NotFound)) << id.value;
        EXPECT_TRUE(failsWith(system.setTitle(id, "x"), ErrorCode::NotFound)) << id.value;
        EXPECT_TRUE(failsWith(system.size(id), ErrorCode::NotFound)) << id.value;
    }
}

TYPED_TEST_P(WindowSystemContract, AC7_IdsAreNeverReused) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    ASSERT_TRUE(succeeds(system.destroyWindow(a)));
    const WindowId b = this->create("B", {.width = 100, .height = 100});
    EXPECT_NE(a, b);

    std::set<std::uint64_t> seen{a.value, b.value};
    constexpr int kCycles = 100;
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        const WindowId id = this->create("cycle", {.width = 10, .height = 10});
        EXPECT_TRUE(seen.insert(id.value).second)
            << "id " << id.value << " reused in cycle " << cycle;
        ASSERT_TRUE(succeeds(system.destroyWindow(id)));
    }
}

// SPEC-0006: window events (EV<n> = SPEC-0006 AC-<n>).

TYPED_TEST_P(WindowSystemContract, EV1_EventsEmptyWhenNothingHappened) {
    WindowSystem& system = this->system();
    this->create("A", {.width = 100, .height = 100});
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
}

TYPED_TEST_P(WindowSystemContract, EV2_CloseRequestIsDeliveredOnce) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    this->requestClose(a);
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{CloseRequested{.window = a}});
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
}

TYPED_TEST_P(WindowSystemContract, EV3_EventsKeepOrder) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowId b = this->create("B", {.width = 100, .height = 100});
    this->requestClose(b);
    this->requestClose(a);
    EXPECT_EQ(system.pollEvents(),
              (std::vector<WindowEvent>{CloseRequested{.window = b}, CloseRequested{.window = a}}));
}

TYPED_TEST_P(WindowSystemContract, EV4_CloseRequestDoesNotCloseWindow) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    this->requestClose(a);
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{CloseRequested{.window = a}});
    EXPECT_EQ(system.windows(), std::vector<WindowId>{a});
    EXPECT_EQ(this->titleOf(a), "A");
}

TYPED_TEST_P(WindowSystemContract, EV5_EventsForDestroyedWindowsAreDropped) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowId b = this->create("B", {.width = 100, .height = 100});
    this->requestClose(a);
    this->requestClose(b);
    ASSERT_TRUE(succeeds(system.destroyWindow(a)));
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{CloseRequested{.window = b}});
}

// SPEC-0009: waiting for events (WE<n> = SPEC-0009 AC-<n>).

TYPED_TEST_P(WindowSystemContract, WE1_WaitReturnsPendingEvents) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    this->requestClose(a);
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              std::vector<WindowEvent>{CloseRequested{.window = a}});
}

TYPED_TEST_P(WindowSystemContract, WE2_WaitTimesOutEmpty) {
    WindowSystem& system = this->system();
    this->create("A", {.width = 100, .height = 100});
    EXPECT_EQ(system.waitEvents(std::chrono::milliseconds{10}), std::vector<WindowEvent>{});
    EXPECT_EQ(system.waitEvents(std::chrono::milliseconds{0}), std::vector<WindowEvent>{});
}

TYPED_TEST_P(WindowSystemContract, WE3_WaitedEventsAreDeliveredOnce) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    this->requestClose(a);
    ASSERT_EQ(system.waitEvents(std::chrono::seconds{1}),
              std::vector<WindowEvent>{CloseRequested{.window = a}});
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
}

TYPED_TEST_P(WindowSystemContract, WE4_NegativeTimeoutNeverBlocks) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    EXPECT_EQ(system.waitEvents(std::chrono::milliseconds{-1}), std::vector<WindowEvent>{});
    this->requestClose(a);
    EXPECT_EQ(system.waitEvents(std::chrono::milliseconds{-1}),
              std::vector<WindowEvent>{CloseRequested{.window = a}});
}

// SPEC-0010: setSize and the Resized event (RS<n> = SPEC-0010 AC-<n>).

TYPED_TEST_P(WindowSystemContract, RS1_SetSizeIsReportedAsResized) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowSize newSize{.width = 200, .height = 150};
    ASSERT_TRUE(succeeds(system.setSize(a, newSize)));
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = newSize}}));
    EXPECT_EQ(this->sizeOf(a), newSize);
}

TYPED_TEST_P(WindowSystemContract, RS2_SetSameSizeProducesNoEvent) {
    WindowSystem& system = this->system();
    const WindowSize initial{.width = 100, .height = 100};
    const WindowId a = this->create("A", initial);
    EXPECT_TRUE(succeeds(system.setSize(a, initial)));
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
    EXPECT_EQ(this->sizeOf(a), initial);
}

TYPED_TEST_P(WindowSystemContract, RS3_SetSizeRejectsBadInputWithoutEvents) {
    WindowSystem& system = this->system();
    const WindowSize initial{.width = 100, .height = 100};
    const WindowId a = this->create("A", initial);
    const WindowId destroyed = this->create("B", initial);
    ASSERT_TRUE(succeeds(system.destroyWindow(destroyed)));

    const std::array<WindowSize, 4> invalidSizes{{
        {.width = 0, .height = 600},
        {.width = 800, .height = 0},
        {.width = -1, .height = 600},
        {.width = 800, .height = -1},
    }};
    for (const WindowSize size : invalidSizes) {
        EXPECT_TRUE(failsWith(system.setSize(a, size), ErrorCode::InvalidArgument))
            << ::testing::PrintToString(size);
    }
    const WindowSize valid{.width = 200, .height = 150};
    EXPECT_TRUE(failsWith(system.setSize(destroyed, valid), ErrorCode::NotFound));
    EXPECT_TRUE(failsWith(system.setSize(WindowId{.value = 999}, valid), ErrorCode::NotFound));

    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
    EXPECT_EQ(this->sizeOf(a), initial);
}

TYPED_TEST_P(WindowSystemContract, RS4_UserResizeIsReportedAsResized) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowSize newSize{.width = 320, .height = 240};
    this->userResize(a, newSize);
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = newSize}}));
    EXPECT_EQ(this->sizeOf(a), newSize);
}

TYPED_TEST_P(WindowSystemContract, RS5_PendingResizesAreCoalesced) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowSize first{.width = 200, .height = 150};
    const WindowSize second{.width = 300, .height = 200};
    ASSERT_TRUE(succeeds(system.setSize(a, first)));
    ASSERT_TRUE(succeeds(system.setSize(a, second)));
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = second}}));
    EXPECT_EQ(this->sizeOf(a), second);
}

TYPED_TEST_P(WindowSystemContract, RS6_ResizesOfDestroyedWindowsAreDropped) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 100});
    const WindowId b = this->create("B", {.width = 100, .height = 100});
    const WindowSize newSize{.width = 200, .height = 150};
    ASSERT_TRUE(succeeds(system.setSize(a, newSize)));
    ASSERT_TRUE(succeeds(system.setSize(b, newSize)));
    ASSERT_TRUE(succeeds(system.destroyWindow(a)));
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = b, .size = newSize}}));
}

// SPEC-0011: resizable windows and size limits (SL<n> = SPEC-0011 AC-<n>).

TYPED_TEST_P(WindowSystemContract, SL1_ResizableIsSetAtCreation) {
    const WindowSystem& system = this->system();
    const WindowId fixed = this->create("Fixed", {.width = 100, .height = 100});
    const WindowId resizable = this->createWith(
        {.title = "Resizable", .size = {.width = 100, .height = 100}, .resizable = true});

    const Expected<bool> resizableFlag = system.resizable(resizable);
    ASSERT_TRUE(succeeds(resizableFlag));
    EXPECT_TRUE(*resizableFlag);
    const Expected<bool> fixedFlag = system.resizable(fixed);
    ASSERT_TRUE(succeeds(fixedFlag));
    EXPECT_FALSE(*fixedFlag);
}

TYPED_TEST_P(WindowSystemContract, SL2_LimitsAreSetAtCreation) {
    const WindowSizeLimits limits{.min = WindowSize{.width = 200, .height = 100},
                                  .max = WindowSize{.width = 800, .height = 600}};
    const WindowId limited = this->createWith(
        {.title = "Limited", .size = {.width = 400, .height = 300}, .limits = limits});
    const WindowId unlimited = this->create("Unlimited", {.width = 400, .height = 300});

    EXPECT_EQ(this->limitsOf(limited), limits);
    EXPECT_EQ(this->limitsOf(unlimited), WindowSizeLimits{});
}

TYPED_TEST_P(WindowSystemContract, SL3_CreateRejectsInvalidLimitsAndSizes) {
    WindowSystem& system = this->system();
    const WindowSize size{.width = 400, .height = 300};
    const WindowSize min{.width = 200, .height = 100};
    const WindowSize max{.width = 800, .height = 600};
    const std::array<WindowDesc, 6> invalid{{
        {.title = "MinWiderThanMax",
         .size = size,
         .limits = {.min = WindowSize{.width = 900, .height = 100}, .max = max}},
        {.title = "MinTallerThanMax",
         .size = size,
         .limits = {.min = WindowSize{.width = 200, .height = 700}, .max = max}},
        {.title = "ZeroMin",
         .size = size,
         .limits = {.min = WindowSize{.width = 0, .height = 100}}},
        {.title = "NegativeMax",
         .size = size,
         .limits = {.max = WindowSize{.width = 800, .height = -1}}},
        {.title = "BelowMin", .size = {.width = 100, .height = 300}, .limits = {.min = min}},
        {.title = "AboveMax", .size = {.width = 900, .height = 300}, .limits = {.max = max}},
    }};
    for (const WindowDesc& desc : invalid) {
        EXPECT_TRUE(failsWith(system.createWindow(desc), ErrorCode::InvalidArgument)) << desc.title;
    }
    EXPECT_TRUE(system.windows().empty());
}

TYPED_TEST_P(WindowSystemContract, SL4_SetSizeIsClampedToLimits) {
    WindowSystem& system = this->system();
    const WindowSizeLimits limits{.min = WindowSize{.width = 200, .height = 100},
                                  .max = WindowSize{.width = 800, .height = 600}};
    const WindowId a =
        this->createWith({.title = "A", .size = {.width = 400, .height = 300}, .limits = limits});
    const WindowSize clamped{.width = 800, .height = 100};

    ASSERT_TRUE(succeeds(system.setSize(a, {.width = 1000, .height = 50})));
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = clamped}}));
    EXPECT_EQ(this->sizeOf(a), clamped);

    EXPECT_TRUE(succeeds(system.setSize(a, clamped)));
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
}

TYPED_TEST_P(WindowSystemContract, SL5_NewLimitsResizeTheWindow) {
    WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 400, .height = 300});

    const WindowSizeLimits maxOnly{.max = WindowSize{.width = 300, .height = 200}};
    ASSERT_TRUE(succeeds(system.setSizeLimits(a, maxOnly)));
    EXPECT_EQ(
        system.waitEvents(std::chrono::seconds{1}),
        (std::vector<WindowEvent>{Resized{.window = a, .size = {.width = 300, .height = 200}}}));
    EXPECT_EQ(this->limitsOf(a), maxOnly);

    const WindowSizeLimits minOnly{.min = WindowSize{.width = 500, .height = 400}};
    ASSERT_TRUE(succeeds(system.setSizeLimits(a, minOnly)));
    EXPECT_EQ(
        system.waitEvents(std::chrono::seconds{1}),
        (std::vector<WindowEvent>{Resized{.window = a, .size = {.width = 500, .height = 400}}}));
    EXPECT_EQ(this->limitsOf(a), minOnly);
    EXPECT_EQ(this->sizeOf(a), (WindowSize{.width = 500, .height = 400}));
}

TYPED_TEST_P(WindowSystemContract, SL6_SetLimitsRejectsBadInputWithoutEvents) {
    WindowSystem& system = this->system();
    const WindowSize initial{.width = 400, .height = 300};
    const WindowId a = this->create("A", initial);
    const WindowId destroyed = this->create("B", initial);
    ASSERT_TRUE(succeeds(system.destroyWindow(destroyed)));

    const WindowSize max{.width = 800, .height = 600};
    const std::array<WindowSizeLimits, 4> invalid{{
        {.min = WindowSize{.width = 900, .height = 100}, .max = max},
        {.min = WindowSize{.width = 200, .height = 700}, .max = max},
        {.min = WindowSize{.width = 0, .height = 100}},
        {.max = WindowSize{.width = 800, .height = -1}},
    }};
    for (const WindowSizeLimits& limits : invalid) {
        EXPECT_TRUE(failsWith(system.setSizeLimits(a, limits), ErrorCode::InvalidArgument))
            << ::testing::PrintToString(limits);
    }
    EXPECT_EQ(system.pollEvents(), std::vector<WindowEvent>{});
    EXPECT_EQ(this->limitsOf(a), WindowSizeLimits{});
    EXPECT_EQ(this->sizeOf(a), initial);

    for (const WindowId gone : {destroyed, WindowId{.value = 999}}) {
        EXPECT_TRUE(failsWith(system.resizable(gone), ErrorCode::NotFound));
        EXPECT_TRUE(failsWith(system.sizeLimits(gone), ErrorCode::NotFound));
        EXPECT_TRUE(failsWith(system.setSizeLimits(gone, {}), ErrorCode::NotFound));
    }
}

TYPED_TEST_P(WindowSystemContract, SL7_UserResizeIsClampedToLimits) {
    WindowSystem& system = this->system();
    const WindowId a =
        this->createWith({.title = "A",
                          .size = {.width = 200, .height = 100},
                          .resizable = true,
                          .limits = {.max = WindowSize{.width = 300, .height = 200}}});
    const WindowSize requested{.width = 640, .height = 480};

    this->userResize(a, requested);
    EXPECT_EQ(
        system.waitEvents(std::chrono::seconds{1}),
        (std::vector<WindowEvent>{Resized{.window = a, .size = {.width = 300, .height = 200}}}));

    ASSERT_TRUE(succeeds(system.setSizeLimits(a, {})));
    this->userResize(a, requested);
    EXPECT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = requested}}));
}

// SPEC-0013: pixel size and display scale (PX<n> = SPEC-0013 AC-<n>).

TYPED_TEST_P(WindowSystemContract, PX1_PixelSizeAndScaleArePositive) {
    const WindowSystem& system = this->system();
    const WindowId a = this->create("A", {.width = 100, .height = 80});

    const Expected<float> scale = system.displayScale(a);
    ASSERT_TRUE(succeeds(scale));
    EXPECT_GT(*scale, 0.0F);
    const Expected<WindowSize> pixels = system.pixelSize(a);
    ASSERT_TRUE(succeeds(pixels));
    EXPECT_GE(pixels->width, 100);
    EXPECT_GE(pixels->height, 80);
}

TYPED_TEST_P(WindowSystemContract, PX2_PixelSizeFollowsResize) {
    WindowSystem& system = this->system();
    const WindowSize before{.width = 100, .height = 80};
    const WindowSize after{.width = 200, .height = 160};
    const WindowId a = this->create("A", before);
    const Expected<WindowSize> oldPixels = system.pixelSize(a);
    ASSERT_TRUE(succeeds(oldPixels));

    ASSERT_TRUE(succeeds(system.setSize(a, after)));
    ASSERT_EQ(system.waitEvents(std::chrono::seconds{1}),
              (std::vector<WindowEvent>{Resized{.window = a, .size = after}}));

    const Expected<WindowSize> newPixels = system.pixelSize(a);
    ASSERT_TRUE(succeeds(newPixels));
    EXPECT_GT(newPixels->width, 0);
    EXPECT_EQ(oldPixels->width * after.width, newPixels->width * before.width);
    EXPECT_EQ(oldPixels->height * after.height, newPixels->height * before.height);
}

TYPED_TEST_P(WindowSystemContract, PX3_UnknownIdsAreNotFound) {
    WindowSystem& system = this->system();
    const WindowId destroyed = this->create("A", {.width = 100, .height = 80});
    ASSERT_TRUE(succeeds(system.destroyWindow(destroyed)));
    for (const WindowId gone : {destroyed, WindowId{.value = 999}}) {
        EXPECT_TRUE(failsWith(system.pixelSize(gone), ErrorCode::NotFound));
        EXPECT_TRUE(failsWith(system.displayScale(gone), ErrorCode::NotFound));
    }
}

REGISTER_TYPED_TEST_SUITE_P(WindowSystemContract, AC1_CreateReturnsValidIdAndListsWindow,
                            AC2_ReportsTitleAndSizeFromDesc, AC3_SetTitleChangesOnlyThatWindow,
                            AC4_RejectsNonPositiveSize, AC5_DestroyedWindowIsNotFound,
                            AC6_UnknownIdsAreNotFound, AC7_IdsAreNeverReused,
                            EV1_EventsEmptyWhenNothingHappened, EV2_CloseRequestIsDeliveredOnce,
                            EV3_EventsKeepOrder, EV4_CloseRequestDoesNotCloseWindow,
                            EV5_EventsForDestroyedWindowsAreDropped, WE1_WaitReturnsPendingEvents,
                            WE2_WaitTimesOutEmpty, WE3_WaitedEventsAreDeliveredOnce,
                            WE4_NegativeTimeoutNeverBlocks, RS1_SetSizeIsReportedAsResized,
                            RS2_SetSameSizeProducesNoEvent, RS3_SetSizeRejectsBadInputWithoutEvents,
                            RS4_UserResizeIsReportedAsResized, RS5_PendingResizesAreCoalesced,
                            RS6_ResizesOfDestroyedWindowsAreDropped, SL1_ResizableIsSetAtCreation,
                            SL2_LimitsAreSetAtCreation, SL3_CreateRejectsInvalidLimitsAndSizes,
                            SL4_SetSizeIsClampedToLimits, SL5_NewLimitsResizeTheWindow,
                            SL6_SetLimitsRejectsBadInputWithoutEvents,
                            SL7_UserResizeIsClampedToLimits, PX1_PixelSizeAndScaleArePositive,
                            PX2_PixelSizeFollowsResize, PX3_UnknownIdsAreNotFound);

} // namespace coil::platform
