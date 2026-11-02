# SPEC-0019: `render` module: `Color`, `DrawList` and `Painter`

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::render` (new module, already planned in `tools/layering.json` and OVERVIEW §3) |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 1) |
| Related ADRs | ADR-0009 (DrawList, Painter; Amendment 2: closed vocabulary, gradients), ADR-0006 (DIPs) |
| Depends on | SPEC-0015 (geometry) |

> **Scope Gate checklist**
> - [x] One concern: the vocabulary widgets draw with, recorded as plain data
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`render`, new; its `render` → `foundation` edge is already in `layering.json`)
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
Nothing can be drawn yet. Before any renderer exists, Coil needs the **vocabulary** that widgets draw
with: a `Painter` that records drawing commands into a `DrawList`, which is plain, comparable data
(ADR-0009). Renderers (`render_software` first, then GPU backends), the recording backend, frame
capture and remote frontends all consume that list. This spec covers the first part of the vocabulary
that EPIC-006 needs: filled rectangles and rounded rectangles, borders, clips, and solid colours or
linear, radial and sweep gradients as fills (ADR-0009 Amendment 2).

## 2. Scope
- `Color`: RGBA.
- Paints: solid `Color`, `LinearGradient`, `RadialGradient`, `SweepGradient`; gradients validated by
  factories.
- `CornerRadii`.
- `DrawList` of commands: `FillRect`, `FillRoundedRect`, `StrokeBorder`, `PushClip`, `PopClip`.
- `Painter`: records into a `DrawList`, normalizes degenerate input, keeps clips balanced.

## 3. Out of Scope
- Rendering anything (`RenderBackend`, `render_recording`, `render_software`: EPIC-006 specs 2–3).
- Colour space and blending rules, gradient interpolation (spec 3 decides; this spec only records).
- Paths, lines, transforms, images (EPIC-012); glyph runs (EPIC-010); `GpuSurface`.
- Rounded clips, opacity layers, shadows.
- Culling commands outside the clip.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/render/include/coil/render/color.hpp
namespace coil::render {

/// Straight (not premultiplied) RGBA in sRGB, each component nominally in [0, 1]. How components are
/// blended and interpolated is the renderer's business (EPIC-006 spec 3).
struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;

    /// From 8-bit components (0-255 -> 0-1).
    [[nodiscard]] static constexpr Color fromRgba8(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                                   std::uint8_t a = 255) noexcept;
    /// From 0xRRGGBBAA.
    [[nodiscard]] static constexpr Color fromHex(std::uint32_t rgba) noexcept;

    friend constexpr bool operator==(Color, Color) = default;
};

} // namespace coil::render
```
```cpp
// modules/render/include/coil/render/paint.hpp
namespace coil::render {

struct GradientStop {
    float offset = 0.0F; ///< In [0, 1].
    Color color;
    friend bool operator==(const GradientStop&, const GradientStop&) = default;
};

/// Colours along the line from `start` to `end` (DIPs, same space as the shapes).
struct LinearGradient {
    Point start;
    Point end;
    std::vector<GradientStop> stops;
    friend bool operator==(const LinearGradient&, const LinearGradient&) = default;
};

/// Colours by distance from `center`, reaching the last stop at `radius`.
struct RadialGradient {
    Point center;
    float radius = 0.0F;
    std::vector<GradientStop> stops;
    friend bool operator==(const RadialGradient&, const RadialGradient&) = default;
};

/// Colours by angle around `center`: one full turn, starting at `startAngle` (radians; 0 points to
/// +x, angles grow clockwise because y grows downwards).
struct SweepGradient {
    Point center;
    float startAngle = 0.0F;
    std::vector<GradientStop> stops;
    friend bool operator==(const SweepGradient&, const SweepGradient&) = default;
};

/// What a shape is filled with.
using Paint = std::variant<Color, LinearGradient, RadialGradient, SweepGradient>;

/// Factories validate gradients (rule 3) and fail with InvalidArgument otherwise.
[[nodiscard]] Expected<Paint> linearGradient(Point start, Point end, std::vector<GradientStop> stops);
[[nodiscard]] Expected<Paint> radialGradient(Point center, float radius,
                                             std::vector<GradientStop> stops);
[[nodiscard]] Expected<Paint> sweepGradient(Point center, float startAngle,
                                            std::vector<GradientStop> stops);

} // namespace coil::render
```
```cpp
// modules/render/include/coil/render/draw_list.hpp
namespace coil::render {

struct CornerRadii {
    float topLeft = 0.0F;
    float topRight = 0.0F;
    float bottomRight = 0.0F;
    float bottomLeft = 0.0F;
    [[nodiscard]] static constexpr CornerRadii all(float radius) noexcept;
    friend constexpr bool operator==(CornerRadii, CornerRadii) = default;
};

struct FillRect { Rect rect; Paint paint; /* == */ };
struct FillRoundedRect { Rect rect; CornerRadii radii; Paint paint; /* == */ };
/// A border of `width` drawn inside `rect` (its outer edge is the rect's edge).
struct StrokeBorder { Rect rect; CornerRadii radii; float width = 0.0F; Paint paint; /* == */ };
/// Intersects the current clip with `rect` until the matching PopClip.
struct PushClip { Rect rect; /* == */ };
struct PopClip { /* == */ };

using DrawCommand = std::variant<FillRect, FillRoundedRect, StrokeBorder, PushClip, PopClip>;

/// Recorded drawing, in painting order. Plain data: copyable, movable, comparable.
class DrawList {
public:
    [[nodiscard]] std::span<const DrawCommand> commands() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void clear() noexcept;
    void append(DrawCommand command); ///< Used by Painter; records as given.
    friend bool operator==(const DrawList&, const DrawList&) = default;
};

} // namespace coil::render
```
```cpp
// modules/render/include/coil/render/painter.hpp
namespace coil::render {

/// Records drawing into a DrawList (ADR-0009). Never fails: degenerate input is normalized or
/// skipped (rules 4-6), and clips are always balanced (rule 7). Not copyable or movable.
class Painter {
public:
    explicit Painter(DrawList& target) noexcept;
    ~Painter(); ///< Closes clips left open.

    void fillRect(Rect rect, const Paint& paint);
    void fillRoundedRect(Rect rect, CornerRadii radii, const Paint& paint);
    void strokeBorder(Rect rect, CornerRadii radii, float width, const Paint& paint);
    void pushClip(Rect rect);
    void popClip();
    [[nodiscard]] std::size_t clipDepth() const noexcept;
};

} // namespace coil::render
```
All in namespace `coil::render`; `Point`/`Rect` are `coil::foundation`'s.

