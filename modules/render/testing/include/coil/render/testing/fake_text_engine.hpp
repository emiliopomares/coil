#pragma once

#include <coil/foundation/error.hpp>
#include <coil/render/font_rasterizer.hpp>
#include <coil/render/text.hpp>
#include <coil/render/text_shaper.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace coil::render::testing {

/// Shape of a fake font, in ems (multiplied by the size to get DIPs or pixels).
struct FakeFontDesc {
    float advance = 0.5F; ///< Every glyph's advance.
    float ascent = 0.8F;
    float descent = 0.2F;
    float lineGap = 0.0F;
};

/// Deterministic fake of both text ports (SPEC-0031 rules 9-12): fixed advances, one glyph per code
/// point (glyph id = code point), solid-box glyphs.
class FakeTextEngine final : public TextShaper, public FontRasterizer {
public:
    FakeTextEngine() = default;
    ~FakeTextEngine() override = default;
    FakeTextEngine(const FakeTextEngine&) = delete;
    FakeTextEngine& operator=(const FakeTextEngine&) = delete;
    FakeTextEngine(FakeTextEngine&&) = delete;
    FakeTextEngine& operator=(FakeTextEngine&&) = delete;

    /// Adds a fake font. Precondition: every field is finite, advance > 0, ascent > 0,
    /// descent >= 0, lineGap >= 0.
    [[nodiscard]] FontId addFont(const FakeFontDesc& desc);

    /// Always fails with InvalidArgument: the fake reads no font files.
    [[nodiscard]] Expected<FontId> loadFont(std::span<const std::byte> data) override;
    [[nodiscard]] Expected<FontMetrics> metrics(FontId font, float size) const override;
    [[nodiscard]] Expected<ShapedRun> shape(FontId font, float size,
                                            std::string_view utf8) const override;
    [[nodiscard]] Expected<GlyphBitmap> rasterize(FontId font, GlyphId glyph,
                                                  float pixelSize) const override;

private:
    std::vector<FakeFontDesc> m_fonts; ///< Font id N is m_fonts[N - 1].
};

} // namespace coil::render::testing
