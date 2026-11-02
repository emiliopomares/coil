# SPEC-0013: Window pixel size, display scale and the `ScaleChanged` event

| Field | Value |
|-------|-------|
| Status | Approved (amendment C in progress) |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-005](../epics/EPIC-005-window-resizing.md) (spec 3) |
| Related ADRs | ADR-0006 / ADR-0019 (DIPs; pixels only in backends), ADR-0009 (rendering), ADR-0018 (SDL3) |
| Depends on | SPEC-0010 (`Resized`, coalescing) |

> **Scope Gate checklist**
> - [x] One concern: how many physical pixels a window has, the scale between DIPs and pixels, and
>       when that scale changes
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules (`platform`, `backends/platform_sdl3`); `hello_window` test doubles get
>       mechanical overrides
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
To draw sharply (EPIC-006), a renderer needs two numbers per window: its **pixel size** (the size of
the drawable in physical pixels) and its **display scale** (physical pixels per DIP). For example, on a
Retina Mac an 800×600 window has 1600×1200 pixels at scale 2.0. Both change when the window moves to
a display with a different density. This spec adds the two queries and a `ScaleChanged` event.

## 2. Scope
- Port:
  - `pixelSize(id)` and `displayScale(id)`;
  - a `ScaleChanged{window, scale}` event (the third `WindowEvent` alternative), coalesced like
    `Resized`.
- Fake:
  - each window has a scale (initially 1.0), and pixel size = size × scale, rounded up;
  - a test control `changeDisplayScale(id, scale)` simulates moving the window to another display.
- Contract suite: PX1–PX3 on both implementations.
- Fake-only tests FS1–FS2, and SDL-only tests SD1–SD2.
- SDL3 adapter:
  - `SDL_GetWindowSizeInPixels` and `SDL_GetWindowDisplayScale`;
  - `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` maps to `ScaleChanged`.
- `hello_window` test doubles: overrides for the two new pure virtuals.

## 3. Out of Scope
- Rendering at the scale (EPIC-006).
- Making port sizes DIPs on platforms where SDL's window coordinates are pixels (Windows, X11). See
  §9 Q3.
- Per-monitor information (display list, refresh rate, safe areas, content scale on its own).
- `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`: it stays ignored (§9 Q1).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp (additions)
namespace coil::platform {

/// A window's display scale (physical pixels per DIP) changed, e.g. it moved to a display with a
/// different density. Its pixel size may have changed with it; query pixelSize() (SPEC-0013).
struct ScaleChanged {
    WindowId window;
    float scale = 0.0F; ///< The new display scale, > 0.

    friend constexpr bool operator==(ScaleChanged, ScaleChanged) = default;
};

using WindowEvent = std::variant<CloseRequested, Resized, ScaleChanged>;

class WindowSystem {
public:
    // … existing members …

    /// Size of the window's drawable area in physical pixels. Fails with NotFound for an unknown
    /// window.
    [[nodiscard]] virtual Expected<WindowSize> pixelSize(WindowId id) const = 0;

    /// Physical pixels per DIP for the window (e.g. 2.0 on a Retina display). Always > 0. Fails
    /// with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<float> displayScale(WindowId id) const = 0;
};

} // namespace coil::platform
```
```cpp
// FakeWindowSystem additions
    [[nodiscard]] Expected<WindowSize> pixelSize(WindowId id) const override;
    [[nodiscard]] Expected<float> displayScale(WindowId id) const override;

    /// Test control: changes `id`'s display scale as if it moved to another display. Fails with
    /// NotFound for an unknown window and InvalidArgument unless `scale` is finite and > 0.
    [[nodiscard]] Expected<void> changeDisplayScale(WindowId id, float scale);
