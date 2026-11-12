#include <coil/foundation/error.hpp>
#include <coil/render/testing/fake_text_engine.hpp>
#include <coil/render/text.hpp>
#include <coil/render/text_contract.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string_view>
#include <vector>

namespace coil::render {

template <>
struct TextContractTraits<testing::FakeTextEngine> {
    static std::unique_ptr<testing::FakeTextEngine> make() {
        return std::make_unique<testing::FakeTextEngine>();
    }

    static FontId addFont(testing::FakeTextEngine& engine) {
        return engine.addFont(testing::FakeFontDesc{});
    }
};

using FakeImplementations = ::testing::Types<testing::FakeTextEngine>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fake, TextContract, FakeImplementations);

namespace {

using text_contract_detail::bytesOf;
using text_contract_detail::failsWith;
using text_contract_detail::succeeds;

/// The font of AC-7 and AC-8.
constexpr testing::FakeFontDesc kDesc{
    .advance = 0.5F, .ascent = 0.8F, .descent = 0.2F, .lineGap = 0.1F};

/// A solid box bitmap (rule 12).
GlyphBitmap solidBox(std::uint32_t width, std::uint32_t height, std::int32_t top) {
    return GlyphBitmap{.width = width,
                       .height = height,
                       .left = 0,
                       .top = top,
                       .coverage = std::vector<std::uint8_t>(
                           static_cast<std::size_t>(width) * height, std::uint8_t{255})};
}

TEST(FakeTextEngine, AC7_FixedAdvanceMetricsAndShaping) {
    testing::FakeTextEngine engine;
    const FontId font = engine.addFont(kDesc);

    const Expected<FontMetrics> metrics = engine.metrics(font, 10.0F);
    ASSERT_TRUE(succeeds(metrics));
    EXPECT_FLOAT_EQ(metrics->ascent, 8.0F);
    EXPECT_FLOAT_EQ(metrics->descent, 2.0F);
    EXPECT_FLOAT_EQ(metrics->lineGap, 1.0F);

    // "aé日": 1, 2 and 3 bytes.
    const Expected<ShapedRun> run = engine.shape(font, 10.0F, "a\xC3\xA9\xE6\x97\xA5");
    ASSERT_TRUE(succeeds(run));
    const ShapedRun expected{
        .font = font,
        .size = 10.0F,
        .glyphs = {{.glyph = GlyphId{0x61}, .position = {}, .advance = 5.0F, .cluster = 0},
                   {.glyph = GlyphId{0xE9},
                    .position = {.x = 5.0F, .y = 0.0F},
                    .advance = 5.0F,
                    .cluster = 1},
                   {.glyph = GlyphId{0x65E5},
                    .position = {.x = 10.0F, .y = 0.0F},
                    .advance = 5.0F,
                    .cluster = 3}},
        .advance = 15.0F};
    EXPECT_EQ(*run, expected);

    // The fake reads no font files, not even something that starts like one.
    constexpr std::string_view kTrueTypeHeader{"\x00\x01\x00\x00", 4};
    EXPECT_TRUE(failsWith(engine.loadFont(bytesOf(kTrueTypeHeader)), ErrorCode::InvalidArgument));
}

TEST(FakeTextEngine, AC8_SolidBoxGlyphs) {
    testing::FakeTextEngine engine;
    const FontId font = engine.addFont(kDesc);

    const Expected<GlyphBitmap> at10 = engine.rasterize(font, GlyphId{'A'}, 10.0F);
    ASSERT_TRUE(succeeds(at10));
    EXPECT_EQ(*at10, solidBox(5U, 10U, 8));
    // ceil(7.5) = 8 wide; ceil(12) + ceil(3) = 15 high.
    const Expected<GlyphBitmap> at15 = engine.rasterize(font, GlyphId{'A'}, 15.0F);
    ASSERT_TRUE(succeeds(at15));
    EXPECT_EQ(*at15, solidBox(8U, 15U, 12));

    for (const std::uint32_t whitespace : {0x20U, 0xA0U, 0x09U}) {
        SCOPED_TRACE(std::format("U+{:04X}", whitespace));
        const Expected<GlyphBitmap> blank = engine.rasterize(font, GlyphId{whitespace}, 10.0F);
        ASSERT_TRUE(succeeds(blank));
        EXPECT_EQ(*blank, GlyphBitmap{});
    }
    EXPECT_TRUE(failsWith(engine.rasterize(font, GlyphId{0x110000}, 10.0F), ErrorCode::NotFound));
    EXPECT_TRUE(failsWith(engine.rasterize(font, GlyphId{0xD800}, 10.0F), ErrorCode::NotFound));
}

} // namespace

} // namespace coil::render
