#pragma once

// Contract suite every implementation of the text ports must pass (SPEC-0031). `Impl` is one
// object that is both a TextShaper and a FontRasterizer. To run it, in a test source inside
// namespace coil::render:
//   template <> struct TextContractTraits<Impl> {
//       static std::unique_ptr<Impl> make();   // a fresh engine
//       static FontId addFont(Impl& engine);   // a usable font
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Name, TextContract, ::testing::Types<Impl>);

#include <coil/foundation/error.hpp>
#include <coil/render/font_rasterizer.hpp>
#include <coil/render/text.hpp>
#include <coil/render/text_shaper.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace coil::render {

/// Specialize for each implementation under contract.
template <typename Impl>
struct TextContractTraits;

namespace text_contract_detail {

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

/// Relative tolerance of the contract's linearity rules (SPEC-0031 rules 4 and 8).
inline constexpr float kRelativeTolerance = 1e-4F;
/// Absolute floor, so values that should be 0 may carry float noise.
inline constexpr float kAbsoluteTolerance = 1e-6F;

/// `actual` equals `expected` within the contract's tolerance.
inline bool near(float expected, float actual) {
    const float scale = std::max(std::abs(expected), std::abs(actual));
    return std::abs(expected - actual) <= (kRelativeTolerance * scale) + kAbsoluteTolerance;
}

/// Whether `offset` starts a code point in valid UTF-8 `text`.
inline bool isCodePointStart(std::string_view text, std::uint32_t offset) {
    if (offset >= text.size()) {
        return false;
    }
    const auto byte = static_cast<unsigned char>(text.at(offset));
    return (byte & 0xC0U) != 0x80U;
}

/// Rule 6 on a run shaped from `text`: ordered positions, advance = sum of advances, clusters
/// non-decreasing on code point boundaries.
inline ::testing::AssertionResult isWellFormed(const ShapedRun& run, std::string_view text) {
    float sum = 0.0F;
    for (std::size_t i = 0; i < run.glyphs.size(); ++i) {
        const ShapedGlyph& glyph = run.glyphs.at(i);
        sum += glyph.advance;
        if (!isCodePointStart(text, glyph.cluster)) {
            return ::testing::AssertionFailure() << "glyph " << i << ": cluster " << glyph.cluster
                                                 << " is not a code point start";
        }
        if (i == 0) {
            continue;
        }
        const ShapedGlyph& previous = run.glyphs.at(i - 1);
        if (glyph.position.x < previous.position.x &&
            !near(previous.position.x, glyph.position.x)) {
            return ::testing::AssertionFailure() << "glyph " << i << " is left of glyph " << i - 1;
        }
        if (glyph.cluster < previous.cluster) {
            return ::testing::AssertionFailure() << "glyph " << i << ": cluster decreases";
        }
    }
    if (!near(sum, run.advance)) {
        return ::testing::AssertionFailure()
               << "run advance " << run.advance << ", sum of glyph advances " << sum;
    }
    return ::testing::AssertionSuccess();
}

/// Rule 8: `large` is `small` shaped at twice the size.
inline ::testing::AssertionResult isTwice(const ShapedRun& small, const ShapedRun& large) {
    if (small.glyphs.size() != large.glyphs.size()) {
        return ::testing::AssertionFailure()
               << small.glyphs.size() << " glyphs vs " << large.glyphs.size();
    }
    for (std::size_t i = 0; i < small.glyphs.size(); ++i) {
        const ShapedGlyph& a = small.glyphs.at(i);
        const ShapedGlyph& b = large.glyphs.at(i);
        if (a.glyph != b.glyph || a.cluster != b.cluster) {
            return ::testing::AssertionFailure() << "glyph " << i << " differs";
        }
        if (!near(2.0F * a.position.x, b.position.x) || !near(2.0F * a.position.y, b.position.y) ||
            !near(2.0F * a.advance, b.advance)) {
            return ::testing::AssertionFailure() << "glyph " << i << " does not scale linearly";
        }
    }
    if (!near(2.0F * small.advance, large.advance)) {
        return ::testing::AssertionFailure()
               << "run advance " << small.advance << " -> " << large.advance;
    }
    return ::testing::AssertionSuccess();
}

/// The bytes of `text`.
inline std::span<const std::byte> bytesOf(std::string_view text) {
    return std::as_bytes(std::span{text.data(), text.size()});
}

} // namespace text_contract_detail

