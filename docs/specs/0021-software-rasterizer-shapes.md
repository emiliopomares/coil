# SPEC-0021: Software renderer: shapes, borders and clips

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::render` (`Image`); `backends/render_software` (new adapter) |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 3, first part; see §9 Q1) |
| Related ADRs | ADR-0009 (+ Amendment 1: software first), ADR-0017 (software reference renderer), ADR-0006 (pixels only in backends) |
| Depends on | SPEC-0019 (`DrawList`), SPEC-0020 (`RenderBackend` port, contract suite) |

> **Scope Gate checklist**
> - [x] One concern: turn solid-colour shapes, borders and clips from a `DrawList` into pixels on the CPU
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 areas (`render`, new `backends/render_software`); new layering entry approved here (§8)
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
This is the first time Coil produces pixels. `render_software` is a deterministic CPU rasterizer that
implements the `RenderBackend` port (ADR-0009 Amendment 1). It is the first real renderer, the
reference for golden tests, and the source of screenshots (ADR-0017). This spec covers solid-colour
rectangles, rounded rectangles, borders and clips, with anti-aliased edges, at any display scale.
Gradients are a second spec (§9 Q1). Showing the pixels in a window is spec 4.

## 2. Scope
- `render`: `Image`, an RGBA8 pixel buffer (`Pixel`), used for render output now and for capture and
  presentation next.
- `backends/render_software`: `SoftwareRenderBackend`, implementing `RenderBackend` and passing the
  contract suite, with a back buffer and a front buffer per target.
- Rasterization of `FillRect`, `FillRoundedRect`, `StrokeBorder`, `PushClip`/`PopClip` with solid
  `Color` paints; anti-aliasing; source-over blending.

## 3. Out of Scope
- Gradient paints (the next spec, §9 Q1). Until then, commands with a gradient paint draw nothing.
- Presenting in a window (spec 4); frame capture as a port feature and PNG (spec 5, ADR-0017).
- Golden-image files (§9 Q5); performance work (SIMD, tiling, dirty regions).
- Linear-light blending, colour management, HDR.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/render/include/coil/render/image.hpp
namespace coil::render {

/// One pixel: 8-bit sRGB-encoded components, straight (not premultiplied) alpha.
struct Pixel {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;
    friend constexpr bool operator==(Pixel, Pixel) = default;
};

/// A width x height grid of pixels, row-major, top row first. Default: 0x0.
class Image {
public:
    Image() = default;
    /// All pixels transparent (0, 0, 0, 0).
    Image(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] std::span<const Pixel> pixels() const noexcept;
    [[nodiscard]] std::span<Pixel> pixels() noexcept;
    /// The pixel at (x, y), or nullopt outside the image.
    [[nodiscard]] std::optional<Pixel> pixel(std::uint32_t x, std::uint32_t y) const noexcept;
    friend bool operator==(const Image&, const Image&) = default;
};

} // namespace coil::render
```
```cpp
// backends/render_software/include/coil/render_software/software_render_backend.hpp
namespace coil::render_software {

/// CPU rasterizer (ADR-0009 Amendment 1, ADR-0017). Deterministic; no OS or GPU dependency.
class SoftwareRenderBackend final : public render::RenderBackend {
public:
    // RenderBackend overrides (createTarget, destroyTarget, resizeTarget, submit, present) …

    /// The image the last submit() rendered. NotFound for an unknown or destroyed id.
    [[nodiscard]] Expected<render::Image> backBuffer(render::RenderTargetId id) const;
    /// The image the last present() showed (transparent before any present). NotFound as above.
    [[nodiscard]] Expected<render::Image> frontBuffer(render::RenderTargetId id) const;
};

} // namespace coil::render_software
```

### 4.2 Behavioral rules
1. **Pixel size.** A target with logical size `w × h` at scale `s` has images of
   `ceil(w·s) × ceil(h·s)` pixels. Creating or resizing a target makes both buffers that size, all
   transparent.
