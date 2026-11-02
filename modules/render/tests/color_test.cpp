#include <coil/render/color.hpp>

#include <gtest/gtest.h>

#include <ostream>

namespace coil::render {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, Color color) {
    return os << "Color{" << color.r << ", " << color.g << ", " << color.b << ", " << color.a
              << "}";
}

} // namespace
} // namespace coil::render

namespace {

using coil::render::Color;

TEST(Color, AC1_ConstructsAndCompares) {
    EXPECT_EQ(Color{}, (Color{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F}));

    const Color expected{.r = 1.0F, .g = 0.0F, .b = 51.0F / 255.0F, .a = 102.0F / 255.0F};
    EXPECT_EQ(Color::fromRgba8(255, 0, 51, 102), expected);
    EXPECT_EQ(Color::fromHex(0xFF003366U), expected);
    EXPECT_EQ(Color::fromRgba8(10, 20, 30), Color::fromHex(0x0A141EFFU));

    EXPECT_NE(expected, (Color{.r = 1.0F, .g = 0.0F, .b = 51.0F / 255.0F, .a = 1.0F}));
}

} // namespace
