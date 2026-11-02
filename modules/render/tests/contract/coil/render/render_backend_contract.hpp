#pragma once

// Contract suite every RenderBackend implementation must pass (SPEC-0020).
// To run it for an implementation `Impl`, in a test source inside namespace coil::render:
//   template <> struct RenderBackendContractTraits<Impl> {
//       static std::unique_ptr<RenderBackend> make();
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, RenderBackendContract, ::testing::Types<Impl>);

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/painter.hpp>
#include <coil/render/render_backend.hpp>

#include <gtest/gtest.h>

#include <format>
#include <limits>
#include <memory>
#include <ostream>

namespace coil::render {

/// Specialize for each implementation under contract; `make()` returns a fresh backend.
template <typename Impl>
struct RenderBackendContractTraits;

// Readable failure messages for the port's value types (test-only; GoogleTest uses operator<<).
inline std::ostream& operator<<(std::ostream& os, RenderTargetId id) {
    return os << "RenderTargetId{" << id.value << "}";
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

/// A description with the given logical size and scale.
inline RenderTargetDesc targetDesc(float width, float height, float scale = 1.0F) {
    return RenderTargetDesc{.size = {.width = width, .height = height}, .scale = scale};
}

/// A frame with one filled rect.
inline DrawList oneRectFrame() {
    DrawList frame;
    Painter painter{frame};
    painter.fillRect(Rect{.origin = {}, .size = {.width = 10.0F, .height = 10.0F}},
                     Color{.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F});
    return frame;
}

template <typename Impl>
class RenderBackendContract : public ::testing::Test {
protected:
    RenderBackend& backend() { return *m_backend; }

    /// Creates a target that the test expects to succeed; returns RenderTargetId{} on failure.
    RenderTargetId create(const RenderTargetDesc& desc) {
        const Expected<RenderTargetId> result = m_backend->createTarget(desc);
        EXPECT_TRUE(succeeds(result));
        return result.value_or(RenderTargetId{});
    }

private:
    std::unique_ptr<RenderBackend> m_backend = RenderBackendContractTraits<Impl>::make();
};

TYPED_TEST_SUITE_P(RenderBackendContract);

TYPED_TEST_P(RenderBackendContract, AC1_CreatesDistinctTargets) {
    const RenderTargetId a = this->create(targetDesc(800.0F, 600.0F));
    const RenderTargetId b = this->create(targetDesc(0.0F, 0.0F));
    const RenderTargetId c = this->create(targetDesc(320.0F, 240.0F, 2.0F));
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

TYPED_TEST_P(RenderBackendContract, AC2_RejectsInvalidDescriptions) {
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    RenderBackend& backend = this->backend();

    EXPECT_TRUE(
        failsWith(backend.createTarget(targetDesc(-1.0F, 10.0F)), ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        failsWith(backend.createTarget(targetDesc(10.0F, kNaN)), ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        failsWith(backend.createTarget(targetDesc(kInfinity, 10.0F)), ErrorCode::InvalidArgument));
    for (const float scale : {0.0F, -1.0F, kNaN, kInfinity}) {
        EXPECT_TRUE(failsWith(backend.createTarget(targetDesc(10.0F, 10.0F, scale)),
                              ErrorCode::InvalidArgument))
            << "scale " << scale;
    }
}

TYPED_TEST_P(RenderBackendContract, AC3_UnknownTargetsAreNotFound) {
    RenderBackend& backend = this->backend();
    const RenderTargetId destroyed = this->create(targetDesc(100.0F, 100.0F));
    ASSERT_TRUE(succeeds(backend.destroyTarget(destroyed)));
    const DrawList frame = oneRectFrame();

    for (const RenderTargetId id : {RenderTargetId{.value = 999'999}, destroyed}) {
        EXPECT_TRUE(failsWith(backend.destroyTarget(id), ErrorCode::NotFound)) << id;
        EXPECT_TRUE(
            failsWith(backend.resizeTarget(id, targetDesc(10.0F, 10.0F)), ErrorCode::NotFound))
            << id;
        EXPECT_TRUE(failsWith(backend.submit(id, frame), ErrorCode::NotFound)) << id;
        EXPECT_TRUE(failsWith(backend.present(id), ErrorCode::NotFound)) << id;
    }
}

TYPED_TEST_P(RenderBackendContract, AC4_TargetLifecycle) {
    RenderBackend& backend = this->backend();
    const RenderTargetId id = this->create(targetDesc(100.0F, 50.0F));

    EXPECT_TRUE(succeeds(backend.present(id)));
    EXPECT_TRUE(succeeds(backend.submit(id, DrawList{})));
    EXPECT_TRUE(succeeds(backend.submit(id, oneRectFrame())));
    EXPECT_TRUE(succeeds(backend.present(id)));
    EXPECT_TRUE(succeeds(backend.resizeTarget(id, targetDesc(200.0F, 100.0F, 2.0F))));
    EXPECT_TRUE(
        failsWith(backend.resizeTarget(id, targetDesc(-5.0F, 100.0F)), ErrorCode::InvalidArgument));
    EXPECT_TRUE(succeeds(backend.destroyTarget(id)));
}

TYPED_TEST_P(RenderBackendContract, AC5_TargetsAreIndependent) {
    RenderBackend& backend = this->backend();
    const RenderTargetId a = this->create(targetDesc(100.0F, 100.0F));
    const RenderTargetId b = this->create(targetDesc(100.0F, 100.0F));
    ASSERT_NE(a, b);

    ASSERT_TRUE(succeeds(backend.destroyTarget(a)));
    EXPECT_TRUE(succeeds(backend.submit(b, oneRectFrame())));
    EXPECT_TRUE(succeeds(backend.present(b)));
    EXPECT_TRUE(succeeds(backend.resizeTarget(b, targetDesc(50.0F, 50.0F))));
    EXPECT_TRUE(succeeds(backend.destroyTarget(b)));
}

REGISTER_TYPED_TEST_SUITE_P(RenderBackendContract, AC1_CreatesDistinctTargets,
                            AC2_RejectsInvalidDescriptions, AC3_UnknownTargetsAreNotFound,
                            AC4_TargetLifecycle, AC5_TargetsAreIndependent);

} // namespace coil::render
