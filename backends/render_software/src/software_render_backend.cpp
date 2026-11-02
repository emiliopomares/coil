#include <coil/foundation/error.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/image.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render_software/software_render_backend.hpp>

namespace coil::render_software {

using render::DrawList;
using render::Image;
using render::RenderTargetDesc;
using render::RenderTargetId;

Expected<RenderTargetId> SoftwareRenderBackend::createTarget(const RenderTargetDesc& /*desc*/) {
    return RenderTargetId{};
}

Expected<void> SoftwareRenderBackend::destroyTarget(RenderTargetId /*id*/) {
    return {};
}

Expected<void> SoftwareRenderBackend::resizeTarget(RenderTargetId /*id*/,
                                                   const RenderTargetDesc& /*desc*/) {
    return {};
}

Expected<void> SoftwareRenderBackend::submit(RenderTargetId /*id*/, const DrawList& /*frame*/) {
    return {};
}

Expected<void> SoftwareRenderBackend::present(RenderTargetId /*id*/) {
    return {};
}

Expected<Image> SoftwareRenderBackend::backBuffer(RenderTargetId /*id*/) const {
    static_cast<void>(m_targets);
    return Image{};
}

Expected<Image> SoftwareRenderBackend::frontBuffer(RenderTargetId /*id*/) const {
    static_cast<void>(m_nextId);
    return Image{};
}

} // namespace coil::render_software
