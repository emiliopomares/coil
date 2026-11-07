#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/render_backend.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace coil::render {

RenderBackend::~RenderBackend() = default;

Expected<PixelRect> captureRect(const RenderTargetDesc& desc, std::optional<Rect> region) {
    // The target's pixel size, ceil(size x scale) (SPEC-0020).
    const float targetWidth = std::ceil(desc.size.width * desc.scale);
    const float targetHeight = std::ceil(desc.size.height * desc.scale);
    if (!region.has_value()) {
        return PixelRect{.x = 0,
                         .y = 0,
                         .width = static_cast<std::uint32_t>(targetWidth),
                         .height = static_cast<std::uint32_t>(targetHeight)};
    }
    const Rect dips = *region;
    if (!std::isfinite(dips.origin.x) || !std::isfinite(dips.origin.y) ||
        !std::isfinite(dips.size.width) || !std::isfinite(dips.size.height) ||
        dips.size.width < 0.0F || dips.size.height < 0.0F) {
        return makeUnexpected(ErrorCode::InvalidArgument,
                              "a capture region needs finite values and a size >= 0");
    }
    // Rounded out to whole pixels, then clipped to the target (rule 2).
    const float left = std::clamp(std::floor(dips.left() * desc.scale), 0.0F, targetWidth);
    const float top = std::clamp(std::floor(dips.top() * desc.scale), 0.0F, targetHeight);
    const float right = std::clamp(std::ceil(dips.right() * desc.scale), 0.0F, targetWidth);
    const float bottom = std::clamp(std::ceil(dips.bottom() * desc.scale), 0.0F, targetHeight);
    if (right <= left || bottom <= top) {
        return PixelRect{};
    }
    return PixelRect{.x = static_cast<std::uint32_t>(left),
                     .y = static_cast<std::uint32_t>(top),
                     .width = static_cast<std::uint32_t>(right - left),
                     .height = static_cast<std::uint32_t>(bottom - top)};
}

} // namespace coil::render
