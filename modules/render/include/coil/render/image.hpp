#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace coil::render {

/// One pixel: 8-bit sRGB-encoded components, straight (not premultiplied) alpha.
struct Pixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;

    friend constexpr bool operator==(Pixel, Pixel) = default;
};

/// A width x height grid of pixels, row-major, top row first. Default: 0x0.
class Image {
public:
    Image() = default;
    /// All pixels transparent (0, 0, 0, 0).
    Image(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }

    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }

    [[nodiscard]] std::span<const Pixel> pixels() const noexcept { return m_pixels; }

    [[nodiscard]] std::span<Pixel> pixels() noexcept { return m_pixels; }

    /// The pixel at (x, y), or nullopt outside the image.
    [[nodiscard]] std::optional<Pixel> pixel(std::uint32_t x, std::uint32_t y) const noexcept;

    friend bool operator==(const Image&, const Image&) = default;

private:
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::vector<Pixel> m_pixels;
};

} // namespace coil::render
