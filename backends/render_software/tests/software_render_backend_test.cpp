#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/foundation/image.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/image.hpp>
#include <coil/render/paint.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render/render_backend_contract.hpp>
#include <coil/render_software/software_render_backend.hpp>
#include <coil/tasks/async_result.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

// Printers live in Image's namespace, where GoogleTest finds them by ADL.
namespace coil::foundation {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, Pixel pixel) {
    return os << "Pixel{" << static_cast<int>(pixel.r) << ", " << static_cast<int>(pixel.g) << ", "
              << static_cast<int>(pixel.b) << ", " << static_cast<int>(pixel.a) << "}";
}

std::ostream& operator<<(std::ostream& os, const Image& image) {
    return os << "Image{" << image.width() << "x" << image.height() << "}";
}

} // namespace
} // namespace coil::foundation

namespace coil::render {

template <>
struct RenderBackendContractTraits<render_software::SoftwareRenderBackend> {
    static std::unique_ptr<RenderBackend> make() {
        return std::make_unique<render_software::SoftwareRenderBackend>();
    }
};

using SoftwareImplementations = ::testing::Types<render_software::SoftwareRenderBackend>;
INSTANTIATE_TYPED_TEST_SUITE_P(Software, RenderBackendContract, SoftwareImplementations);

inline namespace {

using render_software::SoftwareRenderBackend;

constexpr Color kRed{.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};
constexpr Color kHalfBlue{.r = 0.0F, .g = 0.0F, .b = 1.0F, .a = 0.5F};
constexpr Pixel kRedPixel{.r = 255, .g = 0, .b = 0, .a = 255};
constexpr Pixel kClear{};

Rect rect(float x, float y, float width, float height) {
    return Rect{.origin = {.x = x, .y = y}, .size = {.width = width, .height = height}};
}

RenderTargetId createOk(SoftwareRenderBackend& backend, const RenderTargetDesc& desc) {
    const Expected<RenderTargetId> result = backend.createTarget(desc);
    EXPECT_TRUE(succeeds(result));
    return result.value_or(RenderTargetId{});
}

/// Renders `commands` on a fresh target of `width` x `height` DIPs at `scale` and returns the back
/// buffer.
Image render(float width, float height, const std::vector<DrawCommand>& commands,
             float scale = 1.0F) {
    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(width, height, scale));
    DrawList frame;
    for (const DrawCommand& command : commands) {
        frame.append(command);
    }
    EXPECT_TRUE(succeeds(backend.submit(id, frame)));
    const Expected<Image> image = backend.backBuffer(id);
    EXPECT_TRUE(succeeds(image));
    return image.value_or(Image{});
}

Pixel at(const Image& image, std::uint32_t x, std::uint32_t y) {
    const std::optional<Pixel> pixel = image.pixel(x, y);
    EXPECT_TRUE(pixel.has_value()) << "no pixel (" << x << ", " << y << ") in " << image;
    return pixel.value_or(Pixel{.r = 1, .g = 2, .b = 3, .a = 4});
}

/// Each channel within 1 of `expected` (partially covered pixels, SPEC-0021 §5).
::testing::AssertionResult isNear(Pixel actual, Pixel expected) {
    const auto close = [](std::uint8_t a, std::uint8_t b) { return std::abs(a - b) <= 1; };
    if (close(actual.r, expected.r) && close(actual.g, expected.g) && close(actual.b, expected.b) &&
        close(actual.a, expected.a)) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << actual << " is not within 1 of " << expected;
}

bool allClear(const Image& image) {
    return std::ranges::all_of(image.pixels(), [](Pixel pixel) { return pixel == kClear; });
}

TEST(SoftwareRenderBackend, AC2_ImagesFollowTargetSize) {
    const Image blank{3, 2};
    EXPECT_EQ(blank.width(), 3U);
    EXPECT_EQ(blank.height(), 2U);
    EXPECT_EQ(blank.pixels().size(), 6U);
    EXPECT_TRUE(allClear(blank));
    EXPECT_TRUE(blank.pixel(2, 1).has_value());
    EXPECT_FALSE(blank.pixel(3, 0).has_value());
    EXPECT_FALSE(blank.pixel(0, 2).has_value());

    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(4.0F, 3.0F, 2.0F));
    const Image back = backend.backBuffer(id).value_or(Image{});
    EXPECT_EQ(back.width(), 8U);
    EXPECT_EQ(back.height(), 6U);
    EXPECT_TRUE(allClear(back));
    EXPECT_EQ(backend.frontBuffer(id).value_or(Image{}), back);