2. **Frames.** `submit` clears the back buffer to transparent (0, 0, 0, 0), then draws the commands in
   order. `present` copies the back buffer to the front buffer.
3. **Coordinates.** A point at `(x, y)` DIPs is at `(x·s, y·s)` in pixel space. Pixel `(i, j)` is the
   square `[i, i+1) × [j, j+1)`, with its centre at `(i + 0.5, j + 0.5)`.
4. **Coverage (anti-aliasing).** Each command gives each pixel a coverage in [0, 1]:
   - `FillRect`: the area of the pixel square inside the rect (exact).
   - `FillRoundedRect`: `clamp(0.5 − d, 0, 1)`, where `d` is the signed distance from the pixel centre
     to the rounded rect's outline (negative inside). For straight edges this equals the area rule.
   - `StrokeBorder`: coverage of the outer rounded rect minus coverage of the inner one (the rect moved
     in by `width` on each side, radii reduced by `width`, floored at 0), floored at 0.
   - Clips: the coverage is multiplied by the area of the pixel square inside the current clip
     rectangle (the intersection of every pushed clip, rule 7).
5. **Colour.** A colour's components are clamped to [0, 1]. A pixel is drawn with source-over blending
   in sRGB-encoded space (as browsers and CSS do): with `α = colour.a × coverage`, the result is
   `out.a = α + dst.a × (1 − α)` and `out.rgb = (colour.rgb × α + dst.rgb × dst.a × (1 − α)) / out.a`
   (0 if `out.a` is 0). Values are kept as floats internally per frame and stored rounded to the
   nearest 8-bit value.
6. **Gradient paints** draw nothing in this spec (rule 6 is removed by the gradient spec).
7. **Clips.** `PushClip` intersects the current clip with its rect (in pixel space); `PopClip` restores
   the previous one. A frame starts with the whole target as its clip. An empty clip draws nothing.
8. The contract suite of SPEC-0020 passes for `SoftwareRenderBackend`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Invalid description, unknown id | As the port (SPEC-0020) |
| 0×0 target | Images 0×0; submit and present succeed |
| Shape partly or wholly outside the target | Clipped to the target |
| Colour component outside [0, 1] or NaN | Clamped (NaN → 0) |
| Border wider than the rect allows | Already limited by the `Painter` (SPEC-0019); a hand-built list is drawn as given, coverage floored at 0 |

### 4.4 Threading and lifetime
UI thread, as the port. Each target owns its two images.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (capture is spec 5).

## 5. Acceptance Criteria
Colours: red (1, 0, 0, 1), blue at half alpha (0, 0, 1, 0.5). "±1" means each 8-bit channel may differ by
at most 1 (floating-point contraction can differ between compilers, §10.3); other values are exact.

- **AC-1** — `SoftwareRenderBackend` passes the `RenderBackend` contract suite (5 tests).
- **AC-2** — A 4×3 DIP target at scale 2 has 8×6 images, all transparent; after `resizeTarget` to
  5×5 at scale 1 they are 5×5; `frontBuffer` stays transparent until `present`, then equals
  `backBuffer`.
- **AC-3** — On a 4×3 target at scale 1, `FillRect((1, 1) 2×1, red)` makes pixels (1, 1) and (2, 1)
  exactly (255, 0, 0, 255) and every other pixel (0, 0, 0, 0). At scale 2, pixels x 2–5, y 2–3 are red.
  A second submit with an empty list gives a fully transparent back buffer.
- **AC-4** — Anti-aliasing: on a 4×1 target, `FillRect((0, 0) 1.5×1, red)` gives pixel 0
  (255, 0, 0, 255) and pixel 1 (255, 0, 0, 128) ±1. A rect from x = 0.25 to 0.75 gives pixel 0 alpha
  128 ±1.
- **AC-5** — Blending: half-alpha blue over opaque red gives (128, 0, 128, 255) ±1; over a transparent
  pixel it gives (0, 0, 255, 128) ±1.
