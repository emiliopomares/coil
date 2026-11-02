#include <coil/render/image.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

namespace coil::render {

Image::Image(std::uint32_t width, std::uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_pixels(static_cast<std::size_t>(width) * height, Pixel{}) {}

std::optional<Pixel> Image::pixel(std::uint32_t x, std::uint32_t y) const noexcept {
    if (x >= m_width || y >= m_height) {
        return std::nullopt;
    }
    const auto index = static_cast<std::ptrdiff_t>((static_cast<std::size_t>(y) * m_width) + x);
    return *std::next(m_pixels.begin(), index); // in range: checked above
}

} // namespace coil::render
