#include "rasterizer.hpp"

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/foundation/image.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render_software/software_render_backend.hpp>
#include <coil/tasks/async_result.hpp>

#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>

namespace coil::render_software {

using render::DrawList;
using render::Image;
using render::RenderTargetDesc;
using render::RenderTargetId;

namespace {

/// Size finite and >= 0, scale finite and > 0 (SPEC-0020 rule 1).
bool isValid(const RenderTargetDesc& desc) {
    return std::isfinite(desc.size.width) && std::isfinite(desc.size.height) &&
           desc.size.width >= 0.0F && desc.size.height >= 0.0F && std::isfinite(desc.scale) &&
           desc.scale > 0.0F;
}

std::unexpected<Error> invalidDesc() {
    return makeUnexpected(ErrorCode::InvalidArgument,
                          "a render target needs a finite size >= 0 and a finite scale > 0");
}

std::unexpected<Error> unknownTarget(RenderTargetId id) {
    return makeUnexpected(ErrorCode::NotFound, std::format("render target {}", id.value));
}

/// A transparent image of the target's pixel size, ceil(size x scale) (SPEC-0021 rule 1).
Image blankImage(const RenderTargetDesc& desc) {
    return Image{static_cast<std::uint32_t>(std::ceil(desc.size.width * desc.scale)),
                 static_cast<std::uint32_t>(std::ceil(desc.size.height * desc.scale))};
}

} // namespace

Expected<RenderTargetId> SoftwareRenderBackend::createTarget(const RenderTargetDesc& desc) {
    if (!isValid(desc)) {
        return invalidDesc();
    }
    const RenderTargetId id{.value = m_nextId++};
    m_targets.emplace(id.value,
                      Target{.desc = desc, .back = blankImage(desc), .front = blankImage(desc)});
    return id;
}

Expected<void> SoftwareRenderBackend::destroyTarget(RenderTargetId id) {
    if (m_targets.erase(id.value) == 0) {
        return unknownTarget(id);
    }
    return {};
}

Expected<void> SoftwareRenderBackend::resizeTarget(RenderTargetId id,
                                                   const RenderTargetDesc& desc) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    if (!isValid(desc)) {
        return invalidDesc();
    }
    target->second = Target{.desc = desc, .back = blankImage(desc), .front = blankImage(desc)};
    return {};
}

Expected<void> SoftwareRenderBackend::submit(RenderTargetId id, const DrawList& frame) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    detail::rasterize(frame, target->second.desc.scale, target->second.back);
    return {};
}

Expected<void> SoftwareRenderBackend::present(RenderTargetId id) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    target->second.front = target->second.back;
    return {};
}

Expected<Image> SoftwareRenderBackend::backBuffer(RenderTargetId id) const {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    return target->second.back;
}

Expected<Image> SoftwareRenderBackend::frontBuffer(RenderTargetId id) const {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    return target->second.front;
}

tasks::AsyncResult<render::CapturedFrame>
SoftwareRenderBackend::capture(RenderTargetId id, std::optional<Rect> region) {
    const bool known = m_targets.contains(id.value);
    static_cast<void>(known);
    static_cast<void>(region);
    return tasks::AsyncResult<render::CapturedFrame>::ready(
        render::CapturedFrame{.image = {}, .scale = 0.0F}); // Red stub (SPEC-0025)
}

} // namespace coil::render_software
