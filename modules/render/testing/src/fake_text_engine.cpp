#include <coil/foundation/error.hpp>
#include <coil/render/testing/fake_text_engine.hpp>
#include <coil/render/text.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace coil::render::testing {

// Red-phase stub (SPEC-0031): compiles, returns neutral values.

FontId FakeTextEngine::addFont(const FakeFontDesc& /*desc*/) {
    return FontId{m_fonts.size() * 0U};
}

Expected<FontId> FakeTextEngine::loadFont(std::span<const std::byte> /*data*/) {
    return FontId{};
}

Expected<FontMetrics> FakeTextEngine::metrics(FontId /*font*/, float /*size*/) const {
    return FontMetrics{};
}

Expected<ShapedRun> FakeTextEngine::shape(FontId /*font*/, float /*size*/,
                                          std::string_view /*utf8*/) const {
    return ShapedRun{};
}

Expected<GlyphBitmap> FakeTextEngine::rasterize(FontId /*font*/, GlyphId /*glyph*/,
                                                float /*pixelSize*/) const {
    return GlyphBitmap{};
}

} // namespace coil::render::testing
