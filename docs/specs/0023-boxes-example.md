# SPEC-0023: `boxes` example: coloured boxes laid out and redrawn on resize (Milestone 1)

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `examples/boxes` only (new) |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 6) |
| Related ADRs | ADR-0004 (the example is a composition root), ADR-0006 (DIPs vs pixels), ADR-0009 Amendment 1, ADR-0019 (layout) |
| Depends on | SPEC-0018 (`Row`, `Column`, `weight`), SPEC-0021 (software renderer), SPEC-0022 (`presentImage`) |

> **Scope Gate checklist**
> - [x] One concern: an example window shows coloured boxes laid out by rows and columns, redrawn when the window changes size or scale
> - [x] Every acceptance criterion is checkable by an automated test (plus a recorded manual check)
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 area (`examples/boxes`); no library change, no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
Each piece of Milestone 1 now exists on its own: layout (EPIC-004), the software renderer (SPEC-0021)
and presentation in windows (SPEC-0022). This spec joins them in a program you can run: a window with
a header, a sidebar, two content panels and a footer, which re-lay out and redraw as you resize the
window or move it to a display with a different density. It is also the first test that drives the
whole pipeline (layout → `DrawList` → pixels → window) end to end.

## 2. Scope
- New example `examples/boxes`. As with `hello_window`, the app logic is a library (`boxes.hpp/.cpp`)
  so tests can drive it, and `main.cpp` is the only place that picks SDL3 and the software renderer.
- An example-local leaf `ColoredBox : layout::LayoutElement` that remembers its colour.
- Tests: B1–B7 on the fake window system and SDL's `dummy` driver, plus the build gate (B8).

## 3. Out of Scope
- Input of any kind; hover or click (EPIC-007, EPIC-008).
- Text (EPIC-010), gradients (EPIC-006 spec 3b), rounded corners or borders in the example.
- Golden-image files (they come with capture, spec 5; §9 Q5).
- A frame scheduler, continuous redraw or vsync (EPIC-003).
- Any change to `hello_window`.

## 4. Behavior Specification

### 4.1 Public API (example only)
```cpp
// examples/boxes/boxes.hpp
namespace boxes {

inline constexpr std::string_view kTitle = "Coil boxes";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};
inline constexpr coil::platform::WindowSize kMinSize{.width = 320, .height = 240};
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};

/// The colours of the scene (opaque, sRGB).
inline constexpr coil::render::Color kBackground = /* #1E1E24 */;
inline constexpr coil::render::Color kHeader     = /* #3B82F6 */;
inline constexpr coil::render::Color kSidebar    = /* #10B981 */;
inline constexpr coil::render::Color kTopPanel   = /* #F59E0B */;
inline constexpr coil::render::Color kBottomPanel= /* #EF4444 */;
inline constexpr coil::render::Color kFooter     = /* #8B5CF6 */;

/// Opens one resizable window, draws the scene (§4.2) and redraws it whenever the window's size or
/// display scale changes, until the user asks to close it. Writes
/// "boxes: frame <pw>x<ph> at scale <s>" to `log` for each frame drawn. Returns 0 after the window
/// was closed, or 1 if the window or render target could not be created (error on stderr).
[[nodiscard]] int runBoxes(coil::platform::WindowSystem& windows,
                           coil::render_software::SoftwareRenderBackend& renderer,
                           std::ostream& log);

} // namespace boxes
```

### 4.2 The scene
All sizes in DIPs. The root is a `Column` filling the window, with `padding(8)`, spacing 8 and
`CrossAxisAlignment::Stretch`:
1. **Header**: a `ColoredBox(kHeader)` with height 48 (full width).
2. **Body**: a `Row` with `weight(1)`, spacing 8, `Stretch`:
   - **Sidebar**: `ColoredBox(kSidebar)`, width 160 (full height);
   - **Content**: a `Column` with `weight(1)`, spacing 8, `Stretch`: **top panel**
     `ColoredBox(kTopPanel)` with `weight(1)`, **bottom panel** `ColoredBox(kBottomPanel)` with
     `weight(2)`.
3. **Footer**: a `ColoredBox(kFooter)` with height 32 (full width).

Everything not covered by a box is `kBackground`. At 800×600 this gives (x, y, width × height):

| Box | Rect at 800×600 | Rect at 400×300 |
|-----|-----------------|-----------------|
| Header | (8, 8) 784×48 | (8, 8) 384×48 |
| Sidebar | (8, 64) 160×488 | (8, 64) 160×188 |
| Top panel | (176, 64) 616×160 | (176, 64) 216×60 |
| Bottom panel | (176, 232) 616×320 | (176, 132) 216×120 |
| Footer | (8, 560) 784×32 | (8, 260) 384×32 |

