#pragma once

// Contract suite every WindowSystem implementation must pass (SPEC-0003).
// To run it for an implementation `Impl`, in a test source inside namespace coil::platform:
//   template <> struct WindowSystemContractTraits<Impl> {
//       static std::unique_ptr<WindowSystem> make();
//       // Simulates the OS asking to close `id` (SPEC-0006).
//       static void requestClose(WindowSystem& system, WindowId id);
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, WindowSystemContract, ::testing::Types<Impl>);

#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <format>
#include <memory>
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

inline std::ostream& operator<<(std::ostream& os, const WindowEvent& event) {
    if (const auto* close = std::get_if<CloseRequested>(&event)) {
        return os << "CloseRequested{" << close->window << "}";
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

    /// Simulates the OS asking to close `id`, in the implementation's own way (SPEC-0006).
    void requestClose(WindowId id) {
        WindowSystemContractTraits<Impl>::requestClose(*m_system, id);
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

REGISTER_TYPED_TEST_SUITE_P(WindowSystemContract, AC1_CreateReturnsValidIdAndListsWindow,
                            AC2_ReportsTitleAndSizeFromDesc, AC3_SetTitleChangesOnlyThatWindow,
                            AC4_RejectsNonPositiveSize, AC5_DestroyedWindowIsNotFound,
                            AC6_UnknownIdsAreNotFound, AC7_IdsAreNeverReused,
                            EV1_EventsEmptyWhenNothingHappened, EV2_CloseRequestIsDeliveredOnce,
                            EV3_EventsKeepOrder, EV4_CloseRequestDoesNotCloseWindow,
                            EV5_EventsForDestroyedWindowsAreDropped);

} // namespace coil::platform
