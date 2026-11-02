#include "rasterizer.hpp"

#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/image.hpp>
#include <coil/render/paint.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <variant>
#include <vector>

namespace coil::render_software::detail {

namespace {

using render::Color;
using render::CornerRadii;
using render::DrawCommand;
using render::Image;
using render::Paint;
using render::Pixel;

/// An axis-aligned box in pixel space, [x0, x1) x [y0, y1).
struct Box {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;

    /// Also true when a coordinate is NaN.
    [[nodiscard]] bool isEmpty() const noexcept { return !(x1 > x0) || !(y1 > y0); }
};

Box intersect(Box a, Box b) {
    return Box{.x0 = std::max(a.x0, b.x0),
               .y0 = std::max(a.y0, b.y0),
               .x1 = std::min(a.x1, b.x1),
               .y1 = std::min(a.y1, b.y1)};
}

/// The rect in pixel space; empty if any component is not finite.
Box toPixels(Rect rect, float scale) {
    const Box box{.x0 = rect.origin.x * scale,
                  .y0 = rect.origin.y * scale,
                  .x1 = (rect.origin.x + rect.size.width) * scale,
                  .y1 = (rect.origin.y + rect.size.height) * scale};
    if (!std::isfinite(box.x0) || !std::isfinite(box.y0) || !std::isfinite(box.x1) ||
        !std::isfinite(box.y1)) {
        return Box{};
    }
    return box;
}

CornerRadii scaled(CornerRadii radii, float scale) {
    return CornerRadii{.topLeft = radii.topLeft * scale,
                       .topRight = radii.topRight * scale,
                       .bottomRight = radii.bottomRight * scale,
                       .bottomLeft = radii.bottomLeft * scale};
}

/// Area of pixel (x, y)'s unit square inside `box` (rule 4, rects and clips).
float areaCoverage(Box box, float x, float y) {
    const float width = std::max(0.0F, std::min(x + 1.0F, box.x1) - std::max(x, box.x0));
    const float height = std::max(0.0F, std::min(y + 1.0F, box.y1) - std::max(y, box.y0));
    return width * height;
}

/// The radius of the corner on the given side (y grows downwards).
float cornerRadius(CornerRadii radii, bool right, bool bottom) {
    if (bottom) {
        return right ? radii.bottomRight : radii.bottomLeft;
    }
    return right ? radii.topRight : radii.topLeft;
}

/// Signed distance from (px, py) to the outline of `box` with rounded corners (negative inside).
float roundedBoxDistance(Box box, CornerRadii radii, float px, float py) {
    const float halfWidth = (box.x1 - box.x0) / 2.0F;
    const float halfHeight = (box.y1 - box.y0) / 2.0F;
    const float x = px - (box.x0 + halfWidth);
    const float y = py - (box.y0 + halfHeight);
    const float rawRadius = cornerRadius(radii, x > 0.0F, y > 0.0F);
    const float radius =
        std::clamp(std::isnan(rawRadius) ? 0.0F : rawRadius, 0.0F, std::min(halfWidth, halfHeight));
    const float qx = std::abs(x) - halfWidth + radius;
    const float qy = std::abs(y) - halfHeight + radius;
    const float outside = std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F));
    const float inside = std::min(std::max(qx, qy), 0.0F);
    return outside + inside - radius;
}

/// Coverage of pixel (x, y) by a rounded box: a one-pixel ramp across the outline (rule 4).
float roundedCoverage(Box box, CornerRadii radii, float x, float y) {
    if (box.isEmpty()) {
        return 0.0F;
    }
    return std::clamp(0.5F - roundedBoxDistance(box, radii, x + 0.5F, y + 0.5F), 0.0F, 1.0F);
}

float clampComponent(float value) {
    return std::isnan(value) ? 0.0F : std::clamp(value, 0.0F, 1.0F);
}

/// Premultiplied colour, kept as floats while a frame is drawn.
struct Premultiplied {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 0.0F;
};

class Canvas {
public:
    Canvas(std::uint32_t width, std::uint32_t height, float scale)
        : m_width(width)
        , m_height(height)
        , m_scale(scale)
        , m_pixels(static_cast<std::size_t>(width) * height)
        , m_clips{Box{.x0 = 0.0F,
                      .y0 = 0.0F,
                      .x1 = static_cast<float>(width),
                      .y1 = static_cast<float>(height)}} {}

    void draw(const DrawCommand& command) {
        std::visit([this](const auto& value) { drawCommand(value); }, command);
    }

    void writeTo(Image& target) const {
        auto out = target.pixels().begin();
        for (const Premultiplied& pixel : m_pixels) {
            *out = toPixel(pixel);
            out = std::next(out);
        }
    }

private:
    void drawCommand(const render::FillRect& command) {
        const Box box = toPixels(command.rect, m_scale);
        fill(command.paint, box, [box](float x, float y) { return areaCoverage(box, x, y); });
    }