### 4.3 Behavioral rules
1. `runBoxes` creates the window (`kTitle`, `kSize`, resizable, minimum `kMinSize`) and a render target
   with `{size = window size, scale = displayScale}`, then draws the first frame before its first
   event call.
2. **Drawing a frame:** lay out the scene at the window's logical size (`layoutRoot`); record a
   `DrawList`: one `FillRect` of the whole window in `kBackground`, then one `FillRect` per box in
   the order of §4.2; `resizeTarget` to the current size and scale; `submit`; `present`; then
   `presentImage(window, frontBuffer(target))`; then write the frame line with the target's pixel
   size and the scale (formatted as in SPEC-0014: `1`, `1.5`, `2`).
3. **Redraw:** after each batch of events from `waitEvents(kIdleTimeout)`, if the batch contains at
   least one `Resized` or `ScaleChanged` for its window, draw exactly one frame. A batch without
   them, or a timeout, draws nothing.
4. **Close:** on `CloseRequested` for its window (after any redraw for the same batch), destroy the
   target and the window and return 0. Failures to destroy go to stderr and do not change the exit
   code (as in SPEC-0007).
5. **Errors:** if the window or the target cannot be created, report on stderr and return 1, without
   drawing. If any step of drawing a frame fails, report on stderr, skip the rest of that frame
   (no frame line) and keep running.

### 4.4 Threading and lifetime
UI thread only. The scene (layout tree and the list of boxes to paint) is owned by `runBoxes` for the
whole run (§9 Q3).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets yet).

## 5. Acceptance Criteria
"Probe" = the pixel of the window's framebuffer at a point; on the fake that is `framebuffer(id)`,
on SDL the window surface. All colours are opaque, so probes are exact.

- **AC-1 (B1)** — On the fake at 800×600, scale 1: after the first frame the framebuffer is 800×600;
  the centre of each box in §4.2 shows its colour; each box's first and last pixel row and column
  show its colour, and the pixels just outside show `kBackground`; the gaps (4, 4), (172, 300) and
  (400, 228) show `kBackground`.
- **AC-2 (B2)** — After a user resize to 400×300, the framebuffer is 400×300 and matches the
  400×300 column of §4.2 (centre and edge probes as in AC-1).
- **AC-3 (B3)** — After a display-scale change to 2 (at 800×600), the framebuffer is 1600×1200 and
  every probe of AC-1 holds at doubled coordinates (e.g. header from (16, 16) to (1583, 111)).
- **AC-4 (B4)** — The log has one frame line at start (`boxes: frame 800x600 at scale 1`), one per
  batch containing a `Resized` or `ScaleChanged` for its window (a resize and a scale change in the
  same batch give one line), and none for empty batches, timeouts or events of other windows.
- **AC-5 (B5)** — On `CloseRequested`, `runBoxes` returns 0, and afterwards the window system has no
  windows and `frontBuffer(target)` fails with `NotFound`.
- **AC-6 (B6)** — If the window cannot be created (a test double that fails), `runBoxes` returns 1
  and writes no frame line.
- **AC-7 (B7)** — On SDL's `dummy` driver with the software renderer, at the first wait the window
  surface shows `kHeader` at (400, 32) and `kBackground` at (4, 4); after a pushed close request,
  `runBoxes` returns 0.
- **AC-8 (B8)** — The `boxes` executable is part of the default build.

**Manual check (recorded in the Change Log, not automated):** run `build/dev/examples/boxes/boxes`
on a Retina Mac; the frame line says `1600x1200 at scale 2`, edges are sharp, and the boxes follow
live resizing down to the minimum size.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `Boxes.B1_FirstFrameShowsTheScene` | unit | Fake; close from the wait hook |
| AC-2 | `Boxes.B2_ResizeRedrawsAtTheNewLayout` | unit | `userResize` in the wait hook |
| AC-3 | `Boxes.B3_ScaleChangeRedrawsSharp` | unit | `changeDisplayScale` in the wait hook |
| AC-4 | `Boxes.B4_OneFramePerChangingBatch` | unit | Fake; scripted batches, including another window's events |
| AC-5 | `Boxes.B5_CloseReleasesWindowAndTarget` | unit | |
| AC-6 | `Boxes.B6_WindowFailureReturnsOne` | unit | Failing `WindowSystem` double |
| AC-7 | `Boxes.B7_RunsOnSdlDummyDriver` | integration | Decorator reads the surface on first wait, then pushes a close event |
| AC-8 | `gate.examples.AC8_BoxesBuilds` | build | As `hello_window`'s AC6 |

Probe points are computed in the test from the §4.2 table (written out, not from the example's
layout code), so a layout bug cannot make the test agree with itself.

## 7. Non-Functional Requirements
- Deterministic: no wall-clock waits in tests (fake clock; the SDL test pushes its close event).
- No library changes: everything is example code.

