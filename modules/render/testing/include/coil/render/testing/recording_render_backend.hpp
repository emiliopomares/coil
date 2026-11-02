#pragma once

#include <coil/foundation/error.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/render_backend.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace coil::render::testing {

/// Test double for RenderBackend (ADR-0003): keeps everything it is given, draws nothing.
class RecordingRenderBackend final : public RenderBackend {
public:
    RecordingRenderBackend() = default;
    ~RecordingRenderBackend() override = default;
    RecordingRenderBackend(const RecordingRenderBackend&) = delete;
    RecordingRenderBackend& operator=(const RecordingRenderBackend&) = delete;
    RecordingRenderBackend(RecordingRenderBackend&&) = delete;
    RecordingRenderBackend& operator=(RecordingRenderBackend&&) = delete;

    [[nodiscard]] Expected<RenderTargetId> createTarget(const RenderTargetDesc& desc) override;
    [[nodiscard]] Expected<void> destroyTarget(RenderTargetId id) override;
    [[nodiscard]] Expected<void> resizeTarget(RenderTargetId id,
                                              const RenderTargetDesc& desc) override;
    [[nodiscard]] Expected<void> submit(RenderTargetId id, const DrawList& frame) override;
    [[nodiscard]] Expected<void> present(RenderTargetId id) override;

    /// Copies of the frames submitted to `id`, oldest first. NotFound for an unknown or destroyed
    /// id.
    [[nodiscard]] Expected<std::vector<DrawList>> frames(RenderTargetId id) const;
    /// Number of present() calls for `id`. NotFound as above.
    [[nodiscard]] Expected<std::size_t> presentCount(RenderTargetId id) const;
    /// The target's current description. NotFound as above.
    [[nodiscard]] Expected<RenderTargetDesc> desc(RenderTargetId id) const;

private:
    struct Target {
        RenderTargetDesc desc;
        std::vector<DrawList> frames;
        std::size_t presents = 0;
    };

    std::map<std::uint64_t, Target> m_targets;
    std::uint64_t m_nextId = 1; ///< Ids start at 1 and are never reused.
};

} // namespace coil::render::testing
