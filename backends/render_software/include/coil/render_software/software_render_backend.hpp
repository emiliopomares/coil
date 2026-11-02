#pragma once

#include <coil/foundation/error.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/image.hpp>
#include <coil/render/render_backend.hpp>

#include <cstdint>
#include <map>

namespace coil::render_software {

/// CPU rasterizer (ADR-0009 Amendment 1, ADR-0017): deterministic, with no OS or GPU dependency.
/// Each target has a back buffer, drawn by submit(), and a front buffer, updated by present().
class SoftwareRenderBackend final : public render::RenderBackend {
public:
    SoftwareRenderBackend() = default;
    ~SoftwareRenderBackend() override = default;
    SoftwareRenderBackend(const SoftwareRenderBackend&) = delete;
    SoftwareRenderBackend& operator=(const SoftwareRenderBackend&) = delete;
    SoftwareRenderBackend(SoftwareRenderBackend&&) = delete;
    SoftwareRenderBackend& operator=(SoftwareRenderBackend&&) = delete;

    [[nodiscard]] Expected<render::RenderTargetId>
    createTarget(const render::RenderTargetDesc& desc) override;
    [[nodiscard]] Expected<void> destroyTarget(render::RenderTargetId id) override;
    [[nodiscard]] Expected<void> resizeTarget(render::RenderTargetId id,
                                              const render::RenderTargetDesc& desc) override;
    [[nodiscard]] Expected<void> submit(render::RenderTargetId id,
                                        const render::DrawList& frame) override;
    [[nodiscard]] Expected<void> present(render::RenderTargetId id) override;

    /// The image the last submit() rendered. NotFound for an unknown or destroyed id.
    [[nodiscard]] Expected<render::Image> backBuffer(render::RenderTargetId id) const;
    /// The image the last present() showed (transparent before any present). NotFound as above.
    [[nodiscard]] Expected<render::Image> frontBuffer(render::RenderTargetId id) const;

private:
    struct Target {
        render::RenderTargetDesc desc;
        render::Image back;
        render::Image front;
    };

    std::map<std::uint64_t, Target> m_targets;
    std::uint64_t m_nextId = 1; ///< Ids start at 1 and are never reused.
};

} // namespace coil::render_software