template <typename Impl>
class TextContract : public ::testing::Test {
protected:
    TextShaper& shaper() { return *m_engine; }

    FontRasterizer& rasterizer() { return *m_engine; }

    FontId addFont() { return TextContractTraits<Impl>::addFont(*m_engine); }

    /// The first glyph of `font` shaped from "a" at 16, or GlyphId{} if shaping gives none.
    GlyphId someGlyph(FontId font) {
        const Expected<ShapedRun> run = m_engine->shape(font, 16.0F, "a");
        if (!run.has_value() || run->glyphs.empty()) {
            return GlyphId{};
        }
        return run->glyphs.front().glyph;
    }

private:
    std::unique_ptr<Impl> m_engine = TextContractTraits<Impl>::make();
};

TYPED_TEST_SUITE_P(TextContract);

TYPED_TEST_P(TextContract, AC1_FontIdsAreDistinctAndUnknownIdsFail) {
    using text_contract_detail::failsWith;
    const FontId first = this->addFont();
    const FontId second = this->addFont();
    EXPECT_NE(first.value, 0U);
    EXPECT_NE(second.value, 0U);
    EXPECT_NE(first, second);

    const GlyphId glyph = this->someGlyph(first);
    const FontId unknownIds[] = {FontId{0}, FontId{std::max(first.value, second.value) + 1}};
    for (const FontId unknown : unknownIds) {
        SCOPED_TRACE(std::format("FontId{{{}}}", unknown.value));
        EXPECT_TRUE(failsWith(this->shaper().metrics(unknown, 16.0F), ErrorCode::NotFound));
        EXPECT_TRUE(failsWith(this->shaper().shape(unknown, 16.0F, "a"), ErrorCode::NotFound));
        EXPECT_TRUE(
            failsWith(this->rasterizer().rasterize(unknown, glyph, 16.0F), ErrorCode::NotFound));
    }
}