### 4.2 Behavioral rules
1. **`Color`:** `fromRgba8` divides each component by 255; `fromHex(0xRRGGBBAA)` is
   `fromRgba8(RR, GG, BB, AA)`. Default is opaque black.
2. **Commands** record exactly what the painter passes, in call order; `DrawList` equality compares
   every command.
3. **Gradient validity:** at least 2 stops; every offset finite and in [0, 1]; offsets non-decreasing;
   every colour component finite; points and angles finite; `radius` finite and > 0. Otherwise
   `InvalidArgument`. A gradient built directly (not through a factory) is recorded as given.
4. **Skipped shapes:** a rect with a NaN or infinite component, or a width or height ≤ 0, records
   nothing (fill, rounded fill, border and clip alike, see rule 7 for clips).
5. **Corner radii:** each radius that is NaN or negative becomes 0; each is then limited to half the
   rect's smaller side. A `fillRoundedRect` whose radii are all 0 records a `FillRoundedRect` with zero
   radii (no conversion to `FillRect`).
6. **Border width:** NaN, infinite or ≤ 0 records nothing; a width above half the rect's smaller side
   is limited to it.
7. **Clips:** `pushClip` with a skipped rect (rule 4) records `PushClip` with an empty rect at the
   rect's origin (clipping everything), so pushes and pops still pair up. `popClip` with
   `clipDepth() == 0` records nothing. When the painter is destroyed, it records one `PopClip` per clip
   still open. `clipDepth()` is the number of open clips.
