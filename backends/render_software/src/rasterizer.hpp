#pragma once

#include <coil/render/draw_list.hpp>
#include <coil/render/image.hpp>

namespace coil::render_software::detail {

/// Clears `target` to transparent and draws `frame` into it (SPEC-0021 rules 2-7). `scale` maps
/// DIPs to pixels.
void rasterize(const render::DrawList& frame, float scale, render::Image& target);

} // namespace coil::render_software::detail
