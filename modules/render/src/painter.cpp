#include <coil/foundation/geometry.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/paint.hpp>
#include <coil/render/painter.hpp>

#include <algorithm>
#include <cmath>

namespace coil::render {

namespace {

/// Finite, with a positive width and height (rule 4).
bool isDrawable(Rect rect) {
    return std::isfinite(rect.origin.x) && std::isfinite(rect.origin.y) &&
           std::isfinite(rect.size.width) && std::isfinite(rect.size.height) &&
           rect.size.width > 0.0F && rect.size.height > 0.0F;
}

/// Half the rect's smaller side: the largest radius or border width that fits.
float halfSmallerSide(Rect rect) {
    return std::min(rect.size.width, rect.size.height) / 2.0F;
}

/// NaN or negative -> 0; then at most `limit` (rule 5).
float normalizeRadius(float radius, float limit) {
    if (std::isnan(radius) || radius < 0.0F) {
        return 0.0F;
    }
    return std::min(radius, limit);
}

CornerRadii normalize(CornerRadii radii, Rect rect) {
    const float limit = halfSmallerSide(rect);
    return CornerRadii{.topLeft = normalizeRadius(radii.topLeft, limit),
                       .topRight = normalizeRadius(radii.topRight, limit),
                       .bottomRight = normalizeRadius(radii.bottomRight, limit),
                       .bottomLeft = normalizeRadius(radii.bottomLeft, limit)};
}

} // namespace

Painter::~Painter() {
    for (; m_clipDepth > 0; --m_clipDepth) {
        m_target->append(PopClip{});
    }
}

void Painter::fillRect(Rect rect, const Paint& paint) {
    if (isDrawable(rect)) {
        m_target->append(FillRect{.rect = rect, .paint = paint});
    }
}

void Painter::fillRoundedRect(Rect rect, CornerRadii radii, const Paint& paint) {
    if (isDrawable(rect)) {
        m_target->append(
            FillRoundedRect{.rect = rect, .radii = normalize(radii, rect), .paint = paint});
    }
}

void Painter::strokeBorder(Rect rect, CornerRadii radii, float width, const Paint& paint) {
    if (!isDrawable(rect) || !std::isfinite(width) || width <= 0.0F) {
        return;
    }
    m_target->append(StrokeBorder{.rect = rect,
                                  .radii = normalize(radii, rect),
                                  .width = std::min(width, halfSmallerSide(rect)),
                                  .paint = paint});
}

void Painter::pushClip(Rect rect) {
    const Rect clip = isDrawable(rect) ? rect : Rect{.origin = rect.origin, .size = {}};
    m_target->append(PushClip{.rect = clip});
    ++m_clipDepth;
}

void Painter::popClip() {
    if (m_clipDepth == 0) {
        return;
    }
    m_target->append(PopClip{});
    --m_clipDepth;
}

} // namespace coil::render
