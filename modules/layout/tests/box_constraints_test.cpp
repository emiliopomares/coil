#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>

#include <gtest/gtest.h>

#include <format>
#include <limits>
#include <ostream>
#include <string_view>

namespace coil::foundation {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, Size size) {
    return os << "Size{" << size.width << ", " << size.height << "}";
}

} // namespace
} // namespace coil::foundation

namespace coil::layout {
inline namespace {

std::ostream& operator<<(std::ostream& os, const BoxConstraints& constraints) {
    return os << "BoxConstraints{w " << constraints.minWidth() << ".." << constraints.maxWidth()
              << ", h " << constraints.minHeight() << ".." << constraints.maxHeight() << "}";
}

} // namespace
} // namespace coil::layout

namespace {

using coil::foundation::ErrorCode;
using coil::foundation::Expected;
using coil::foundation::Insets;
using coil::foundation::Size;
using coil::layout::BoxConstraints;
using coil::layout::kUnbounded;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

/// Builds valid constraints; the test fails (and the default value is returned) if creation fails.
BoxConstraints make(float minWidth, float maxWidth, float minHeight, float maxHeight) {
    const Expected<BoxConstraints> result =
        BoxConstraints::create(minWidth, maxWidth, minHeight, maxHeight);
    EXPECT_TRUE(result.has_value()) << "create(" << minWidth << ", " << maxWidth << ", "
                                    << minHeight << ", " << maxHeight << ") failed";
    return result.value_or(BoxConstraints{});
}

void expectInvalid(const Expected<BoxConstraints>& result, std::string_view what) {
    ASSERT_FALSE(result.has_value()) << what << " should fail";
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument) << what;
}

void expectRanges(const BoxConstraints& constraints, float minWidth, float maxWidth,
                  float minHeight, float maxHeight) {
    EXPECT_EQ(constraints.minWidth(), minWidth);
    EXPECT_EQ(constraints.maxWidth(), maxWidth);
    EXPECT_EQ(constraints.minHeight(), minHeight);
    EXPECT_EQ(constraints.maxHeight(), maxHeight);
}

TEST(BoxConstraints, AC3_CreateValidates) {
    const Expected<BoxConstraints> bounded = BoxConstraints::create(10.0F, 100.0F, 20.0F, 200.0F);
    ASSERT_TRUE(bounded.has_value());
    expectRanges(*bounded, 10.0F, 100.0F, 20.0F, 200.0F);

    const Expected<BoxConstraints> open =
        BoxConstraints::create(5.0F, kUnbounded, 0.0F, kUnbounded);
    ASSERT_TRUE(open.has_value());
    expectRanges(*open, 5.0F, kUnbounded, 0.0F, kUnbounded);

    const Expected<BoxConstraints> equal = BoxConstraints::create(30.0F, 30.0F, 40.0F, 40.0F);
    ASSERT_TRUE(equal.has_value());
    expectRanges(*equal, 30.0F, 30.0F, 40.0F, 40.0F);

    expectInvalid(BoxConstraints::create(kNaN, 100.0F, 0.0F, 100.0F), "NaN min width");
    expectInvalid(BoxConstraints::create(0.0F, kNaN, 0.0F, 100.0F), "NaN max width");
    expectInvalid(BoxConstraints::create(0.0F, 100.0F, kNaN, 100.0F), "NaN min height");
    expectInvalid(BoxConstraints::create(0.0F, 100.0F, 0.0F, kNaN), "NaN max height");
    expectInvalid(BoxConstraints::create(-1.0F, 100.0F, 0.0F, 100.0F), "negative min width");
    expectInvalid(BoxConstraints::create(0.0F, 100.0F, -1.0F, 100.0F), "negative min height");
    expectInvalid(BoxConstraints::create(kUnbounded, kUnbounded, 0.0F, 100.0F),
                  "infinite min width");
    expectInvalid(BoxConstraints::create(0.0F, 100.0F, kUnbounded, kUnbounded),
                  "infinite min height");
    expectInvalid(BoxConstraints::create(101.0F, 100.0F, 0.0F, 100.0F), "min width > max width");
    expectInvalid(BoxConstraints::create(0.0F, 100.0F, 101.0F, 100.0F), "min height > max height");
    expectInvalid(BoxConstraints::create(0.0F, -1.0F, 0.0F, 100.0F), "negative max width");
}