```

### 4.2 Behavioral rules
1. For a live window, `displayScale` MUST be > 0, and `pixelSize` MUST have both dimensions ≥ the
   corresponding dimension of `size` (a pixel is never larger than a window unit).
2. When the size changes and the scale doesn't, the ratio between `pixelSize` and `size` MUST stay the
   same, once the `Resized` event has been returned.
3. When a window's display scale changes, exactly one `ScaleChanged{id, newScale}` MUST be delivered.
   Once it has been returned, `displayScale(id)` reports `newScale`, and `pixelSize(id)` reflects it.
4. **Coalescing**, the same as SPEC-0010 rule 9: at most one `ScaleChanged` per window is pending. A
   newer one replaces it and goes to the end of the queue. A scale change to the current scale
   produces no event.
5. `ScaleChanged` keeps its order relative to other events and is dropped if its window is destroyed
   before it is returned (SPEC-0006).
6. Unknown or destroyed ids MUST fail with `NotFound` (`pixelSize`, `displayScale`,
   `changeDisplayScale`).
7. Fake:
   - a new window's scale is 1.0;
   - `pixelSize = {ceil(width × scale), ceil(height × scale)}`;
   - `changeDisplayScale` follows rules 3–4, and rejects a scale that isn't finite and > 0 with
     `InvalidArgument`.
8. SDL:
   - `pixelSize` = `SDL_GetWindowSizeInPixels`, and `displayScale` = `SDL_GetWindowDisplayScale`;
   - `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` for a live window maps to `ScaleChanged`, with the
     scale queried when the event is translated;
   - several such events for one window in a batch become one (rule 4);
   - **(amendment C)** the adapter remembers the last scale it reported for each window, starting with
     the scale right after creation, and drops an event whose queried scale equals it (rule 4: no
     event for an unchanged scale). Without this, a Retina window reports a spurious
     `ScaleChanged{2}` at startup, because SDL queues the 1 → 2 change that happens during creation;
   - if the query fails (it returns 0.0), the event is dropped instead of reporting an invalid scale.
9. **SDL high pixel density** (amendment B, 2026-11-02): every SDL window MUST be created with
   `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so it gets a full-density back buffer where the display has one
   (for example, Retina). Without the flag, macOS gives the window a 1× buffer and reports scale 1.
   That's truthful, but anything drawn would be upscaled and blurry. Coil works in DIPs and converts
   only in backends (ADR-0006), so it always wants full density.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Unknown or destroyed id | `NotFound` |
| Fake: scale ≤ 0, NaN or infinite | `InvalidArgument`, no event |
| Scale changed to its current value | No event |
| Two scale changes before polling | One `ScaleChanged` with the last scale |
| SDL reports scale 0.0 (query failure) | Event dropped; `displayScale` fails with `Internal` |

### 4.4 Threading and lifetime
UI/main thread only, like the rest of the port.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable now. Captures (ADR-0017) will report their pixel scale from this.

## 5. Acceptance Criteria
- **AC-1 (PX1, contract)** — A new 100×80 window has `displayScale > 0`, and a `pixelSize` with
  width ≥ 100 and height ≥ 80.
- **AC-2 (PX2, contract)** — After `setSize(200×160)` and its `Resized`, the pixel size has the same
  ratio to the size as before: `oldPixel.w × 200 == newPixel.w × 100`, and the same for the height.
- **AC-3 (PX3, contract)** — `pixelSize` and `displayScale` on an unknown or destroyed id fail with
  `NotFound`.
- **AC-4 (FS1, fake)** — A new fake window has scale 1.0 and pixel size equal to its size. Then:
  - `changeDisplayScale(id, 2.0)` yields exactly `ScaleChanged{id, 2.0}`, after which the scale is
    2.0 and the pixel size is twice the size;
  - `changeDisplayScale(id, 1.5)` on a 101×80 window gives a pixel size of 152×120 (rounded up);
  - changing to the current scale yields no event;
  - `0`, `-1`, NaN and infinity fail with `InvalidArgument`, and an unknown id fails with
    `NotFound`.
- **AC-5 (FS2, fake)** — Two `changeDisplayScale` calls (2.0, then 3.0) before polling yield exactly
  `ScaleChanged{id, 3.0}`. A `ScaleChanged` for a window destroyed before polling is dropped.
- **AC-6 (SD1, SDL)** — On the `dummy` driver a new window's scale is 1.0, and its pixel size equals
  its size.
- **AC-8 (SD3, SDL; amendment B)** — Every window created by the SDL window system, resizable or
  not, has `SDL_WINDOW_HIGH_PIXEL_DENSITY` in `SDL_GetWindowFlags`.
