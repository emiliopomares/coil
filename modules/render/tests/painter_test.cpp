#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/paint.hpp>
#include <coil/render/painter.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <ostream>
#include <string_view>
#include <vector>

namespace coil::foundation {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, Rect rect) {
    return os << "Rect{" << rect.origin.x << ", " << rect.origin.y << ", " << rect.size.width
              << ", " << rect.size.height << "}";
}

} // namespace
} // namespace coil::foundation

namespace coil::render {
inline namespace {

std::ostream& operator<<(std::ostream& os, Color color) {
    return os << "Color{" << color.r << ", " << color.g << ", " << color.b << ", " << color.a
              << "}";
}

std::ostream& operator<<(std::ostream& os, const LinearGradient& gradient) {
    return os << "LinearGradient{" << gradient.stops.size() << " stops}";
}

std::ostream& operator<<(std::ostream& os, const RadialGradient& gradient) {
    return os << "RadialGradient{r " << gradient.radius << ", " << gradient.stops.size()
              << " stops}";
}

std::ostream& operator<<(std::ostream& os, const SweepGradient& gradient) {
    return os << "SweepGradient{" << gradient.stops.size() << " stops}";
}

std::ostream& operator<<(std::ostream& os, CornerRadii radii) {
    return os << "CornerRadii{" << radii.topLeft << ", " << radii.topRight << ", "
              << radii.bottomRight << ", " << radii.bottomLeft << "}";
}

std::ostream& operator<<(std::ostream& os, const FillRect& command) {
    return os << "FillRect{" << command.rect << "}";
}

std::ostream& operator<<(std::ostream& os, const FillRoundedRect& command) {
    return os << "FillRoundedRect{" << command.rect << ", " << command.radii << "}";
}

std::ostream& operator<<(std::ostream& os, const StrokeBorder& command) {
    return os << "StrokeBorder{" << command.rect << ", " << command.radii << ", width "
              << command.width << "}";
}

std::ostream& operator<<(std::ostream& os, PushClip command) {
    return os << "PushClip{" << command.rect << "}";
}

std::ostream& operator<<(std::ostream& os, PopClip /*command*/) {
    return os << "PopClip{}";
}

} // namespace
} // namespace coil::render

namespace {

using coil::foundation::ErrorCode;
using coil::foundation::Expected;
using coil::foundation::Point;
using coil::foundation::Rect;
using coil::render::Color;
using coil::render::CornerRadii;
using coil::render::DrawCommand;
using coil::render::DrawList;
using coil::render::FillRect;
using coil::render::FillRoundedRect;
using coil::render::GradientStop;
using coil::render::LinearGradient;
using coil::render::Paint;
using coil::render::Painter;
using coil::render::PopClip;
using coil::render::PushClip;
using coil::render::RadialGradient;
using coil::render::StrokeBorder;
using coil::render::SweepGradient;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInfinity = std::numeric_limits<float>::infinity();

constexpr Color kRed{.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};
constexpr Color kGreen{.r = 0.0F, .g = 1.0F, .b = 0.0F, .a = 1.0F};
constexpr Color kBlue{.r = 0.0F, .g = 0.0F, .b = 1.0F, .a = 1.0F};

Rect rect(float x, float y, float width, float height) {
    return Rect{.origin = {.x = x, .y = y}, .size = {.width = width, .height = height}};
}

Point point(float x, float y) {
    return Point{.x = x, .y = y};
}

CornerRadii radii(float topLeft, float topRight, float bottomRight, float bottomLeft) {
    return CornerRadii{.topLeft = topLeft,
                       .topRight = topRight,
                       .bottomRight = bottomRight,
                       .bottomLeft = bottomLeft};
}

std::vector<DrawCommand> commandsOf(const DrawList& list) {
    return {list.commands().begin(), list.commands().end()};
}

std::vector<GradientStop> redToBlue() {
    return {GradientStop{.offset = 0.0F, .color = kRed},
            GradientStop{.offset = 1.0F, .color = kBlue}};
}

void expectInvalid(const Expected<Paint>& result, std::string_view what) {
    ASSERT_FALSE(result.has_value()) << what << " should fail";
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument) << what;
}

