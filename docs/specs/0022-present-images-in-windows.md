# SPEC-0022: Presenting images in windows

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-03 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3`. Prerequisite refactor: `Image` moves from `render` to `foundation` (§9 Q2) |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 4) |
| Related ADRs | ADR-0009 Amendment 1 (presentation decided by the first spec that needs it), ADR-0004, ADR-0006 (pixels only in backends), ADR-0018 (SDL3) |
| Depends on | SPEC-0013 (`pixelSize`, high-density windows), SPEC-0021 (`Image`, software renderer) |

> **Scope Gate checklist**
> - [x] One concern: a window can show an image, pixel for pixel
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 areas (`platform`, `backends/platform_sdl3`), after a behaviour-free refactor of `Image`'s location (§9 Q2); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
The software renderer produces images in memory (SPEC-0021), but nothing reaches the screen. This spec
lets a window show an `Image`, pixel for pixel, at the window's full pixel density. The application
(the composition root) connects the two: render a frame, take the renderer's front buffer, and present
it in the window. ADR-0009 Amendment 1 left the presentation mechanism to this spec, with an ADR
amendment only if it adds a dependency edge. The recommended design (§9 Q1) adds none: it extends the
`WindowSystem` port, so the renderer stays window-agnostic and the window system stays
renderer-agnostic.

## 2. Scope
- **Prerequisite refactor:** `Image` and `Pixel` move from `render` to `foundation`
  (`coil/foundation/image.hpp`); `coil/render/image.hpp` keeps `coil::render::Image` and
  `coil::render::Pixel` as aliases, so existing code compiles unchanged. SPEC-0021's test file changes
  only in where its two `operator<<` printers live: they move to `coil::foundation`, where GoogleTest
  finds them by ADL (amendment A); no test or assertion changes.
- Port: `WindowSystem::presentImage(WindowId, const Image&)`.
- Fake: implements it, plus the test control `framebuffer(id)` (what the window shows).
- Contract suite: new tests PI1–PI4, and a trait `framebuffer(system, id)` per implementation.
- SDL3 adapter: presents through the window surface (`SDL_GetWindowSurface`, `SDL_UpdateWindowSurface`).

## 3. Out of Scope
- Choosing when to render and present (the app loop, EPIC-003; for now examples do it themselves).
- Scaling or stretching images; vsync; partial updates.
- GPU presentation (Vulkan/OpenGL swapchains need native window handles; their own ADR and epic).
- Frame capture (spec 5); the Milestone 1 example (spec 6).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/foundation/include/coil/foundation/image.hpp  (moved from render, unchanged otherwise)
namespace coil::foundation { struct Pixel { … }; class Image { … }; }
namespace coil { using foundation::Image; using foundation::Pixel; }

// modules/render/include/coil/render/image.hpp  (now only aliases)
namespace coil::render { using foundation::Image; using foundation::Pixel; }
```
```cpp
// modules/platform/include/coil/platform/window_system.hpp (addition)
class WindowSystem {
public:
    // … existing members …

    /// Shows `image` in the window, pixel for pixel from the top-left corner, at the window's
    /// pixel size (pixelSize()). Each pixel is composited over opaque black; window pixels the
    /// image does not cover become opaque black; image pixels beyond the window are cropped.
    /// Fails with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<void> presentImage(WindowId id, const Image& image) = 0;
};
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp (additions)
    [[nodiscard]] Expected<void> presentImage(WindowId id, const Image& image) override;

    /// Test control: what the window shows, at its pixel size. Opaque black before any present.
    /// NotFound for an unknown window.
    [[nodiscard]] Expected<Image> framebuffer(WindowId id) const;
```

### 4.2 Behavioral rules
1. After `presentImage(id, image)`, the window's pixel at `(x, y)` (for `x < pixelWidth`,
   `y < pixelHeight`) is: if `(x, y)` is inside `image`, that pixel composited over opaque black,
   `rgb = round(rgb × a / 255)`, `a = 255`; otherwise `(0, 0, 0, 255)`.
2. `presentImage` with an unknown or destroyed window fails with `NotFound` and shows nothing.
3. Presenting a 0×0 image makes the whole window black.
4. **Fake:** `framebuffer(id)` is an image of the window's pixel size. It starts opaque black, is set
   by `presentImage` (rule 1), and becomes opaque black again whenever the pixel size changes (resize or
   scale change), since the window's previous contents no longer fit.