8. Coordinates are DIPs in the root's space, like layout (SPEC-0016 rule 1).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Gradient factory with < 2 stops, bad offsets, NaN, or radius ≤ 0 | `InvalidArgument` |
| Empty, negative or non-finite rect | Nothing recorded (clip: empty clip) |
| Negative or NaN radius | 0 |
| Radius larger than half the smaller side | Limited to it |
| Border width ≤ 0, NaN, infinite | Nothing recorded |
| `popClip` without a push | Nothing recorded |
| Painter destroyed with open clips | Matching `PopClip`s recorded |

### 4.4 Threading and lifetime
A `Painter` is used on the UI thread during painting; it must not outlive its `DrawList`. A finished
`DrawList` is plain data and may be handed to another thread (ADR-0010).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1** — `Color{}` is (0, 0, 0, 1); `fromRgba8(255, 0, 51, 102)` is (1, 0, 0.2, 0.4);
  `fromHex(0xFF003366)` equals it; colours compare by value.
- **AC-2** — A painter given `fillRect(r1, red)`, `fillRoundedRect(r2, all(4), blue)` and
  `strokeBorder(r3, all(2), 3, green)` records exactly those three commands in that order, with
  those values.
- **AC-3** — Gradient factories accept valid input and the paint is recorded unchanged in a
  `fillRect`; they fail with `InvalidArgument` for 1 stop, an offset of −0.1, 1.1 or NaN, decreasing
  offsets, a NaN colour component, a NaN point, a radius of 0, −1 or NaN, and a NaN start angle.
- **AC-4** — Rects of width 0, height −1, or with a NaN or infinite component record nothing through
  `fillRect`, `fillRoundedRect` or `strokeBorder`.
- **AC-5** — Radii: for a 40×20 rect, `all(15)` records 10 on every corner; `{-3, NaN, 5, 30}`
  records `{0, 0, 5, 10}`.
- **AC-6** — Border width: 0, −1, NaN and ∞ record nothing; width 15 on a 40×20 rect records 10.
- **AC-7** — Clips: `pushClip(a)`, `pushClip(b)`, `popClip()`, `popClip()`, `popClip()` records
  Push a, Push b, Pop, Pop (the third pop is ignored), and `clipDepth()` goes 1, 2, 1, 0, 0. A painter
  destroyed after two unmatched pushes leaves two `PopClip`s at the end. `pushClip` of a 0×10 rect at
  (5, 5) records `PushClip{(5, 5) 0×0}`.
- **AC-8** — `DrawList`: starts empty; equal when commands are equal; a copy compares equal and is
  independent of later changes; `clear()` empties it.

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used |
|----|-----------|------|---------------------|
| AC-1 | `Color.AC1_ConstructsAndCompares` | unit | — |
| AC-2 | `Painter.AC2_RecordsShapesInOrder` | unit | — |
| AC-3 | `Painter.AC3_GradientsAreValidated` | unit | — |
| AC-4 | `Painter.AC4_DegenerateRectsAreSkipped` | unit | — |
| AC-5 | `Painter.AC5_CornerRadiiAreNormalized` | unit | — |
| AC-6 | `Painter.AC6_BorderWidthIsNormalized` | unit | — |
| AC-7 | `Painter.AC7_ClipsStayBalanced` | unit | — |
| AC-8 | `DrawList.AC8_IsPlainComparableData` | unit | — |

Files: `modules/render/tests/color_test.cpp`, `painter_test.cpp`, `draw_list_test.cpp`.

## 7. Non-Functional Requirements
- `Painter` does not allocate except to append commands and copy gradient stops.
- `DrawList` stores commands contiguously (`std::vector`).

## 8. Architectural Impact
- New module `render` (`modules/render`), depending on `foundation` only, as `tools/layering.json`
  and OVERVIEW §3 already state. No layering change.