TEST(Painter, AC2_RecordsShapesInOrder) {
    DrawList list;
    {
        Painter painter{list};
        painter.fillRect(rect(0.0F, 0.0F, 10.0F, 10.0F), kRed);
        painter.fillRoundedRect(rect(10.0F, 0.0F, 40.0F, 20.0F), CornerRadii::all(4.0F), kBlue);
        painter.strokeBorder(rect(0.0F, 20.0F, 40.0F, 20.0F), CornerRadii::all(2.0F), 3.0F, kGreen);
    }

    const std::vector<DrawCommand> expected{
        FillRect{.rect = rect(0.0F, 0.0F, 10.0F, 10.0F), .paint = kRed},
        FillRoundedRect{.rect = rect(10.0F, 0.0F, 40.0F, 20.0F),
                        .radii = radii(4.0F, 4.0F, 4.0F, 4.0F),
                        .paint = kBlue},
        StrokeBorder{.rect = rect(0.0F, 20.0F, 40.0F, 20.0F),
                     .radii = radii(2.0F, 2.0F, 2.0F, 2.0F),
                     .width = 3.0F,
                     .paint = kGreen},
    };
    EXPECT_EQ(commandsOf(list), expected);
}

TEST(Painter, AC3_GradientsAreValidated) {
    const Expected<Paint> linear =
        coil::render::linearGradient(point(0.0F, 0.0F), point(10.0F, 0.0F), redToBlue());
    const Expected<Paint> radial =
        coil::render::radialGradient(point(5.0F, 5.0F), 5.0F, redToBlue());
    const Expected<Paint> sweep = coil::render::sweepGradient(point(5.0F, 5.0F), 0.5F, redToBlue());
    ASSERT_TRUE(linear.has_value());
    ASSERT_TRUE(radial.has_value());
    ASSERT_TRUE(sweep.has_value());
    EXPECT_EQ(*linear,
              (Paint{LinearGradient{
                  .start = point(0.0F, 0.0F), .end = point(10.0F, 0.0F), .stops = redToBlue()}}));
    EXPECT_EQ(
        *radial,
        (Paint{RadialGradient{.center = point(5.0F, 5.0F), .radius = 5.0F, .stops = redToBlue()}}));
    EXPECT_EQ(*sweep, (Paint{SweepGradient{
                          .center = point(5.0F, 5.0F), .startAngle = 0.5F, .stops = redToBlue()}}));

    DrawList list;
    {
        Painter painter{list};
        painter.fillRect(rect(0.0F, 0.0F, 10.0F, 10.0F), *linear);
    }
    EXPECT_EQ(commandsOf(list), (std::vector<DrawCommand>{FillRect{
                                    .rect = rect(0.0F, 0.0F, 10.0F, 10.0F), .paint = *linear}}));

    const auto stopsWith = [](float first, float second) {
        return std::vector<GradientStop>{GradientStop{.offset = first, .color = kRed},
                                         GradientStop{.offset = second, .color = kBlue}};
    };
    const Point origin = point(0.0F, 0.0F);
    const Point end = point(10.0F, 0.0F);
    expectInvalid(
        coil::render::linearGradient(origin, end, {GradientStop{.offset = 0.0F, .color = kRed}}),
        "one stop");
    expectInvalid(coil::render::linearGradient(origin, end, stopsWith(-0.1F, 1.0F)), "offset -0.1");
    expectInvalid(coil::render::linearGradient(origin, end, stopsWith(0.0F, 1.1F)), "offset 1.1");
    expectInvalid(coil::render::linearGradient(origin, end, stopsWith(kNaN, 1.0F)), "offset NaN");
    expectInvalid(coil::render::linearGradient(origin, end, stopsWith(0.8F, 0.2F)),
                  "decreasing offsets");
    expectInvalid(
        coil::render::linearGradient(origin, end,
                                     {GradientStop{.offset = 0.0F, .color = Color{.r = kNaN}},
                                      GradientStop{.offset = 1.0F, .color = kBlue}}),
        "NaN colour");
    expectInvalid(coil::render::linearGradient(point(kNaN, 0.0F), end, redToBlue()), "NaN point");
    expectInvalid(coil::render::radialGradient(origin, 0.0F, redToBlue()), "radius 0");
    expectInvalid(coil::render::radialGradient(origin, -1.0F, redToBlue()), "radius -1");
    expectInvalid(coil::render::radialGradient(origin, kNaN, redToBlue()), "radius NaN");
    expectInvalid(coil::render::sweepGradient(origin, kNaN, redToBlue()), "start angle NaN");
}