- **AC-7 (SD2, SDL; amended by C)** — Two `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` events pushed for
  one window whose scale has not changed (the `dummy` driver's 1.0) yield **no** event. One pushed for
  an SDL window that Coil doesn't own yields nothing either. (A real scale change through SDL can't be
  produced on `dummy`; it is covered by the manual check.)

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `…/WindowSystemContract.PX1_PixelSizeAndScaleArePositive` | contract (Fake, Sdl3) | |
| AC-2 | `…PX2_PixelSizeFollowsResize` | contract | `waitEvents(1 s)` for the `Resized` |
| AC-3 | `…PX3_UnknownIdsAreNotFound` | contract | |
| AC-4 | `FakeWindowSystem.FS1_DisplayScaleChangesPixelSize` | unit | |
| AC-5 | `FakeWindowSystem.FS2_ScaleChangesAreCoalescedAndDropped` | unit | |
| AC-6 | `SdlWindowSystem.SD1_DummyDriverHasScaleOne` | integration | |
| AC-8 | `SdlWindowSystem.SD3_WindowsUseHighPixelDensity` | integration | amendment B |
| AC-7 | `SdlWindowSystem.SD2_UnchangedScaleEventsAreDropped` | integration | `SDL_PushEvent`; a second SDL window created directly with `SDL_CreateWindow` |

## 7. Non-Functional Requirements
- No new dependency.

## 8. Architectural Impact
- `WindowEvent` gains its third alternative.
- Two new pure virtuals: the fake, SDL and the two `hello_window` test doubles implement them.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — One event, or also `PixelSizeChanged`?** *Recommended: only `ScaleChanged`.* The pixel size
  changes only together with the size (`Resized`) or the scale (`ScaleChanged`), and after either
  event the application re-queries `pixelSize`. One fewer event kind to handle.
- **Q2 — Scale type?** *Recommended: `float`*, as SDL reports it (`1.0`, `1.25`, `1.5`, `2.0`, …).
- **Q3 — Port sizes on Windows and X11.** There, SDL's window coordinates are physical pixels, not DIPs
  (on macOS and Wayland they are DIPs). So `size()` / `setSize()` would be in pixels on those
  platforms, which contradicts ADR-0006. *Recommended: document it as a known gap now and fix it in a
  dedicated spec, together with the Windows/Linux CI (EPIC-001).* The fix would make the SDL adapter
  convert using `displayScale`, and it can only be verified on those platforms. Fixing it blind now
  would be untested code.
- **Q4 — Fake control name?** *Recommended: `changeDisplayScale(id, scale)`.*
- **Q5 — Seeing it work.** The `dummy` driver can't change the scale, so nothing automated touches a
  real Retina scale. *Recommended:* add **EPIC-005 spec 5**, in which `hello_window` prints its pixel
  size and scale at start and on `ScaleChanged`, for example `hello_window: 1600x1200 pixels at scale
  2`. It's a small spec with a manual check on your screen, and you can drag the window to an external
  monitor if you have one. This adds a spec to an approved epic, so it needs your approval.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0013): …`):
   - **Port:** `ScaleChanged`, the variant, `pixelSize`, `displayScale`.
   - **Stubs:**
     - fake and SDL `pixelSize` return `{0, 0}`, and `displayScale` returns `0.0f`;
     - fake `changeDisplayScale` returns success and does nothing;
     - SDL ignores the scale event.
   - **`hello_window` doubles:** add the overrides.
   - **Tests:** PX1–PX3, FS1–FS2, SD1–SD2, plus an `operator<<` case for `ScaleChanged`. Lint test code
     before freezing.
   - **Expected Red:**
     - Fail: PX1 and PX2 (zeros), PX3 (the stub succeeds), FS1, FS2, SD1 and SD2.
     - No guards expected. FS2's "dropped for a destroyed window" part passes trivially, but the
       test fails on its first part.
2. **Green:**
   - Fake: per-window scale, pixel size computed with `ceil`, `changeDisplayScale` validated and
     coalesced through a shared queue helper (also used by `Resized`).
   - SDL: the two queries, with a 0.0 or failed query reported as `Internal`; translation of
     `DISPLAY_SCALE_CHANGED` with one event per window per batch.
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on the SDL tests, and docs (EPIC-005; record
   Q3's gap in EPIC-001's CI spec notes).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/window_system.hpp` | modify | Event and queries |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | Behavior and test control |
| `modules/platform/tests/contract/…/window_system_contract.hpp` | modify | PX1–PX3, printer |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | FS1–FS2 |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | Queries, translation |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | SD1–SD2 |
| `examples/hello_window/tests/hello_window_test.cpp` | modify | Overrides in the two test doubles |
| `docs/epics/EPIC-005-window-resizing.md`, `EPIC-001-…` | modify | Status, Q3 note |

### 10.3 Risks / alternatives considered
- **Checked in SDL 3.4.18 source:**
  - the `dummy` driver's desktop mode has the default pixel density 1.0, and no content scale;
  - `SDL_GetWindowSizeInPixels` falls back to size × the mode's pixel density;
  - `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` carries no data, and SDL does **not** coalesce it, hence
    the adapter's own coalescing (rule 8).
