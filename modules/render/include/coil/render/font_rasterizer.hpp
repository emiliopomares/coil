#pragma once

#include <coil/foundation/error.hpp>
#include <coil/render/text.hpp>

namespace coil::render {

/// Port: turns glyphs into coverage bitmaps (ADR-0009, SPEC-0031). Used by renderers. Accepts the
/// FontIds of the TextShaper it is paired with. Not thread-safe.
class FontRasterizer {
public:
    virtual ~FontRasterizer() = 0;

    FontRasterizer(const FontRasterizer&) = delete;
    FontRasterizer& operator=(const FontRasterizer&) = delete;
    FontRasterizer(FontRasterizer&&) = delete;
    FontRasterizer& operator=(FontRasterizer&&) = delete;

    /// Rasterizes `glyph` of `font` with an em height of `pixelSize` pixels. Fails with NotFound
    /// for an unknown font, InvalidArgument for a pixel size that is not finite and > 0, and
    /// NotFound for a glyph the font doesn't have.
    [[nodiscard]] virtual Expected<GlyphBitmap> rasterize(FontId font, GlyphId glyph,
                                                          float pixelSize) const = 0;

protected:
    FontRasterizer() = default;
};

} // namespace coil::render