    ASSERT_TRUE(succeeds(backend.resizeTarget(id, targetDesc(5.0F, 5.0F))));
    EXPECT_EQ(backend.backBuffer(id).value_or(Image{}).width(), 5U);
    EXPECT_EQ(backend.frontBuffer(id).value_or(Image{}).height(), 5U);

    DrawList frame;
    frame.append(FillRect{.rect = rect(0.0F, 0.0F, 5.0F, 5.0F), .paint = kRed});
    ASSERT_TRUE(succeeds(backend.submit(id, frame)));
    EXPECT_EQ(at(backend.backBuffer(id).value_or(Image{}), 2, 2), kRedPixel);
    EXPECT_TRUE(allClear(backend.frontBuffer(id).value_or(Image{})));
    ASSERT_TRUE(succeeds(backend.present(id)));
    EXPECT_EQ(backend.frontBuffer(id).value_or(Image{}), backend.backBuffer(id).value_or(Image{}));
}

TEST(SoftwareRenderBackend, AC3_FillsPixelAlignedRects) {
    const Image image =
        render(4.0F, 3.0F, {FillRect{.rect = rect(1.0F, 1.0F, 2.0F, 1.0F), .paint = kRed}});
    for (std::uint32_t y = 0; y < 3; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const bool inside = y == 1 && (x == 1 || x == 2);
            EXPECT_EQ(at(image, x, y), inside ? kRedPixel : kClear) << "(" << x << ", " << y << ")";
        }
    }

    const Image scaled =
        render(4.0F, 3.0F, {FillRect{.rect = rect(1.0F, 1.0F, 2.0F, 1.0F), .paint = kRed}}, 2.0F);
    for (std::uint32_t y = 0; y < 6; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const bool inside = y >= 2 && y <= 3 && x >= 2 && x <= 5;
            EXPECT_EQ(at(scaled, x, y), inside ? kRedPixel : kClear)
                << "(" << x << ", " << y << ")";
        }
    }

    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(4.0F, 3.0F));
    DrawList frame;
    frame.append(FillRect{.rect = rect(0.0F, 0.0F, 4.0F, 3.0F), .paint = kRed});
    ASSERT_TRUE(succeeds(backend.submit(id, frame)));
    ASSERT_TRUE(succeeds(backend.submit(id, DrawList{})));
    const Image cleared = backend.backBuffer(id).value_or(Image{1, 1});
    EXPECT_EQ(cleared.width(), 4U);
    EXPECT_TRUE(allClear(cleared));
}

TEST(SoftwareRenderBackend, AC4_AntiAliasesFractionalEdges) {
    const Image wide =
        render(4.0F, 1.0F, {FillRect{.rect = rect(0.0F, 0.0F, 1.5F, 1.0F), .paint = kRed}});
    EXPECT_EQ(at(wide, 0, 0), kRedPixel);
    EXPECT_TRUE(isNear(at(wide, 1, 0), Pixel{.r = 255, .g = 0, .b = 0, .a = 128}));
    EXPECT_EQ(at(wide, 2, 0), kClear);

    const Image narrow =
        render(4.0F, 1.0F, {FillRect{.rect = rect(0.25F, 0.0F, 0.5F, 1.0F), .paint = kRed}});
    EXPECT_TRUE(isNear(at(narrow, 0, 0), Pixel{.r = 255, .g = 0, .b = 0, .a = 128}));
    EXPECT_EQ(at(narrow, 1, 0), kClear);
}

TEST(SoftwareRenderBackend, AC5_BlendsSourceOver) {
    const Image image =
        render(2.0F, 1.0F,
               {
                   FillRect{.rect = rect(0.0F, 0.0F, 1.0F, 1.0F), .paint = kRed},
                   FillRect{.rect = rect(0.0F, 0.0F, 2.0F, 1.0F), .paint = kHalfBlue},
               });
    EXPECT_TRUE(isNear(at(image, 0, 0), Pixel{.r = 128, .g = 0, .b = 128, .a = 255}));
    EXPECT_TRUE(isNear(at(image, 1, 0), Pixel{.r = 0, .g = 0, .b = 255, .a = 128}));
}

