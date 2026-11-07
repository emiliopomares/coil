#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/render_backend.hpp>

#include <optional>

namespace coil::render {

RenderBackend::~RenderBackend() = default;

Expected<PixelRect> captureRect(const RenderTargetDesc& desc, std::optional<Rect> region) {
    static_cast<void>(desc);
    static_cast<void>(region);
    return PixelRect{}; // Red stub (SPEC-0025)
}

} // namespace coil::render
