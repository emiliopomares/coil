#include <coil/foundation/error.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render/testing/recording_render_backend.hpp>

#include <cstddef>
#include <vector>

namespace coil::render::testing {

Expected<RenderTargetId> RecordingRenderBackend::createTarget(const RenderTargetDesc& /*desc*/) {
    return RenderTargetId{};
}

Expected<void> RecordingRenderBackend::destroyTarget(RenderTargetId /*id*/) {
    return {};
}

Expected<void> RecordingRenderBackend::resizeTarget(RenderTargetId /*id*/,
                                                    const RenderTargetDesc& /*desc*/) {
    return {};
}

Expected<void> RecordingRenderBackend::submit(RenderTargetId /*id*/, const DrawList& /*frame*/) {
    return {};
}

Expected<void> RecordingRenderBackend::present(RenderTargetId /*id*/) {
    return {};
}

Expected<std::vector<DrawList>> RecordingRenderBackend::frames(RenderTargetId /*id*/) const {
    static_cast<void>(m_targets);
    return std::vector<DrawList>{};
}

Expected<std::size_t> RecordingRenderBackend::presentCount(RenderTargetId /*id*/) const {
    static_cast<void>(m_targets);
    return std::size_t{0};
}

Expected<RenderTargetDesc> RecordingRenderBackend::desc(RenderTargetId /*id*/) const {
    static_cast<void>(m_nextId);
    return RenderTargetDesc{};
}

} // namespace coil::render::testing