TYPED_TEST_P(TextContract, AC2_InvalidSizesAndFontDataFail) {
    using text_contract_detail::bytesOf;
    using text_contract_detail::failsWith;
    const FontId font = this->addFont();
    const GlyphId glyph = this->someGlyph(font);
    const float badSizes[] = {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()};
    for (const float size : badSizes) {
        SCOPED_TRACE(std::format("size {}", size));
        EXPECT_TRUE(failsWith(this->shaper().metrics(font, size), ErrorCode::InvalidArgument));
        EXPECT_TRUE(failsWith(this->shaper().shape(font, size, "a"), ErrorCode::InvalidArgument));
        EXPECT_TRUE(
            failsWith(this->rasterizer().rasterize(font, glyph, size), ErrorCode::InvalidArgument));
    }
    EXPECT_TRUE(failsWith(this->shaper().loadFont({}), ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        failsWith(this->shaper().loadFont(bytesOf("not a font")), ErrorCode::InvalidArgument));
}

TYPED_TEST_P(TextContract, AC3_MetricsScaleLinearly) {
    using text_contract_detail::near;
    using text_contract_detail::succeeds;
    const FontId font = this->addFont();
    const Expected<FontMetrics> small = this->shaper().metrics(font, 10.0F);
    const Expected<FontMetrics> large = this->shaper().metrics(font, 20.0F);
    ASSERT_TRUE(succeeds(small));
    ASSERT_TRUE(succeeds(large));
    for (const FontMetrics& m : {*small, *large}) {
        EXPECT_GT(m.ascent, 0.0F);
        EXPECT_GE(m.descent, 0.0F);
        EXPECT_GE(m.lineGap, 0.0F);
    }
    EXPECT_TRUE(near(2.0F * small->ascent, large->ascent));
    EXPECT_TRUE(near(2.0F * small->descent, large->descent));
    EXPECT_TRUE(near(2.0F * small->lineGap, large->lineGap));
}

TYPED_TEST_P(TextContract, AC4_ShapedRunsAreOrderedAndLinear) {
    using text_contract_detail::isTwice;
    using text_contract_detail::isWellFormed;
    using text_contract_detail::succeeds;
    const FontId font = this->addFont();

    constexpr std::string_view kHello = "Hello, world";
    const Expected<ShapedRun> run = this->shaper().shape(font, 16.0F, kHello);
    ASSERT_TRUE(succeeds(run));
    EXPECT_EQ(run->font, font);
    EXPECT_EQ(run->size, 16.0F);
    EXPECT_FALSE(run->glyphs.empty());
    EXPECT_TRUE(isWellFormed(*run, kHello));
    const Expected<ShapedRun> again = this->shaper().shape(font, 16.0F, kHello);
    ASSERT_TRUE(succeeds(again));
    EXPECT_EQ(*run, *again) << "shaping is not deterministic";

    const Expected<ShapedRun> empty = this->shaper().shape(font, 16.0F, "");
    ASSERT_TRUE(succeeds(empty));
    EXPECT_TRUE(empty->glyphs.empty());
    EXPECT_EQ(empty->advance, 0.0F);

    // "Hello", "ñandú", "日本" (UTF-8).
    constexpr std::string_view kTexts[] = {"Hello",
                                           "\xC3\xB1"
                                           "and\xC3\xBA",
                                           "\xE6\x97\xA5\xE6\x9C\xAC"};
    for (const std::string_view text : kTexts) {
        SCOPED_TRACE(text);
        const Expected<ShapedRun> small = this->shaper().shape(font, 16.0F, text);
        const Expected<ShapedRun> large = this->shaper().shape(font, 32.0F, text);
        ASSERT_TRUE(succeeds(small));
        ASSERT_TRUE(succeeds(large));
        EXPECT_FALSE(small->glyphs.empty());
        EXPECT_TRUE(isWellFormed(*small, text));
        EXPECT_TRUE(isTwice(*small, *large));
    }
}

TYPED_TEST_P(TextContract, AC5_InvalidUtf8Fails) {
    using text_contract_detail::failsWith;
    const FontId font = this->addFont();
    constexpr std::string_view kInvalid[] = {
        "\xFF",         // not a lead byte
        "\xE6\x97",     // truncated three-byte sequence
        "\xC0\xAF",     // overlong '/'
        "\xED\xA0\x80", // encoded surrogate U+D800
    };
    for (const std::string_view text : kInvalid) {
        SCOPED_TRACE(std::format("{} bytes, first {:#04x}", text.size(),
                                 static_cast<unsigned char>(text.front())));
        EXPECT_TRUE(failsWith(this->shaper().shape(font, 16.0F, text), ErrorCode::InvalidArgument));
    }
}

TYPED_TEST_P(TextContract, AC6_ShapedGlyphsRasterize) {
    using text_contract_detail::failsWith;
    using text_contract_detail::succeeds;
    const FontId font = this->addFont();
    const Expected<ShapedRun> run = this->shaper().shape(font, 16.0F, "Ag ");
    ASSERT_TRUE(succeeds(run));
    ASSERT_FALSE(run->glyphs.empty());
    bool anyCoverage = false;
    for (const ShapedGlyph& shaped : run->glyphs) {
        SCOPED_TRACE(std::format("glyph {}", shaped.glyph.value));
        const Expected<GlyphBitmap> bitmap =
            this->rasterizer().rasterize(font, shaped.glyph, 24.0F);
        ASSERT_TRUE(succeeds(bitmap));
        EXPECT_EQ(bitmap->coverage.size(), static_cast<std::size_t>(bitmap->width) *
                                               static_cast<std::size_t>(bitmap->height));
        anyCoverage = anyCoverage ||
                      std::ranges::any_of(bitmap->coverage, [](std::uint8_t c) { return c > 0U; });
        const Expected<GlyphBitmap> again = this->rasterizer().rasterize(font, shaped.glyph, 24.0F);
        ASSERT_TRUE(succeeds(again));
        EXPECT_EQ(*bitmap, *again) << "rasterizing is not deterministic";
    }
    EXPECT_TRUE(anyCoverage) << "no glyph of \"Ag \" covers any pixel";
    EXPECT_TRUE(failsWith(this->rasterizer().rasterize(font, GlyphId{0xFFFF'FFFFU}, 24.0F),
                          ErrorCode::NotFound));
}

REGISTER_TYPED_TEST_SUITE_P(TextContract, AC1_FontIdsAreDistinctAndUnknownIdsFail,
                            AC2_InvalidSizesAndFontDataFail, AC3_MetricsScaleLinearly,
                            AC4_ShapedRunsAreOrderedAndLinear, AC5_InvalidUtf8Fails,
                            AC6_ShapedGlyphsRasterize);

} // namespace coil::render
