#pragma once

#include <coil/foundation/error.hpp>
#include <coil/render/text.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace coil::render {

/// Port: loads fonts and shapes UTF-8 text with them (ADR-0009, SPEC-0031). Implemented by
/// backends/text_freetype (HarfBuzz) and by FakeTextEngine for tests. Not thread-safe.
class TextShaper {
public:
    virtual ~TextShaper() = 0;

    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&&) = delete;
    TextShaper& operator=(TextShaper&&) = delete;

    /// Loads a font file (TrueType or OpenType, face 0) from `data`; the shaper keeps its own copy.
    /// Fails with InvalidArgument for data it cannot read as a font.
    [[nodiscard]] virtual Expected<FontId> loadFont(std::span<const std::byte> data) = 0;
    /// The font's vertical metrics at `size` DIPs. Fails with NotFound for an unknown font, then
    /// InvalidArgument for a size that is not finite and > 0.
    [[nodiscard]] virtual Expected<FontMetrics> metrics(FontId font, float size) const = 0;
    /// Shapes `utf8` as one left-to-right run with `font` at `size` DIPs. Positions and advances
    /// are unhinted, so they don't depend on display scale. Fails with NotFound for an unknown
    /// font, then InvalidArgument for an invalid size or text that is not valid UTF-8.
    [[nodiscard]] virtual Expected<ShapedRun> shape(FontId font, float size,
                                                    std::string_view utf8) const = 0;

protected:
    TextShaper() = default;
};

} // namespace coil::render