TEST(SoftwareRenderBackend, AC6_RoundsCorners) {
    const Image round = render(10.0F, 10.0F,
                               {FillRoundedRect{.rect = rect(0.0F, 0.0F, 10.0F, 10.0F),
                                                .radii = CornerRadii::all(5.0F),
                                                .paint = kRed}});
    for (std::uint32_t y = 4; y <= 5; ++y) {
        for (std::uint32_t x = 4; x <= 5; ++x) {
            EXPECT_EQ(at(round, x, y), kRedPixel) << "(" << x << ", " << y << ")";
        }
    }
    EXPECT_EQ(at(round, 0, 0), kClear);
    const Pixel edge = at(round, 0, 4);
    EXPECT_GE(edge.a, 1);
    EXPECT_LE(edge.a, 254);
    for (std::uint32_t y = 0; y < 10; ++y) {
        for (std::uint32_t x = 0; x < 10; ++x) {
            EXPECT_EQ(at(round, x, y), at(round, 9 - x, y))
                << "horizontal mirror (" << x << ", " << y << ")";
            EXPECT_EQ(at(round, x, y), at(round, x, 9 - y))
                << "vertical mirror (" << x << ", " << y << ")";
        }
    }

    const Image square =
        render(10.0F, 10.0F,
               {FillRoundedRect{
                   .rect = rect(2.0F, 3.0F, 5.0F, 4.0F), .radii = CornerRadii{}, .paint = kRed}});
    const Image plain =
        render(10.0F, 10.0F, {FillRect{.rect = rect(2.0F, 3.0F, 5.0F, 4.0F), .paint = kRed}});
    EXPECT_EQ(square, plain);
    EXPECT_FALSE(allClear(plain));
}

TEST(SoftwareRenderBackend, AC7_StrokesBordersInside) {
    const Image image = render(10.0F, 10.0F,
                               {StrokeBorder{.rect = rect(0.0F, 0.0F, 10.0F, 10.0F),
                                             .radii = CornerRadii{},
                                             .width = 2.0F,
                                             .paint = kRed}});
    for (std::uint32_t y = 0; y < 10; ++y) {
        for (std::uint32_t x = 0; x < 10; ++x) {
            const bool ring = x <= 1 || x >= 8 || y <= 1 || y >= 8;
            EXPECT_EQ(at(image, x, y), ring ? kRedPixel : kClear) << "(" << x << ", " << y << ")";
        }
    }
}

TEST(SoftwareRenderBackend, AC8_ClipsNestAndRestore) {
    const FillRect everything{.rect = rect(0.0F, 0.0F, 8.0F, 8.0F), .paint = kRed};
    const auto redPixels = [](const Image& image) {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> red;
        for (std::uint32_t y = 0; y < image.height(); ++y) {
            for (std::uint32_t x = 0; x < image.width(); ++x) {
                if (image.pixel(x, y) == kRedPixel) {
                    red.emplace_back(x, y);
                }
            }
        }
        return red;
    };
    using Pairs = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

    const Image outer =
        render(8.0F, 8.0F, {PushClip{.rect = rect(0.0F, 0.0F, 2.0F, 2.0F)}, everything, PopClip{}});
    EXPECT_EQ(redPixels(outer), (Pairs{{0, 0}, {1, 0}, {0, 1}, {1, 1}}));

    const Image nested =
        render(8.0F, 8.0F,
               {PushClip{.rect = rect(0.0F, 0.0F, 2.0F, 2.0F)},
                PushClip{.rect = rect(1.0F, 1.0F, 5.0F, 5.0F)}, everything, PopClip{}, PopClip{}});
    EXPECT_EQ(redPixels(nested), (Pairs{{1, 1}}));

    const Image restored =
        render(8.0F, 8.0F,
               {PushClip{.rect = rect(0.0F, 0.0F, 2.0F, 2.0F)},
                PushClip{.rect = rect(1.0F, 1.0F, 5.0F, 5.0F)}, PopClip{}, everything, PopClip{}});
    EXPECT_EQ(redPixels(restored), (Pairs{{0, 0}, {1, 0}, {0, 1}, {1, 1}}));

    const Image empty =
        render(8.0F, 8.0F,
               {PushClip{.rect = rect(3.0F, 3.0F, 0.0F, 0.0F)}, everything, PopClip{},
                FillRect{.rect = rect(7.0F, 7.0F, 1.0F, 1.0F), .paint = kRed}});
    EXPECT_EQ(redPixels(empty), (Pairs{{7, 7}}));

    const Expected<Paint> gradient =
        linearGradient(Point{.x = 0.0F, .y = 0.0F}, Point{.x = 8.0F, .y = 0.0F},
                       {GradientStop{.offset = 0.0F, .color = kRed},
                        GradientStop{.offset = 1.0F, .color = kHalfBlue}});
    ASSERT_TRUE(succeeds(gradient));
    const Image gradientFill = render(
        8.0F, 8.0F,
        {FillRect{.rect = rect(0.0F, 0.0F, 8.0F, 8.0F), .paint = gradient.value_or(Paint{kRed})},
         FillRect{.rect = rect(7.0F, 7.0F, 1.0F, 1.0F), .paint = kRed}});
    EXPECT_EQ(redPixels(gradientFill), (Pairs{{7, 7}}));
    EXPECT_EQ(at(gradientFill, 0, 0), kClear);
}

