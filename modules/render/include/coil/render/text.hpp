#pragma once

#include <coil/foundation/geometry.hpp>

#include <cstdint>
#include <vector>

namespace coil::render {

/// Identifies a loaded font. Issued by the TextShaper that loaded it; never reused, and never 0
/// (SPEC-0031).
struct FontId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(FontId, FontId) = default;
};

/// A glyph index inside one font. Meaningless without its FontId.
struct GlyphId {
    std::uint32_t value = 0;

    friend constexpr bool operator==(GlyphId, GlyphId) = default;
};

/// Vertical metrics of a font at one size, in DIPs. Line height = ascent + descent + lineGap.
struct FontMetrics {
    float ascent = 0.0F;  ///< Baseline to the top of the tallest glyphs (> 0).
    float descent = 0.0F; ///< Baseline down to the bottom of the lowest glyphs (>= 0).
    float lineGap = 0.0F; ///< Extra space the font asks for between lines (>= 0).

    friend constexpr bool operator==(FontMetrics, FontMetrics) = default;
};

/// One glyph of a shaped run.
struct ShapedGlyph {
    GlyphId glyph;
    /// Pen position for this glyph, relative to the run's origin on the baseline, in DIPs (x to
    /// the right, y down). Includes any offset the shaper applies.
    Point position;
    float advance = 0.0F;      ///< How far the pen moves after this glyph, in DIPs.
    std::uint32_t cluster = 0; ///< Byte offset, in the shaped text, of the cluster's first byte.

    friend constexpr bool operator==(ShapedGlyph, ShapedGlyph) = default;
};

/// A string shaped with one font at one size.
struct ShapedRun {
    FontId font;
    float size = 0.0F;               ///< Font size (em height) in DIPs.
    std::vector<ShapedGlyph> glyphs; ///< In visual order (left to right).
    float advance = 0.0F;            ///< Total pen advance of the run, in DIPs.

    friend bool operator==(const ShapedRun&, const ShapedRun&) = default;
};

/// 8-bit coverage of one glyph at one pixel size. Row 0 is the top; 0 is empty, 255 is full.
struct GlyphBitmap {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    /// From the pen position to the bitmap's left column, in pixels (may be negative).
    std::int32_t left = 0;
    /// From the baseline up to the bitmap's top row, in pixels (FreeType's bitmap_top).
    std::int32_t top = 0;
    std::vector<std::uint8_t> coverage; ///< width * height values, row-major.

    friend bool operator==(const GlyphBitmap&, const GlyphBitmap&) = default;
};

} // namespace coil::render
