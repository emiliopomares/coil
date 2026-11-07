#include "boxes.hpp"

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/linear_layout.hpp>
#include <coil/layout/modifier.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/render/color.hpp>
#include <coil/render/draw_list.hpp>
#include <coil/render/painter.hpp>
#include <coil/render/render_backend.hpp>
#include <coil/render_software/software_render_backend.hpp>

#include <algorithm>
#include <cstdio>
#include <expected>
#include <memory>
#include <ostream>
#include <print>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace boxes {

namespace {

using coil::Expected;
using coil::Rect;
using coil::Size;
using coil::layout::Column;
using coil::layout::CrossAxisAlignment;
using coil::layout::LinearLayoutOptions;
using coil::layout::ModifierChain;
using coil::layout::Row;
using coil::platform::WindowId;
using coil::platform::WindowSystem;
using coil::render::Color;
using coil::render::RenderTargetDesc;
using coil::render::RenderTargetId;
using coil::render_software::SoftwareRenderBackend;

/// A leaf that takes the smallest size its constraints allow and is painted in one colour.
class ColoredBox final : public coil::layout::LayoutElement {
public:
    explicit ColoredBox(Color colour) noexcept
        : m_colour(colour) {}

    [[nodiscard]] Color colour() const noexcept { return m_colour; }

protected:
    [[nodiscard]] Size measureContent(const coil::layout::BoxConstraints& constraints) override {
        return constraints.constrain(Size{});
    }

    void arrangeContent(Rect /*contentBounds*/) override {}

private:
    Color m_colour;
};

/// The layout tree and the boxes to paint, in paint order. The pointers point into `root`, which
/// owns them for as long as the scene exists (SPEC-0023 Q3).
struct Scene {
    std::unique_ptr<Column> root;
    std::vector<const ColoredBox*> boxes;
};

constexpr float kGap = 8.0F;

/// Stretched children, kGap apart.
constexpr LinearLayoutOptions kStretched{.spacing = kGap, .crossAxis = CrossAxisAlignment::Stretch};

/// Adds a ColoredBox with `chain` to `parent` and to the scene's paint list.
template <typename Container>
Expected<void> addBox(Container& parent, Scene& scene, Color colour, ModifierChain chain) {
    auto box = std::make_unique<ColoredBox>(colour);
    if (auto set = box->setModifiers(std::move(chain)); !set) {
        return set;
    }
    const ColoredBox* painted = box.get();
    if (auto added = parent.add(std::move(box)); !added) {
        return added;
    }
    scene.boxes.push_back(painted);
    return {};
}

/// Builds the scene of SPEC-0023 §4.2.
Expected<Scene> makeScene() {
    Scene scene{.root = std::make_unique<Column>(), .boxes = {}};
    auto content = std::make_unique<Column>();
    auto body = std::make_unique<Row>();
    // A braced list runs its steps in order, so `content` is filled before it is moved into `body`.
    // Every step runs; the first failure is reported.
    const auto steps = {
        scene.root->setModifiers(ModifierChain{}.then(coil::layout::padding(kGap))),
        scene.root->setOptions(kStretched),
        addBox(*scene.root, scene, kHeader,
               ModifierChain{}.then(coil::layout::size({.width = 0.0F, .height = 48.0F}))),
        body->setModifiers(ModifierChain{}.then(coil::layout::weight(1.0F))),
        body->setOptions(kStretched),
        addBox(*body, scene, kSidebar,
               ModifierChain{}.then(coil::layout::size({.width = 160.0F, .height = 0.0F}))),
        content->setModifiers(ModifierChain{}.then(coil::layout::weight(1.0F))),
        content->setOptions(kStretched),
        addBox(*content, scene, kTopPanel, ModifierChain{}.then(coil::layout::weight(1.0F))),
        addBox(*content, scene, kBottomPanel, ModifierChain{}.then(coil::layout::weight(2.0F))),
        body->add(std::move(content)),
        scene.root->add(std::move(body)),
        addBox(*scene.root, scene, kFooter,
               ModifierChain{}.then(coil::layout::size({.width = 0.0F, .height = 32.0F}))),
    };
    for (const Expected<void>& step : steps) {
        if (!step) {
            return std::unexpected(step.error());
        }
    }
    return scene;
}

/// The window's logical size and display scale, as a render target description.
Expected<RenderTargetDesc> targetDescOf(const WindowSystem& windows, WindowId window) {
    const auto size = windows.size(window);
    if (!size) {
        return std::unexpected(size.error());
    }
    const auto scale = windows.displayScale(window);
    if (!scale) {
        return std::unexpected(scale.error());
    }
    return RenderTargetDesc{.size = {.width = static_cast<float>(size->width),
                                     .height = static_cast<float>(size->height)},
                            .scale = *scale};
}

/// Lays out, paints, renders and presents one frame, then writes the frame line (rule 2).
Expected<void> drawFrame(WindowSystem& windows, SoftwareRenderBackend& renderer, WindowId window,
                         RenderTargetId target, Scene& scene, std::ostream& log) {
    const auto desc = targetDescOf(windows, window);
    if (!desc) {
        return std::unexpected(desc.error());
    }
    if (auto laidOut = coil::layout::layoutRoot(*scene.root, desc->size); !laidOut) {
        return laidOut;
    }
    coil::render::DrawList frame;
    {
        coil::render::Painter painter{frame};
        painter.fillRect(Rect{.origin = {}, .size = desc->size}, kBackground);
        for (const ColoredBox* box : scene.boxes) {
            painter.fillRect(box->bounds(), box->colour());
        }
    }
    if (auto resized = renderer.resizeTarget(target, *desc); !resized) {
        return resized;
    }
    if (auto submitted = renderer.submit(target, frame); !submitted) {
        return submitted;
    }
    if (auto presented = renderer.present(target); !presented) {
        return presented;
    }
    const auto image = renderer.frontBuffer(target);
    if (!image) {
        return std::unexpected(image.error());
    }
    if (auto shown = windows.presentImage(window, *image); !shown) {
        return shown;
    }
    std::println(log, "boxes: frame {}x{} at scale {}", image->width(), image->height(),
                 desc->scale);
    return {};
}

/// Whether `event` changes the size or scale of `window` (rule 3).
bool changesWindow(const coil::platform::WindowEvent& event, WindowId window) {
    if (const auto* resized = std::get_if<coil::platform::Resized>(&event)) {
        return resized->window == window;
    }
    if (const auto* rescaled = std::get_if<coil::platform::ScaleChanged>(&event)) {
        return rescaled->window == window;
    }
    return false;
}

/// Destroys the target and the window; failures are reported but change nothing (rule 4).
void release(WindowSystem& windows, SoftwareRenderBackend& renderer, WindowId window,
             RenderTargetId target) {
    if (const auto destroyed = renderer.destroyTarget(target); !destroyed) {
        std::println(stderr, "boxes: cannot destroy render target: {}", destroyed.error());
    }
    if (const auto destroyed = windows.destroyWindow(window); !destroyed) {
        std::println(stderr, "boxes: cannot destroy window: {}", destroyed.error());
    }
}

} // namespace