// SPEC-0025: capture from the software backend (SC<n>).

/// The image a capture gives; 0x0 (and a test failure) if it failed.
Image capturedImage(SoftwareRenderBackend& backend, RenderTargetId id,
                    std::optional<Rect> region = std::nullopt) {
    const Expected<CapturedFrame> frame = captureNow(backend, id, region);
    EXPECT_TRUE(succeeds(frame));
    return frame.has_value() ? frame->image : Image{};
}

/// The front buffer; 0x0 (and a test failure) if it cannot be read.
Image front(const SoftwareRenderBackend& backend, RenderTargetId id) {
    const Expected<Image> image = backend.frontBuffer(id);
    EXPECT_TRUE(succeeds(image));
    return image.value_or(Image{});
}

/// A frame filling `width` x `height` DIPs with `colour`.
DrawList filled(float width, float height, Color colour) {
    DrawList frame;
    frame.append(FillRect{.rect = rect(0.0F, 0.0F, width, height), .paint = colour});
    return frame;
}

TEST(SoftwareRenderBackend, SC1_CaptureIsTheFrontBuffer) {
    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(20.0F, 10.0F));
    ASSERT_TRUE(succeeds(backend.submit(id, oneRectFrame())));
    ASSERT_TRUE(succeeds(backend.present(id)));
    const Image shown = front(backend, id);
    EXPECT_EQ(capturedImage(backend, id), shown);

    const Image region = capturedImage(backend, id, rect(5.0F, 2.0F, 10.0F, 6.0F));
    ASSERT_EQ(region.width(), 10U);
    ASSERT_EQ(region.height(), 6U);
    for (std::uint32_t y = 0; y < 6; ++y) {
        for (std::uint32_t x = 0; x < 10; ++x) {
            EXPECT_EQ(region.pixel(x, y), shown.pixel(x + 5, y + 2))
                << "(" << x << ", " << y << ")";
        }
    }
}

TEST(SoftwareRenderBackend, SC2_CaptureShowsThePresentedFrame) {
    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(4.0F, 4.0F));
    EXPECT_EQ(capturedImage(backend, id).width(), 4U);
    EXPECT_TRUE(allClear(capturedImage(backend, id))) << "before any present";

    ASSERT_TRUE(succeeds(backend.submit(id, filled(4.0F, 4.0F, kRed))));
    ASSERT_TRUE(succeeds(backend.present(id)));
    ASSERT_TRUE(succeeds(backend.submit(id, filled(4.0F, 4.0F, kHalfBlue))));
    tasks::AsyncResult<CapturedFrame> result = backend.capture(id, std::nullopt);
    EXPECT_TRUE(result.isReady()) << "not ready when capture returned";
    const std::optional<Expected<CapturedFrame>> outcome = result.take();
    const Image image = outcome.has_value() && outcome->has_value() ? (*outcome)->image : Image{};
    EXPECT_EQ(image.width(), 4U);
    EXPECT_EQ(image.pixel(0, 0), std::optional<Pixel>{kRedPixel})
        << "a submitted, unpresented frame";
}

TEST(SoftwareRenderBackend, SC3_CaptureIsACopy) {
    SoftwareRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(4.0F, 4.0F));
    ASSERT_TRUE(succeeds(backend.submit(id, filled(4.0F, 4.0F, kRed))));
    ASSERT_TRUE(succeeds(backend.present(id)));
    const Image captured = capturedImage(backend, id);
    const Image expected = front(backend, id);
    ASSERT_EQ(captured, expected);

    ASSERT_TRUE(succeeds(backend.submit(id, filled(4.0F, 4.0F, kHalfBlue))));
    ASSERT_TRUE(succeeds(backend.present(id)));
    EXPECT_EQ(captured, expected) << "changed by a later frame";
    ASSERT_TRUE(succeeds(backend.destroyTarget(id)));
    EXPECT_EQ(captured, expected) << "changed by destroying the target";
}

} // namespace
} // namespace coil::render
