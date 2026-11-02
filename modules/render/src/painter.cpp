#include <coil/foundation/geometry.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/paint.hpp>
#include <coil/render/painter.hpp>

namespace coil::render {

Painter::~Painter() {
    m_clipDepth = 0;
}

void Painter::fillRect(Rect /*rect*/, const Paint& /*paint*/) {
    static_cast<void>(m_target);
}

void Painter::fillRoundedRect(Rect /*rect*/, CornerRadii /*radii*/, const Paint& /*paint*/) {
    static_cast<void>(m_target);
}

void Painter::strokeBorder(Rect /*rect*/, CornerRadii /*radii*/, float /*width*/,
                           const Paint& /*paint*/) {
    static_cast<void>(m_target);
}

void Painter::pushClip(Rect /*rect*/) {
    static_cast<void>(m_target);
}

void Painter::popClip() {
    static_cast<void>(m_target);
}

} // namespace coil::render