    void drawCommand(const render::FillRoundedRect& command) {
        const Box box = toPixels(command.rect, m_scale);
        const CornerRadii radii = scaled(command.radii, m_scale);
        fill(command.paint, grown(box),
             [box, radii](float x, float y) { return roundedCoverage(box, radii, x, y); });
    }

    void drawCommand(const render::StrokeBorder& command) {
        const Box outer = toPixels(command.rect, m_scale);
        const CornerRadii outerRadii = scaled(command.radii, m_scale);
        const float width = command.width * m_scale;
        const Box inner{.x0 = outer.x0 + width,
                        .y0 = outer.y0 + width,
                        .x1 = outer.x1 - width,
                        .y1 = outer.y1 - width};
        const auto shrink = [width](float radius) { return std::max(0.0F, radius - width); };
        const CornerRadii innerRadii{.topLeft = shrink(outerRadii.topLeft),
                                     .topRight = shrink(outerRadii.topRight),
                                     .bottomRight = shrink(outerRadii.bottomRight),
                                     .bottomLeft = shrink(outerRadii.bottomLeft)};
        fill(command.paint, grown(outer), [=](float x, float y) {
            return std::max(0.0F, roundedCoverage(outer, outerRadii, x, y) -
                                      roundedCoverage(inner, innerRadii, x, y));
        });
    }

    void drawCommand(const render::PushClip& command) {
        m_clips.push_back(intersect(m_clips.back(), toPixels(command.rect, m_scale)));
    }

    void drawCommand(const render::PopClip& /*command*/) {
        if (m_clips.size() > 1) {
            m_clips.pop_back();
        }
    }

    /// The box grown by one pixel on each side, for distance-based coverage ramps.
    static Box grown(Box box) {
        return Box{
            .x0 = box.x0 - 1.0F, .y0 = box.y0 - 1.0F, .x1 = box.x1 + 1.0F, .y1 = box.y1 + 1.0F};
    }

    /// Blends `paint` into every pixel of `bounds` (within the clip) with the given coverage.
    template <typename Coverage>
    void fill(const Paint& paint, Box bounds, Coverage coverage) {
        const auto* color = std::get_if<Color>(&paint);
        const Box clip = m_clips.back();
        const Box area = intersect(bounds, clip);
        if (color == nullptr || area.isEmpty()) {
            return; // gradient paints draw nothing yet (rule 6)
        }
        const float alpha = clampComponent(color->a);
        const Premultiplied source{.r = clampComponent(color->r) * alpha,
                                   .g = clampComponent(color->g) * alpha,
                                   .b = clampComponent(color->b) * alpha,
                                   .a = alpha};
        const auto firstX = static_cast<std::uint32_t>(std::max(0.0F, std::floor(area.x0)));
        const auto firstY = static_cast<std::uint32_t>(std::max(0.0F, std::floor(area.y0)));
        const auto endX =
            static_cast<std::uint32_t>(std::min(static_cast<float>(m_width), std::ceil(area.x1)));
        const auto endY =
            static_cast<std::uint32_t>(std::min(static_cast<float>(m_height), std::ceil(area.y1)));
        for (std::uint32_t y = firstY; y < endY; ++y) {
            for (std::uint32_t x = firstX; x < endX; ++x) {
                const auto px = static_cast<float>(x);
                const auto py = static_cast<float>(y);
                const float amount = coverage(px, py) * areaCoverage(clip, px, py);
                if (amount > 0.0F) {
                    blend(pixelAt(x, y), source, amount);
                }
            }
        }
    }

    /// Source-over in premultiplied form (rule 5).
    static void blend(Premultiplied& target, Premultiplied source, float amount) {
        const float keep = 1.0F - (source.a * amount);
        target.r = (source.r * amount) + (target.r * keep);
        target.g = (source.g * amount) + (target.g * keep);
        target.b = (source.b * amount) + (target.b * keep);
        target.a = (source.a * amount) + (target.a * keep);
    }

    Premultiplied& pixelAt(std::uint32_t x, std::uint32_t y) {
        const auto index = static_cast<std::ptrdiff_t>((static_cast<std::size_t>(y) * m_width) + x);
        return *std::next(m_pixels.begin(), index);
    }

    static std::uint8_t toByte(float value) {
        return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }

    static Pixel toPixel(Premultiplied pixel) {
        if (pixel.a <= 0.0F) {
            return Pixel{};
        }
        return Pixel{.r = toByte(pixel.r / pixel.a),
                     .g = toByte(pixel.g / pixel.a),
                     .b = toByte(pixel.b / pixel.a),
                     .a = toByte(pixel.a)};
    }

    std::uint32_t m_width;
    std::uint32_t m_height;
    float m_scale;
    std::vector<Premultiplied> m_pixels;
    std::vector<Box> m_clips; ///< The first entry is the whole target.
};

} // namespace

void rasterize(const render::DrawList& frame, float scale, render::Image& target) {
    Canvas canvas{target.width(), target.height(), scale};
    for (const DrawCommand& command : frame.commands()) {
        canvas.draw(command);
    }
    canvas.writeTo(target);
}

} // namespace coil::render_software::detail