- New public types: `Color`, `GradientStop`, `LinearGradient`, `RadialGradient`, `SweepGradient`,
  `Paint`, `CornerRadii`, the five commands, `DrawCommand`, `DrawList`, `Painter`, and the three
  gradient factories.
- The `DrawList` vocabulary is ABI-relevant for backends (ADR-0009 consequences).

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — `Color` representation.** *Recommended:* four `float`s, straight (not premultiplied) sRGB,
  nominally 0–1. Floats leave room for HDR and wide gamut later, and premultiplying is the renderer's
  job. Alternative: four bytes (compact, but lossy for gradients and blending).
- **Q2 — Commands as `std::variant` value types.** *Recommended* (plain, comparable, copyable data, as
  ADR-0009 asks). Alternative: a command class hierarchy (needs allocation and virtual dispatch).
- **Q3 — Gradients as fills.** *Recommended:* a gradient is a `Paint`, usable by every shape (EPIC-006
  left this to spec 1). Alternative: separate gradient commands.
- **Q4 — Invalid drawing input.** *Recommended:* the painter never fails. Degenerate shapes are skipped
  and radii and widths limited (rules 4–6), because painting has no one to report errors to; gradients,
  which carry data, are validated where they are built. Alternative: `Expected` from every painter call.
- **Q5 — Clip balance.** *Recommended:* the painter guarantees it (rule 7), so a widget's mistake can
  never break the clipping of the rest of the window.
- **Q6 — Sweep gradient angles.** *Recommended:* radians, 0 along +x, clockwise on screen (since y grows
  down), one full turn.
- **Q7 — Border placement.** *Recommended:* inside the rect, so a border never makes an element bigger
  than its layout bounds.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0019): …`):
   - **Module:** `modules/render` with `CMakeLists.txt` (`coil_add_module(render … DEPENDS
     coil::foundation)`) and `add_subdirectory` in the root `CMakeLists.txt`.
   - **Stubs:** the headers in §4.1; `fromRgba8`/`fromHex` return `{}`; gradient factories return a
     default `Color` and never fail; `CornerRadii::all` returns `{}`; `DrawList::append` drops the
     command; the painter records nothing; `clipDepth()` is 0.
   - **Tests:** the three test files. Lint the test code.
   - **Expected Red:** all 8 fail on assertions; the other 166 pass.
2. **Green:** per §4.2.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-006 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/render/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/.clang-tidy` | create | Module and tests |
| `modules/render/include/coil/render/{color,paint,draw_list,painter}.hpp` | create | Public API |
| `modules/render/src/{paint,draw_list,painter}.cpp` | create | Implementation |
| `modules/render/tests/{color,painter,draw_list}_test.cpp` | create | AC-1–AC-8 |
| `CMakeLists.txt` | modify | `add_subdirectory(modules/render)` |
| `docs/epics/EPIC-006-first-pixels.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Gradient stops in a `std::vector`** make commands non-trivially copyable. Accepted: the list is
  built once per frame, and a small-buffer optimization can come later if profiling asks for it.
- **Exact float checks:** AC-1's 51/255 = 0.2 and 102/255 = 0.4 are compared with the same `float`
  expressions the implementation uses, so they are exact.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `Color.AC1_ConstructsAndCompares`
- [ ] AC-2 → `Painter.AC2_RecordsShapesInOrder`
- [ ] AC-3 → `Painter.AC3_GradientsAreValidated`
- [ ] AC-4 → `Painter.AC4_DegenerateRectsAreSkipped`
- [ ] AC-5 → `Painter.AC5_CornerRadiiAreNormalized`
- [ ] AC-6 → `Painter.AC6_BorderWidthIsNormalized`
- [ ] AC-7 → `Painter.AC7_ClipsStayBalanced`
- [ ] AC-8 → `DrawList.AC8_IsPlainComparableData`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q7 confirmed as recommended; spec and plan approved | Emilio Pomares |