5. **SDL:** the adapter writes into the window surface, which has the window's pixel size, and calls
   `SDL_UpdateWindowSurface`. Failures from SDL are reported as `Internal`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Unknown or destroyed window | `NotFound` |
| Image smaller than the window | Rest of the window black |
| Image larger than the window | Cropped |
| 0×0 image | Window black |
| SDL surface failure | `Internal` |

### 4.4 Threading and lifetime
UI thread, as the rest of the port. The window system does not keep a reference to `image`.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (capture is spec 5).

## 5. Acceptance Criteria
Contract tests run on the fake and on SDL's `dummy` driver (scale 1). "±1" allows one 8-bit step on
composited channels.

- **AC-1 (PI1, contract)** — Presenting a 4×3 image of opaque pixels with distinct colours on a 4×3
  window makes `framebuffer` equal to that image.
- **AC-2 (PI2, contract)** — A pixel (255, 0, 0, 128) shows as (128, 0, 0, 255) ±1; a fully
  transparent pixel shows as (0, 0, 0, 255).
- **AC-3 (PI3, contract)** — A 2×2 image on a 4×3 window shows its pixels at (0..1, 0..1) and black
  elsewhere; a 6×5 image on the same window shows only its top-left 4×3 pixels; a 0×0 image makes the
  window black.
- **AC-4 (PI4, contract)** — `presentImage` on a window id that never existed, and on a destroyed one,
  fails with `NotFound`.
- **AC-5 (fake)** — `framebuffer` of a new window is opaque black of its pixel size; at display scale 2
  (`changeDisplayScale`) a 4×3 window's framebuffer is 8×6, and becomes black again after the change;
  `framebuffer` of an unknown id fails with `NotFound`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `WindowSystemContract/{Fake,Sdl3}.PI1_PresentsImagePixelForPixel` | unit / integration | new trait `framebuffer` |
| AC-2 | `…PI2_CompositesOverBlack` | unit / integration | |
| AC-3 | `…PI3_CropsAndFillsBlack` | unit / integration | |
| AC-4 | `…PI4_UnknownWindowsAreNotFound` | unit / integration | |
| AC-5 | `FakeWindowSystem.FS3_FramebufferFollowsPixelSize` | unit | |