- **AC-6** — Rounded rect: a 10×10 target with `FillRoundedRect((0, 0) 10×10, all(5), red)` has opaque
  centre pixels (4, 4)–(5, 5), transparent corner pixel (0, 0), pixel (0, 4) with alpha between 1 and
  254, and is symmetric under horizontal and vertical mirroring. With radii 0, the result equals that of
  `FillRect` with the same rect.
- **AC-7** — Border: `StrokeBorder((0, 0) 10×10, radii 0, width 2, red)` makes pixels with x or y in
  {0, 1, 8, 9} opaque red and pixels (2..7, 2..7) transparent.
- **AC-8** — Clips: with `PushClip((0, 0) 2×2)` then a full-target `FillRect`, only pixels (0..1,
  0..1) are red; nested `PushClip((1, 1) 5×5)` limits drawing to pixel (1, 1); after `PopClip` the outer
  clip applies again; a `PushClip` with an empty rect draws nothing until popped. A `FillRect` with a
  gradient paint leaves the image unchanged.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `RenderBackendContract/Software.*` (5 tests) | unit | contract suite from SPEC-0020 |
| AC-2 | `SoftwareRenderBackend.AC2_ImagesFollowTargetSize` | unit | |
| AC-3 | `SoftwareRenderBackend.AC3_FillsPixelAlignedRects` | unit | |
| AC-4 | `SoftwareRenderBackend.AC4_AntiAliasesFractionalEdges` | unit | |
| AC-5 | `SoftwareRenderBackend.AC5_BlendsSourceOver` | unit | |
| AC-6 | `SoftwareRenderBackend.AC6_RoundsCorners` | unit | |
| AC-7 | `SoftwareRenderBackend.AC7_StrokesBordersInside` | unit | |
| AC-8 | `SoftwareRenderBackend.AC8_ClipsNestAndRestore` | unit | |

Plus `Image` basics (size, transparent fill, `pixel` out of range → nullopt) inside AC-2's test.

## 7. Non-Functional Requirements
- Deterministic: the same input gives the same image on the same build.
- No allocation per pixel; per-command work is bounded by the command's bounding box intersected with
  the clip.

## 8. Architectural Impact
- New adapter `backends/render_software` (`coil::render_software`), depending on `render` (and
  `foundation`) only. **New layering entry** `"render_software": ["render"]` under `backends` in
  `tools/layering.json`; it adds no path through the architecture (a backend depending on its port
  module, like `platform_sdl3`), so this spec's approval covers it (CLAUDE.md §1).
- `render` gains `Image` and `Pixel`.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares), including Q1 (EPIC-006 spec 3 split into 3a and 3b) and Q6 (layering entry).
- **Q1 — Splitting EPIC-006 spec 3.** *Recommended:* this spec covers solid shapes, borders and clips;
  a new spec, "`render_software`: linear, radial and sweep gradients", follows it (EPIC-006 table: 3 →
  3a and 3b). Gradients bring their own decisions (interpolation, extend modes, dithering) and would push
  this spec past 8 criteria.
- **Q2 — Colour space for blending.** *Recommended: sRGB-encoded space*, as browsers, CSS and most
  design tools do, so colours match what designers expect. Linear-light blending is more physically
  correct (anti-aliased edges look slightly thinner in sRGB space) and can be added later as an option.
- **Q3 — Anti-aliasing.** *Recommended:* analytic coverage (exact area for rects and clips, a
  distance-based 1-pixel ramp for rounded edges) rather than supersampling: exact, cheap and
  deterministic.
- **Q4 — `Image` format.** *Recommended:* RGBA8, straight alpha, sRGB-encoded, top row first, in
  `render`, so capture (spec 5) and presentation (spec 4) can use it without depending on this backend.
  Frames start transparent; the application paints its own background.
