#include <coil/foundation/error.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render/testing/recording_render_backend.hpp>

#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <vector>

namespace coil::render::testing {

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

} // namespace

Expected<RenderTargetId> RecordingRenderBackend::createTarget(const RenderTargetDesc& desc) {
    if (!isValid(desc)) {
        return invalidDesc();
    }
    const RenderTargetId id{.value = m_nextId++};
    m_targets.emplace(id.value, Target{.desc = desc, .frames = {}, .presents = 0});
    return id;
}

Expected<void> RecordingRenderBackend::destroyTarget(RenderTargetId id) {
    if (m_targets.erase(id.value) == 0) {
        return unknownTarget(id);
    }
    return {};
}

Expected<void> RecordingRenderBackend::resizeTarget(RenderTargetId id,
                                                    const RenderTargetDesc& desc) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    if (!isValid(desc)) {
        return invalidDesc();
    }
    target->second.desc = desc;
    return {};
}

Expected<void> RecordingRenderBackend::submit(RenderTargetId id, const DrawList& frame) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    target->second.frames.push_back(frame);
    return {};
}

Expected<void> RecordingRenderBackend::present(RenderTargetId id) {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    ++target->second.presents;
    return {};
}

Expected<std::vector<DrawList>> RecordingRenderBackend::frames(RenderTargetId id) const {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    return target->second.frames;
}

Expected<std::size_t> RecordingRenderBackend::presentCount(RenderTargetId id) const {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    return target->second.presents;
}

Expected<RenderTargetDesc> RecordingRenderBackend::desc(RenderTargetId id) const {
    const auto target = m_targets.find(id.value);
    if (target == m_targets.end()) {
        return unknownTarget(id);
    }
    return target->second.desc;
}

} // namespace coil::render::testing
