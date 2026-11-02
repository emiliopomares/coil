#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>

#include <gtest/gtest.h>

#include <ostream>

namespace coil::render {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, const DrawList& list) {
    return os << "DrawList{" << list.commands().size() << " commands}";
}

} // namespace
} // namespace coil::render

namespace {

using coil::foundation::Rect;
using coil::render::Color;
using coil::render::DrawList;
using coil::render::FillRect;
using coil::render::PopClip;

TEST(DrawList, AC8_IsPlainComparableData) {
    DrawList list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list, DrawList{});

    const FillRect fill{
        .rect = Rect{.origin = {.x = 1.0F, .y = 2.0F}, .size = {.width = 3.0F, .height = 4.0F}},
        .paint = Color{.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F}};
    list.append(fill);
    EXPECT_FALSE(list.empty());
    ASSERT_EQ(list.commands().size(), 1U);
    EXPECT_NE(list, DrawList{});

    const DrawList copy = list;
    EXPECT_EQ(copy, list);
    list.append(PopClip{});
    EXPECT_NE(copy, list);
    EXPECT_EQ(copy.commands().size(), 1U);

    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list, DrawList{});
}

} // namespace