- **Real scale changes are untested automatically**, because the `dummy` driver can't produce them.
  The behavior is proven on the fake, the SDL translation is proven with pushed events, and the real
  values are checked manually (Q5).
- **Alternative considered:** carrying the pixel size in `ScaleChanged`. Rejected, because the event
  would then duplicate a query that can go stale; one source of truth is enough.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL tests
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `…PX1_PixelSizeAndScaleArePositive` (Fake, Sdl3)
- [x] AC-2 → `…PX2_PixelSizeFollowsResize` (Fake, Sdl3)
- [x] AC-3 → `…PX3_UnknownIdsAreNotFound` (Fake, Sdl3)
- [x] AC-4 → `FakeWindowSystem.FS1_DisplayScaleChangesPixelSize`
- [x] AC-5 → `FakeWindowSystem.FS2_ScaleChangesAreCoalescedAndDropped`
- [x] AC-6 → `SdlWindowSystem.SD1_DummyDriverHasScaleOne`
- [x] AC-7 → `SdlWindowSystem.SD2_UnchangedScaleEventsAreDropped`
- [x] AC-8 → `SdlWindowSystem.SD3_WindowsUseHighPixelDensity` (amendment B)

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q5 confirmed as recommended (Q5 adds EPIC-005 spec 5); spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: 10 tests fail on assertions: PX1–PX3 for Fake and Sdl3, FS1, FS2, SD1, SD2 (stubs report 0×0 pixels and scale 0, succeed where `NotFound` is required, ignore scale changes and scale events). The other 116 pass. API sketch: `ScaleChanged::scale` gets a default initializer (`= 0.0F`, required by `cppcoreguidelines-pro-type-member-init`). The fake's `changeDisplayScale` stub already returns `NotFound` for unknown ids (it must use the object to satisfy lint); FS1 still fails on the scale change itself. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green + Refactor**: fake keeps a per-window scale (initially 1.0), computes pixel size with `ceil`, validates `changeDisplayScale`, and queues `Resized` / `ScaleChanged` through one shared replace-pending helper. SDL maps `pixelSize` / `displayScale` to `SDL_GetWindowSizeInPixels` / `SDL_GetWindowDisplayScale` (failures → `Internal`) and translates `DISPLAY_SCALE_CHANGED` with the queried scale, one per window per batch, dropped if the query returns 0. `tools/check.sh` exit 0 (126/126); `ctest --preset asan` 126/126; macOS `leaks`: 0 leaks for the SDL PX/SD tests. `git diff 6a7e758 -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
| 2026-11-02 | Acknowledged; Status → Done | Emilio Pomares |
| 2026-11-02 | **Amendment B** (found by SPEC-0014's manual check: `800x600 pixels at scale 1` on a Retina display): rule 9 and AC-8 (SD3) added. SDL windows must be created with `SDL_WINDOW_HIGH_PIXEL_DENSITY` (SDL's Cocoa backend allocates a full-density buffer only with that flag). Goes through Red and Green. | Emilio Pomares |
| 2026-11-02 | **Red (amendment B)**: SD3 fails on its assertion (flag missing on both windows); the other 131 tests pass; tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green (amendment B)**: SDL windows are always created with `SDL_WINDOW_HIGH_PIXEL_DENSITY` (plus `SDL_WINDOW_RESIZABLE` when requested). `tools/check.sh` exit 0 (132/132); `ctest --preset asan` 132/132; `leaks`: 0 for SD1–SD3. `git diff 99d7f0e -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
| 2026-11-02 | **Amendment C** (found by SPEC-0014's second manual check: the scale line printed twice, start plus a spurious `ScaleChanged{2}`): rule 8 now requires the SDL adapter to drop scale events that don't change the last reported scale; SD2 renamed `SD2_UnchangedScaleEventsAreDropped` and now expects no event. Approved by the user. | Emilio Pomares |
| 2026-11-02 | **Red (amendment C)**: the rewritten SD2 fails (the adapter still reports `ScaleChanged{1}` for an unchanged scale); the other 131 tests pass; tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green (amendment C)**: each SDL window records the scale at creation; a scale event is reported only if the queried scale differs from the last reported one. `tools/check.sh` exit 0 (132/132); `ctest --preset asan` 132/132; `leaks`: 0 for SD1–SD3. `git diff bc68997 -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