TEST(BoxConstraints, AC4_Factories) {
    const Expected<BoxConstraints> tight =
        BoxConstraints::tight(Size{.width = 100.0F, .height = 50.0F});
    ASSERT_TRUE(tight.has_value());
    expectRanges(*tight, 100.0F, 100.0F, 50.0F, 50.0F);
    EXPECT_TRUE(tight->isTight());
    EXPECT_TRUE(tight->hasBoundedWidth());
    EXPECT_TRUE(tight->hasBoundedHeight());

    const Expected<BoxConstraints> loose =
        BoxConstraints::loose(Size{.width = 100.0F, .height = 50.0F});
    ASSERT_TRUE(loose.has_value());
    expectRanges(*loose, 0.0F, 100.0F, 0.0F, 50.0F);
    EXPECT_FALSE(loose->isTight());
    EXPECT_TRUE(loose->hasBoundedWidth());
    EXPECT_TRUE(loose->hasBoundedHeight());

    const BoxConstraints unbounded = BoxConstraints::unbounded();
    expectRanges(unbounded, 0.0F, kUnbounded, 0.0F, kUnbounded);
    EXPECT_EQ(unbounded, BoxConstraints{});
    EXPECT_EQ(unbounded, make(0.0F, kUnbounded, 0.0F, kUnbounded));
    EXPECT_FALSE(unbounded.isTight());
    EXPECT_FALSE(unbounded.hasBoundedWidth());
    EXPECT_FALSE(unbounded.hasBoundedHeight());

    const BoxConstraints halfOpen = make(0.0F, 100.0F, 0.0F, kUnbounded);
    EXPECT_TRUE(halfOpen.hasBoundedWidth());
    EXPECT_FALSE(halfOpen.hasBoundedHeight());

    const Expected<BoxConstraints> looseUnbounded =
        BoxConstraints::loose(Size{.width = kUnbounded, .height = kUnbounded});
    ASSERT_TRUE(looseUnbounded.has_value());
    EXPECT_EQ(*looseUnbounded, BoxConstraints::unbounded());

    expectInvalid(BoxConstraints::tight(Size{.width = -1.0F, .height = 50.0F}),
                  "tight, negative width");
    expectInvalid(BoxConstraints::tight(Size{.width = 100.0F, .height = -1.0F}),
                  "tight, negative height");
    expectInvalid(BoxConstraints::tight(Size{.width = kNaN, .height = 50.0F}), "tight, NaN width");
    expectInvalid(BoxConstraints::tight(Size{.width = 100.0F, .height = kNaN}),
                  "tight, NaN height");
    expectInvalid(BoxConstraints::tight(Size{.width = kUnbounded, .height = 50.0F}),
                  "tight, infinite width");
    expectInvalid(BoxConstraints::tight(Size{.width = 100.0F, .height = kUnbounded}),
                  "tight, infinite height");
    expectInvalid(BoxConstraints::loose(Size{.width = -1.0F, .height = 50.0F}),
                  "loose, negative width");
    expectInvalid(BoxConstraints::loose(Size{.width = 100.0F, .height = -1.0F}),
                  "loose, negative height");
    expectInvalid(BoxConstraints::loose(Size{.width = kNaN, .height = 50.0F}), "loose, NaN width");
    expectInvalid(BoxConstraints::loose(Size{.width = 100.0F, .height = kNaN}),
                  "loose, NaN height");
}

TEST(BoxConstraints, AC5_ConstrainClampsIntoRange) {
    const BoxConstraints constraints = make(10.0F, 100.0F, 20.0F, kUnbounded);

    EXPECT_EQ(constraints.constrain(Size{.width = 5.0F, .height = 5.0F}),
              (Size{.width = 10.0F, .height = 20.0F}));
    EXPECT_EQ(constraints.constrain(Size{.width = 50.0F, .height = 60.0F}),
              (Size{.width = 50.0F, .height = 60.0F}));
    EXPECT_EQ(constraints.constrain(Size{.width = 500.0F, .height = 1000.0F}),
              (Size{.width = 100.0F, .height = 1000.0F}));
    EXPECT_EQ(constraints.constrain(Size{.width = kNaN, .height = kNaN}),
              (Size{.width = 10.0F, .height = 20.0F}));

    EXPECT_TRUE(constraints.isSatisfiedBy(Size{.width = 10.0F, .height = 20.0F}));
    EXPECT_TRUE(constraints.isSatisfiedBy(Size{.width = 100.0F, .height = 1e9F}));
    EXPECT_FALSE(constraints.isSatisfiedBy(Size{.width = 9.0F, .height = 20.0F}));
    EXPECT_FALSE(constraints.isSatisfiedBy(Size{.width = 101.0F, .height = 20.0F}));
    EXPECT_FALSE(constraints.isSatisfiedBy(Size{.width = 50.0F, .height = 19.0F}));
    EXPECT_FALSE(constraints.isSatisfiedBy(Size{.width = kNaN, .height = 20.0F}));
}

TEST(BoxConstraints, AC6_DeflateSubtractsInsets) {
    const BoxConstraints constraints = make(30.0F, 100.0F, 0.0F, kUnbounded);

    expectRanges(
        constraints.deflate(Insets{.left = 5.0F, .top = 10.0F, .right = 15.0F, .bottom = 20.0F}),
        10.0F, 80.0F, 0.0F, kUnbounded);
    expectRanges(
        constraints.deflate(Insets{.left = 60.0F, .top = 0.0F, .right = 60.0F, .bottom = 0.0F}),
        0.0F, 0.0F, 0.0F, kUnbounded);
    expectRanges(
        constraints.deflate(Insets{.left = kNaN, .top = kNaN, .right = 10.0F, .bottom = 0.0F}),
        20.0F, 90.0F, 0.0F, kUnbounded);
    expectRanges(
        constraints.deflate(Insets{.left = -5.0F, .top = 0.0F, .right = -5.0F, .bottom = 0.0F}),
        40.0F, 110.0F, 0.0F, kUnbounded);
}

TEST(BoxConstraints, AC7_LoosenDropsMinimums) {
    expectRanges(make(30.0F, 100.0F, 40.0F, kUnbounded).loosen(), 0.0F, 100.0F, 0.0F, kUnbounded);
}

TEST(BoxConstraints, AC8_FormatsReadably) {
    EXPECT_EQ(std::format("{}", make(0.0F, 100.0F, 20.0F, kUnbounded)), "w 0..100, h 20..inf");
    EXPECT_EQ(std::format("{}", make(100.0F, 100.0F, 50.0F, 50.0F)), "w 100..100, h 50..50");
}

} // namespace
