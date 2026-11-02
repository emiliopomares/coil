#pragma once

#include <cstdint>

namespace coil::render {

/// Straight (not premultiplied) RGBA in sRGB, each component nominally in [0, 1]. How components
/// are blended and interpolated is the renderer's business (EPIC-006 spec 3).
struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;

    /// From 8-bit components (0-255 -> 0-1).
    [[nodiscard]] static constexpr Color fromRgba8(std::uint8_t red, std::uint8_t green,
                                                   std::uint8_t blue,
                                                   std::uint8_t alpha = 255) noexcept {
        return {.r = static_cast<float>(red) / 255.0F,
                .g = static_cast<float>(green) / 255.0F,
                .b = static_cast<float>(blue) / 255.0F,
                .a = static_cast<float>(alpha) / 255.0F};
    }

    /// From 0xRRGGBBAA.
    [[nodiscard]] static constexpr Color fromHex(std::uint32_t rgba) noexcept {
        return fromRgba8(static_cast<std::uint8_t>(rgba >> 24U),
                         static_cast<std::uint8_t>(rgba >> 16U),
                         static_cast<std::uint8_t>(rgba >> 8U), static_cast<std::uint8_t>(rgba));
    }

    friend constexpr bool operator==(Color, Color) = default;
};

} // namespace coil::render