The refactor (§9 Q2) gets no new test: a test that cannot fail in Red proves nothing. It is proven by
its own commit, in which every existing test (including SPEC-0021's, which use `coil::render::Image`)
passes; the only test-file change is amendment A's move of two printers.

The SDL trait reads the window surface back (`SDL_GetWindowSurface`, `SDL_ReadSurfacePixel`), the same way
the SDL trait already reaches the window for `userResize`.

## 7. Non-Functional Requirements
- One copy per present (image → window surface); no per-frame allocation in the SDL adapter beyond
  what SDL does.

## 8. Architectural Impact
- **No new layering edge**: `foundation` gains `Image` (used by both `platform` and `render`), and
  `platform`'s port gains one function. ADR-0009 Amendment 1's open point (the presentation mechanism) is
  resolved here; its text gets a one-line pointer to this spec.
- Public API: `presentImage` added to the port (all implementations updated); `Image`/`Pixel` move
  namespace with aliases kept.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — How frames reach windows.** *Recommended:* `WindowSystem::presentImage(WindowId, const Image&)`,
  with the app copying the renderer's front buffer into it. No new edge, the renderer stays
  window-agnostic, and the fake makes it testable. Alternative: an adapter that depends on both
  `render_software` and SDL (a backend-to-backend edge, needing an ADR), which saves one copy per frame.
  GPU backends will need native window handles anyway; that is a separate decision when they arrive.
- **Q2 — Moving `Image` to `foundation`.** *Recommended:* yes, as a separate behaviour-free refactor
  commit before Red, with aliases in `render` (SPEC-0021's tests unchanged). `platform` cannot depend on
  `render`, and a second image type in `platform` would duplicate it. This is why the spec touches more
  than two modules; the refactor changes no behaviour, so I propose counting it outside the gate.
- **Q3 — SDL mechanism.** *Recommended: the window surface* (`SDL_GetWindowSurface`). It is plain CPU
  memory at pixel size, supported by the `dummy` driver (so CI can read it back), and needs no renderer.
  Alternative: `SDL_Renderer` with a streaming texture (GPU scaling and vsync), which can replace it later
  without changing the port.
- **Q4 — Transparency.** *Recommended:* composited over opaque black (rule 1). Applications paint their
  own background; black makes a missing background obvious.
- **Q5 — Size mismatches** (for example, presenting a frame rendered before a resize). *Recommended:*
  pixel for pixel from the top-left, cropped or filled with black, never scaled; the next frame at the
  new size fixes it.
- **Q6 — Order of EPIC-006's remaining specs.** *Recommended:* after this, spec 6 (the Milestone 1
  example) before spec 5 (capture) and 3b (gradients), since only spec 6 is needed to see the milestone.

---

## 10. Implementation Plan

### 10.1 Steps
0. **Refactor** (commit `refactor(0022): move Image and Pixel to foundation`): move the header and source,
   leave aliases in `render`; move SPEC-0021's two test printers to `coil::foundation` (amendment A);
   full suite green (194/194), no test or assertion changed.
1. **Red** (commit `test(0022): …`):
   - **Stubs:** `presentImage` in the port; the fake and SDL adapter return success without drawing; the
     fake's `framebuffer` returns a 0×0 image.
   - **Tests:** PI1–PI4 in the contract suite with the `framebuffer` trait (fake and SDL); FS3.
     Lint the test code.
   - **Expected Red:** PI1–PI4 fail on both implementations (8) and FS3 fails, all on assertions; the
     other 194 pass.
2. **Green:** fake framebuffer per window (reset on pixel-size change); compositing helper; SDL window
   surface writes (`SDL_LockSurface` if needed, pixel format conversion through `SDL_MapSurfaceRGBA`).
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; macOS `leaks` on the SDL tests; manual check:
   none yet (spec 6 shows pixels in `hello_window`'s successor).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/foundation/{include/coil/foundation/image.hpp, src/image.cpp}` | create (moved) | `Image`, `Pixel` |
| `modules/render/include/coil/render/image.hpp`, `modules/render/src/image.cpp` | modify / delete | Aliases only |
| `modules/foundation/CMakeLists.txt`, `modules/render/CMakeLists.txt` | modify | Sources |
| `modules/platform/include/coil/platform/window_system.hpp` | modify | `presentImage` |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | `presentImage`, `framebuffer` |
| `modules/platform/tests/contract/coil/platform/window_system_contract.hpp` | modify | PI1–PI4, trait |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | Trait, FS3 |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | Window-surface presentation |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | Trait reading the surface |
| `docs/adr/0009-rendering-abstraction.md` | modify | Pointer: presentation resolved by SPEC-0022 |
| `docs/epics/EPIC-006-first-pixels.md` | modify | Spec table, order |

### 10.3 Risks / alternatives considered
- **Adding test cases to the frozen contract header:** only new `TYPED_TEST_P`s and a trait are added; the
  existing tests are unchanged, which the Red diff shows.
- **macOS window surfaces** may be emulated by SDL with a renderer internally; that is SDL's business and
  stays behind the port. The manual check in spec 6 confirms it on screen.

## 11. Definition of Done
- [x] Refactor commit: only the printers move in tests (amendment A), full suite green
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL tests
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `WindowSystemContract/{Fake,Sdl3}.PI1_PresentsImagePixelForPixel`
- [ ] AC-2 → `WindowSystemContract/{Fake,Sdl3}.PI2_CompositesOverBlack`
- [ ] AC-3 → `WindowSystemContract/{Fake,Sdl3}.PI3_CropsAndFillsBlack`
- [ ] AC-4 → `WindowSystemContract/{Fake,Sdl3}.PI4_UnknownWindowsAreNotFound`
- [ ] AC-5 → `FakeWindowSystem.FS3_FramebufferFollowsPixelSize`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-03 | Spec and implementation plan created (Draft) | |
| 2026-11-05 | Spec, plan and Q1–Q6 approved as recommended | Emilio Pomares |
| 2026-11-05 | **Amendment A:** moving `Image` hides SPEC-0021's test printers from ADL (compile error); the refactor moves them to `coil::foundation` without changing any test | Emilio Pomares |
| 2026-11-05 | Red: 9 new tests fail on assertions (PI1–PI4 on Fake and Sdl3, FS3), 194 others pass. Fake shows a 0x0 framebuffer; SDL's dummy surface reads back 4x3 opaque black; `presentImage` succeeds for unknown windows. `hello_window`'s two test doubles of the port gain the new override (forwarding / `NotFound`), per §8 "all implementations updated" | — |
