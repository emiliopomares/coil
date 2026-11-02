#include <coil/foundation/geometry.hpp>

#include <gtest/gtest.h>

#include <format>
#include <ostream>

namespace coil::foundation {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<). Members are printed directly,
// so the messages don't depend on the formatters under test.
std::ostream& operator<<(std::ostream& os, Size size) {
    return os << "Size{" << size.width << ", " << size.height << "}";
}

std::ostream& operator<<(std::ostream& os, Point point) {
    return os << "Point{" << point.x << ", " << point.y << "}";
}

std::ostream& operator<<(std::ostream& os, Rect rect) {
    return os << "Rect{" << rect.origin << ", " << rect.size << "}";
}

std::ostream& operator<<(std::ostream& os, Insets insets) {
    return os << "Insets{" << insets.left << ", " << insets.top << ", " << insets.right << ", "
              << insets.bottom << "}";
}

} // namespace
} // namespace coil::foundation

namespace {

using coil::foundation::Insets;
using coil::foundation::Point;
using coil::foundation::Rect;
using coil::foundation::Size;

TEST(Geometry, AC1_ValuesDefaultToZeroAndCompareByValue) {
    EXPECT_EQ(Size{}, (Size{.width = 0.0F, .height = 0.0F}));
    EXPECT_EQ(Point{}, (Point{.x = 0.0F, .y = 0.0F}));
    EXPECT_EQ(Rect{},
              (Rect{.origin = {.x = 0.0F, .y = 0.0F}, .size = {.width = 0.0F, .height = 0.0F}}));
    EXPECT_EQ(Insets{}, (Insets{.left = 0.0F, .top = 0.0F, .right = 0.0F, .bottom = 0.0F}));

    EXPECT_EQ((Size{.width = 1.0F, .height = 2.0F}), (Size{.width = 1.0F, .height = 2.0F}));
    EXPECT_NE((Size{.width = 1.0F, .height = 2.0F}), (Size{.width = 9.0F, .height = 2.0F}));
    EXPECT_NE((Size{.width = 1.0F, .height = 2.0F}), (Size{.width = 1.0F, .height = 9.0F}));

    EXPECT_EQ((Point{.x = 1.0F, .y = 2.0F}), (Point{.x = 1.0F, .y = 2.0F}));
    EXPECT_NE((Point{.x = 1.0F, .y = 2.0F}), (Point{.x = 9.0F, .y = 2.0F}));
    EXPECT_NE((Point{.x = 1.0F, .y = 2.0F}), (Point{.x = 1.0F, .y = 9.0F}));

    const Rect rect{.origin = {.x = 1.0F, .y = 2.0F}, .size = {.width = 3.0F, .height = 4.0F}};
    EXPECT_EQ(rect,
              (Rect{.origin = {.x = 1.0F, .y = 2.0F}, .size = {.width = 3.0F, .height = 4.0F}}));
    EXPECT_NE(rect,
              (Rect{.origin = {.x = 9.0F, .y = 2.0F}, .size = {.width = 3.0F, .height = 4.0F}}));
    EXPECT_NE(rect,
              (Rect{.origin = {.x = 1.0F, .y = 9.0F}, .size = {.width = 3.0F, .height = 4.0F}}));
    EXPECT_NE(rect,
              (Rect{.origin = {.x = 1.0F, .y = 2.0F}, .size = {.width = 9.0F, .height = 4.0F}}));
    EXPECT_NE(rect,
              (Rect{.origin = {.x = 1.0F, .y = 2.0F}, .size = {.width = 3.0F, .height = 9.0F}}));

    const Insets insets{.left = 1.0F, .top = 2.0F, .right = 3.0F, .bottom = 4.0F};
    EXPECT_EQ(insets, (Insets{.left = 1.0F, .top = 2.0F, .right = 3.0F, .bottom = 4.0F}));
    EXPECT_NE(insets, (Insets{.left = 9.0F, .top = 2.0F, .right = 3.0F, .bottom = 4.0F}));
    EXPECT_NE(insets, (Insets{.left = 1.0F, .top = 9.0F, .right = 3.0F, .bottom = 4.0F}));
    EXPECT_NE(insets, (Insets{.left = 1.0F, .top = 2.0F, .right = 9.0F, .bottom = 4.0F}));
    EXPECT_NE(insets, (Insets{.left = 1.0F, .top = 2.0F, .right = 3.0F, .bottom = 9.0F}));

    const Rect placed{.origin = {.x = 10.0F, .y = 20.0F},
                      .size = {.width = 100.0F, .height = 50.0F}};
    EXPECT_EQ(placed.left(), 10.0F);
    EXPECT_EQ(placed.top(), 20.0F);
    EXPECT_EQ(placed.right(), 110.0F);
    EXPECT_EQ(placed.bottom(), 70.0F);
    EXPECT_EQ(insets.horizontal(), 4.0F);
    EXPECT_EQ(insets.vertical(), 6.0F);
}

TEST(Geometry, AC2_FormatsReadably) {
    EXPECT_EQ(std::format("{}", Size{.width = 100.0F, .height = 50.0F}), "100x50");
    EXPECT_EQ(std::format("{}", Point{.x = 10.0F, .y = 20.5F}), "(10, 20.5)");
    EXPECT_EQ(std::format("{}", Rect{.origin = {.x = 10.0F, .y = 20.0F},
                                     .size = {.width = 100.0F, .height = 50.0F}}),
              "(10, 20) 100x50");
    EXPECT_EQ(std::format("{}", Insets{.left = 1.0F, .top = 2.0F, .right = 3.0F, .bottom = 4.0F}),
              "{1, 2, 3, 4}");
}

} // namespace