int runBoxes(coil::platform::WindowSystem& windows,
             coil::render_software::SoftwareRenderBackend& renderer, std::ostream& log) {
    auto scene = makeScene();
    if (!scene) {
        std::println(stderr, "boxes: cannot build the scene: {}", scene.error());
        return 1;
    }
    const auto window = windows.createWindow({.title = std::string{kTitle},
                                              .size = kSize,
                                              .resizable = true,
                                              .limits = {.min = kMinSize}});
    if (!window) {
        std::println(stderr, "boxes: cannot create window: {}", window.error());
        return 1;
    }
    const auto desc = targetDescOf(windows, *window);
    const auto target = desc ? renderer.createTarget(*desc)
                             : Expected<RenderTargetId>{std::unexpected(desc.error())};
    if (!target) {
        std::println(stderr, "boxes: cannot create render target: {}", target.error());
        if (const auto destroyed = windows.destroyWindow(*window); !destroyed) {
            std::println(stderr, "boxes: cannot destroy window: {}", destroyed.error());
        }
        return 1;
    }
    const auto draw = [&] {
        if (const auto drawn = drawFrame(windows, renderer, *window, *target, *scene, log);
            !drawn) {
            std::println(stderr, "boxes: cannot draw a frame: {}", drawn.error());
        }
    };
    draw();
    const coil::platform::WindowEvent closeRequested =
        coil::platform::CloseRequested{.window = *window};
    while (true) {
        const auto events = windows.waitEvents(kIdleTimeout);
        if (std::ranges::any_of(events, [&](const coil::platform::WindowEvent& event) {
                return changesWindow(event, *window);
            })) {
            draw();
        }
        if (std::ranges::contains(events, closeRequested)) {
            release(windows, renderer, *window, *target);
            return 0;
        }
    }
}

} // namespace boxes