## 8. Architectural Impact
None. The example links `layout`, `render`, `render_software`, `platform` and (in `main.cpp` and the
SDL test only) `platform_sdl3`. Examples are composition roots (ADR-0004) and are outside
`tools/layering.json`.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — New example or extend `hello_window`?** *Recommended:* a new `examples/boxes`, leaving
  `hello_window` as the minimal window example its specs describe.
- **Q2 — Which renderer type the example takes.** *Recommended:* `SoftwareRenderBackend&`
  concretely, because only it has `frontBuffer` until capture (spec 5) adds a port-level way to read
  frames. `main.cpp` is still the only place that constructs backends.
- **Q3 — Finding the boxes to paint.** `Row`/`Column` do not expose their children, and there is no
  widget tree yet. *Recommended:* the scene keeps a list of non-owning pointers to its `ColoredBox`
  leaves, owned by the same scene object, so they never outlive the tree. The widget tree (EPIC-008)
  replaces this.
- **Q4 — Redraw policy.** *Recommended:* one frame per batch that changes size or scale (rule 3);
  nothing on timeouts. Continuous redraw belongs to EPIC-003.
- **Q5 — "Golden-image test" in EPIC-006's Done-when.** *Recommended:* exact pixel probes now (B1–B3);
  a stored golden image is added by spec 5 (capture), which defines the file format. The epic's
  Done-when gets a note saying so.
- **Q6 — The scene and colours** in §4.2. *Recommended:* as written; it exercises fixed sizes,
  weights, nested rows and columns, padding, spacing and stretch.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0023): …`):
   - **Stubs:** `boxes.hpp` as in §4.1; `runBoxes` creates the window, returns 0 on the first
     `CloseRequested` and draws nothing, and returns 1 if creation fails. `main.cpp`, CMake targets
     and `ColoredBox` declared.
   - **Tests:** B1–B8.
   - **Expected Red:** B1–B5 and B7 fail on assertions (black or missing frame, no frame lines,
     target not destroyed). B6 and B8 pass with the stubs, since a stub must create a window and the
     executable must build; they are kept as regression tests and listed as such in the Red output.
2. **Green:** `ColoredBox`; scene construction; frame drawing; redraw policy; error handling.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; `leaks` on the example tests; manual check
   on a Retina display.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `examples/boxes/{boxes.hpp, boxes.cpp, main.cpp, CMakeLists.txt}` | create | The example |
| `examples/boxes/tests/{boxes_test.cpp, CMakeLists.txt}` | create | B1–B8 |
| `CMakeLists.txt` | modify | `add_subdirectory(examples/boxes)` |
| `docs/epics/EPIC-006-first-pixels.md` | modify | Spec table; Done-when note (Q5) |

### 10.3 Risks / alternatives considered
- **macOS live resize:** SDL may deliver resizes only when the drag ends unless the app redraws from an
  event watch. If the manual check shows that, it is recorded and handled by EPIC-003 (the main loop),
  not worked around here.
- **Per-pixel `SDL_WriteSurfacePixel`** (SPEC-0022) may make large Retina frames slow to present. The
  manual check records how it feels; a fast path is a follow-up spec if needed.

## 11. Definition of Done
- [x] Red phase: new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the example tests
- [x] Manual check recorded
- [x] Every AC ticked below with its proving test

### AC verification
- [x] AC-1 → `Boxes.B1_FirstFrameShowsTheScene`
- [x] AC-2 → `Boxes.B2_ResizeRedrawsAtTheNewLayout`
- [x] AC-3 → `Boxes.B3_ScaleChangeRedrawsSharp`
- [x] AC-4 → `Boxes.B4_OneFramePerChangingBatch`
- [x] AC-5 → `Boxes.B5_CloseReleasesWindowAndTarget`
- [x] AC-6 → `Boxes.B6_WindowFailureReturnsOne`
- [x] AC-7 → `Boxes.B7_RunsOnSdlDummyDriver`
- [x] AC-8 → `gate.examples.AC8_BoxesBuilds`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q6 approved as recommended | Emilio Pomares |
| 2026-11-07 | Red: B1–B5 and B7 fail on assertions (black frames, no frame lines, window not released, SDL surface black); B6 and B8 pass with the stub, as planned (regression tests); the other 203 pass | — |
| 2026-11-07 | Green (`7b8cbe7`): 211/211 on `dev` and `asan`; tests unchanged since Red (`c8e0918`); all gates pass; `leaks`: 0 leaks | — |
| 2026-11-07 | Manual check by the user on macOS: the scene renders correctly on screen ("it's pretty"). The scale line, live-resize behaviour and resize speed were not reported; they are re-checked with EPIC-003 and EPIC-013. Status → Done | Emilio Pomares |
