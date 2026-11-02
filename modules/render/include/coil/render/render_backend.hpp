#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/draw_list.hpp>

#include <cstdint>

namespace coil::render {

/// Identifies a render target. Unique per backend instance; never reused, even after destruction.
struct RenderTargetId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(RenderTargetId, RenderTargetId) = default;
};

/// A target's logical size (DIPs) and display scale. Its pixel size is
/// ceil(width * scale) x ceil(height * scale), like WindowSystem::pixelSize.
struct RenderTargetDesc {
    Size size;
    float scale = 1.0F;

    friend constexpr bool operator==(RenderTargetDesc, RenderTargetDesc) = default;
};

/// Port: turns DrawLists into pixels on render targets (ADR-0009). Implemented by renderers
/// (software, then GPU backends) and by RecordingRenderBackend for tests. Used from the UI thread.
class RenderBackend {
public:
    virtual ~RenderBackend();

    RenderBackend(const RenderBackend&) = delete;
    RenderBackend& operator=(const RenderBackend&) = delete;
    RenderBackend(RenderBackend&&) = delete;
    RenderBackend& operator=(RenderBackend&&) = delete;

    /// Creates a target. Fails with InvalidArgument if the size is negative or not finite, or the
    /// scale is not finite and > 0. A 0x0 size is valid (for example, a minimized window).
    [[nodiscard]] virtual Expected<RenderTargetId> createTarget(const RenderTargetDesc& desc) = 0;
    /// Destroys a target. Fails with NotFound for an unknown or destroyed id.
    [[nodiscard]] virtual Expected<void> destroyTarget(RenderTargetId id) = 0;
    /// Changes a target's size or scale (for example, after a window resize). Same errors as
    /// createTarget, plus NotFound.
    [[nodiscard]] virtual Expected<void> resizeTarget(RenderTargetId id,
                                                      const RenderTargetDesc& desc) = 0;
    /// Renders `frame` into the target's back buffer. The backend keeps no reference to `frame`.
    /// Fails with NotFound for an unknown id.
    [[nodiscard]] virtual Expected<void> submit(RenderTargetId id, const DrawList& frame) = 0;
    /// Shows the last submitted frame. Fails with NotFound for an unknown id.
    [[nodiscard]] virtual Expected<void> present(RenderTargetId id) = 0;

protected:
    RenderBackend() = default;
};

} // namespace coil::render