TEST(Painter, AC4_DegenerateRectsAreSkipped) {
    DrawList list;
    {
        Painter painter{list};
        for (const Rect bad :
             {rect(0.0F, 0.0F, 0.0F, 10.0F), rect(0.0F, 0.0F, 10.0F, -1.0F),
              rect(kNaN, 0.0F, 10.0F, 10.0F), rect(0.0F, 0.0F, kInfinity, 10.0F)}) {
            painter.fillRect(bad, kRed);
            painter.fillRoundedRect(bad, CornerRadii::all(2.0F), kRed);
            painter.strokeBorder(bad, CornerRadii::all(2.0F), 1.0F, kRed);
        }
        painter.fillRect(rect(1.0F, 2.0F, 3.0F, 4.0F), kGreen);
    }
    EXPECT_EQ(commandsOf(list), (std::vector<DrawCommand>{FillRect{
                                    .rect = rect(1.0F, 2.0F, 3.0F, 4.0F), .paint = kGreen}}));
}

TEST(Painter, AC5_CornerRadiiAreNormalized) {
    const Rect box = rect(0.0F, 0.0F, 40.0F, 20.0F);
    DrawList list;
    {
        Painter painter{list};
        painter.fillRoundedRect(box, CornerRadii::all(15.0F), kRed);
        painter.fillRoundedRect(box, radii(-3.0F, kNaN, 5.0F, 30.0F), kRed);
    }
    const std::vector<DrawCommand> expected{
        FillRoundedRect{.rect = box, .radii = radii(10.0F, 10.0F, 10.0F, 10.0F), .paint = kRed},
        FillRoundedRect{.rect = box, .radii = radii(0.0F, 0.0F, 5.0F, 10.0F), .paint = kRed},
    };
    EXPECT_EQ(commandsOf(list), expected);
}

TEST(Painter, AC6_BorderWidthIsNormalized) {
    const Rect box = rect(0.0F, 0.0F, 40.0F, 20.0F);
    DrawList list;
    {
        Painter painter{list};
        for (const float bad : {0.0F, -1.0F, kNaN, kInfinity}) {
            painter.strokeBorder(box, CornerRadii{}, bad, kRed);
        }
        painter.strokeBorder(box, CornerRadii{}, 15.0F, kRed);
    }
    EXPECT_EQ(commandsOf(list),
              (std::vector<DrawCommand>{StrokeBorder{
                  .rect = box, .radii = CornerRadii{}, .width = 10.0F, .paint = kRed}}));
}

TEST(Painter, AC7_ClipsStayBalanced) {
    const Rect a = rect(0.0F, 0.0F, 100.0F, 100.0F);
    const Rect b = rect(10.0F, 10.0F, 50.0F, 50.0F);

    DrawList balanced;
    {
        Painter painter{balanced};
        painter.pushClip(a);
        EXPECT_EQ(painter.clipDepth(), 1U);
        painter.pushClip(b);
        EXPECT_EQ(painter.clipDepth(), 2U);
        painter.popClip();
        EXPECT_EQ(painter.clipDepth(), 1U);
        painter.popClip();
        EXPECT_EQ(painter.clipDepth(), 0U);
        painter.popClip();
        EXPECT_EQ(painter.clipDepth(), 0U);
    }
    EXPECT_EQ(
        commandsOf(balanced),
        (std::vector<DrawCommand>{PushClip{.rect = a}, PushClip{.rect = b}, PopClip{}, PopClip{}}));

    DrawList unbalanced;
    {
        Painter painter{unbalanced};
        painter.pushClip(a);
        painter.pushClip(b);
    }
    EXPECT_EQ(
        commandsOf(unbalanced),
        (std::vector<DrawCommand>{PushClip{.rect = a}, PushClip{.rect = b}, PopClip{}, PopClip{}}));

    DrawList degenerate;
    {
        Painter painter{degenerate};
        painter.pushClip(rect(5.0F, 5.0F, 0.0F, 10.0F));
    }
    EXPECT_EQ(
        commandsOf(degenerate),
        (std::vector<DrawCommand>{PushClip{.rect = rect(5.0F, 5.0F, 0.0F, 0.0F)}, PopClip{}}));
}

} // namespace
