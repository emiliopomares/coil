#pragma once

#include <coil/foundation/image.hpp>

namespace coil::render {

// Image and Pixel live in foundation, so that windows can present them too (SPEC-0022).
using foundation::Image;
using foundation::Pixel;

} // namespace coil::render