- **Q5 — Test strategy.** *Recommended:* exact pixel assertions on tiny targets now (±1 per channel on
  partially covered pixels). Golden image files come with capture (spec 5), when there is a format to
  store them in.
- **Q6 — Where it lives.** *Recommended:* `backends/render_software`, as an adapter of the
  `RenderBackend` port with its own layering entry (§8).
- **Q7 — Gradient paints until the next spec.** *Recommended: draw nothing* (rule 6), rather than a
  placeholder colour that could be mistaken for real output.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0021): …`):
   - **Wiring:** `backends/render_software` (CMake, `coil_add_backend`), `tools/layering.json` entry,
     root `add_subdirectory`.
   - **Stubs:** `Image` complete except that the constructor leaves the image 0×0; `SoftwareRenderBackend`
     whose calls all succeed with id 0 and whose buffers are empty images.
   - **Tests:** `backends/render_software/tests/software_render_backend_test.cpp` (contract
     instantiation, AC-2–AC-8). Lint the test code.
   - **Expected Red:** all 12 tests (5 contract + 7) fail on assertions; the other 182 pass.
2. **Green:** `Image`; target bookkeeping (as the recording backend); the rasterizer: per command, the
   pixel bounding box ∩ clip, coverage per rule 4, blending per rule 5, clip stack per rule 7.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-006 spec table (3a/3b).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/render/include/coil/render/image.hpp`, `src/image.cpp` | create | `Pixel`, `Image` |
| `backends/render_software/{CMakeLists.txt, include/coil/render_software/software_render_backend.hpp, src/software_render_backend.cpp, src/rasterizer.{hpp,cpp}}` | create | Adapter and rasterizer |
| `backends/render_software/tests/{CMakeLists.txt, .clang-tidy, software_render_backend_test.cpp}` | create | Tests |
| `CMakeLists.txt`, `modules/render/CMakeLists.txt` | modify | New targets |
| `tools/layering.json` | modify | `render_software` entry |
| `docs/epics/EPIC-006-first-pixels.md` | modify | Spec table (3a/3b) |

### 10.3 Risks / alternatives considered
- **Cross-compiler float differences** (fused multiply-add) can move a partially covered pixel by one
  8-bit step, hence the ±1 tolerance there; fully covered and empty pixels are exact.
- **Speed:** a straightforward per-pixel loop is fast enough for examples (EPIC-006's goal); the production
  path is Vulkan (ADR-0009 Amendment 1).

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `RenderBackendContract/Software.*`
- [x] AC-2 → `SoftwareRenderBackend.AC2_ImagesFollowTargetSize`
- [x] AC-3 → `SoftwareRenderBackend.AC3_FillsPixelAlignedRects`
- [x] AC-4 → `SoftwareRenderBackend.AC4_AntiAliasesFractionalEdges`
- [x] AC-5 → `SoftwareRenderBackend.AC5_BlendsSourceOver`
- [x] AC-6 → `SoftwareRenderBackend.AC6_RoundsCorners`
- [x] AC-7 → `SoftwareRenderBackend.AC7_StrokesBordersInside`
- [x] AC-8 → `SoftwareRenderBackend.AC8_ClipsNestAndRestore`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q7 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: new adapter `backends/render_software` and layering entry; all 12 new tests (5 contract + 7) fail on assertions (stubs: `Image`'s constructor leaves it 0×0; the backend succeeds everywhere with id 0 and empty buffers); the other 182 pass. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green + Refactor**: `Image` storage; target bookkeeping with back and front buffers of `ceil(size × scale)` pixels; a per-frame canvas of premultiplied floats with a clip stack, exact area coverage for rects and clips, signed-distance coverage for rounded rects and borders (outer minus inner), source-over blending, and conversion to straight 8-bit sRGB on output. `tools/check.sh` exit 0 (194/194); `ctest --preset asan` 194/194. `git diff 470785b -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
| 2026-11-03 | Acknowledged; Status → Done | Emilio Pomares |
