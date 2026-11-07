#include <coil/foundation/error.hpp>
#include <coil/foundation/image.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render/render_backend_contract.hpp>
#include <coil/render/testing/recording_render_backend.hpp>
#include <coil/tasks/async_result.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

namespace coil::render {

template <>
struct RenderBackendContractTraits<testing::RecordingRenderBackend> {
    static std::unique_ptr<RenderBackend> make() {
        return std::make_unique<testing::RecordingRenderBackend>();
    }
};

using RecordingImplementations = ::testing::Types<testing::RecordingRenderBackend>;
INSTANTIATE_TYPED_TEST_SUITE_P(Recording, RenderBackendContract, RecordingImplementations);

inline namespace {

std::ostream& operator<<(std::ostream& os, const DrawList& list) {
    return os << "DrawList{" << list.commands().size() << " commands}";
}

std::ostream& operator<<(std::ostream& os, RenderTargetDesc desc) {
    return os << "RenderTargetDesc{" << desc.size.width << "x" << desc.size.height << " @"
              << desc.scale << "}";
}

using testing::RecordingRenderBackend;

RenderTargetId createOk(RecordingRenderBackend& backend, const RenderTargetDesc& desc) {
    const Expected<RenderTargetId> result = backend.createTarget(desc);
    EXPECT_TRUE(succeeds(result));
    return result.value_or(RenderTargetId{});
}

TEST(RecordingRenderBackend, AC6_KeepsCopiesOfFrames) {
    RecordingRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(100.0F, 100.0F));

    DrawList frame = oneRectFrame();
    ASSERT_TRUE(succeeds(backend.submit(id, frame)));
    ASSERT_TRUE(succeeds(backend.submit(id, DrawList{})));
    frame.clear();
    frame.append(PopClip{});

    const Expected<std::vector<DrawList>> frames = backend.frames(id);
    ASSERT_TRUE(succeeds(frames));
    EXPECT_EQ(frames.value_or(std::vector<DrawList>{}),
              (std::vector<DrawList>{oneRectFrame(), DrawList{}}));
}

TEST(RecordingRenderBackend, AC7_CountsPresentsAndTracksDescriptions) {
    RecordingRenderBackend backend;
    const RenderTargetId a = createOk(backend, targetDesc(100.0F, 50.0F));
    const RenderTargetId b = createOk(backend, targetDesc(10.0F, 10.0F, 2.0F));

    ASSERT_TRUE(succeeds(backend.present(a)));
    ASSERT_TRUE(succeeds(backend.present(a)));
    ASSERT_TRUE(succeeds(backend.present(b)));
    EXPECT_EQ(backend.presentCount(a).value_or(0), 2U);
    EXPECT_EQ(backend.presentCount(b).value_or(0), 1U);

    EXPECT_EQ(backend.desc(a).value_or(RenderTargetDesc{}), targetDesc(100.0F, 50.0F));
    EXPECT_EQ(backend.desc(b).value_or(RenderTargetDesc{}), targetDesc(10.0F, 10.0F, 2.0F));
    ASSERT_TRUE(succeeds(backend.resizeTarget(a, targetDesc(300.0F, 200.0F, 1.5F))));
    EXPECT_EQ(backend.desc(a).value_or(RenderTargetDesc{}), targetDesc(300.0F, 200.0F, 1.5F));
    EXPECT_TRUE(
        failsWith(backend.resizeTarget(a, targetDesc(-1.0F, 200.0F)), ErrorCode::InvalidArgument));
    EXPECT_EQ(backend.desc(a).value_or(RenderTargetDesc{}), targetDesc(300.0F, 200.0F, 1.5F));
}

TEST(RecordingRenderBackend, AC8_InspectionOfUnknownTargetsFails) {
    RecordingRenderBackend backend;
    const RenderTargetId destroyed = createOk(backend, targetDesc(100.0F, 100.0F));
    ASSERT_TRUE(succeeds(backend.destroyTarget(destroyed)));

    for (const RenderTargetId id : {RenderTargetId{.value = 999'999}, destroyed}) {
        EXPECT_TRUE(failsWith(backend.frames(id), ErrorCode::NotFound)) << id;
        EXPECT_TRUE(failsWith(backend.presentCount(id), ErrorCode::NotFound)) << id;
        EXPECT_TRUE(failsWith(backend.desc(id), ErrorCode::NotFound)) << id;
    }
}

// SPEC-0025 AC-7: the recording backend captures transparent images.
TEST(RecordingRenderBackend, SC4_CaptureIsTransparent) {
    RecordingRenderBackend backend;
    const RenderTargetId id = createOk(backend, targetDesc(10.0F, 5.0F, 2.0F));
    ASSERT_TRUE(succeeds(backend.submit(id, oneRectFrame())));
    ASSERT_TRUE(succeeds(backend.present(id)));

    tasks::AsyncResult<CapturedFrame> whole = backend.capture(id, std::nullopt);
    EXPECT_TRUE(whole.isReady());
    const std::optional<Expected<CapturedFrame>> outcome = whole.take();
    const Image image = outcome.has_value() && outcome->has_value() ? (*outcome)->image : Image{};
    EXPECT_EQ(image.width(), 20U);
    EXPECT_EQ(image.height(), 10U);
    EXPECT_TRUE(std::ranges::all_of(image.pixels(), [](Pixel pixel) { return pixel == Pixel{}; }));

    const Expected<CapturedFrame> region = captureNow(backend, id, dipRect(1.0F, 1.0F, 2.0F, 2.0F));
    EXPECT_EQ(capturedSize(region), (std::pair{4U, 4U}));
}

} // namespace
} // namespace coil::render
