# SPEC-0025: Frame capture (`RenderBackend::capture`)

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `coil::render` (port, recording backend, contract suite); `backends/render_software` |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 5) |
| Related ADRs | ADR-0017 (frame capture), ADR-0009 Amendment 3 (`render` may depend on `tasks`), ADR-0014 (`AsyncResult`), ADR-0006 (DIPs vs pixels) |
| Depends on | SPEC-0020 (port), SPEC-0021 (software renderer), SPEC-0024 (`AsyncResult`) |

> **Scope Gate checklist**
> - [x] One concern: a backend can hand back the pixels of the last frame it showed, whole or a region
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 areas (`render`, `backends/render_software`); edges `render → tasks` (ADR-0009 Amendment 3) and `render_software → tasks` (§8, approved with this spec)
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
ADR-0017 gives every backend a capture capability: tests, agents and the UI host read back the pixels
of a frame through the port, whichever renderer drew it. The UI host (EPIC-013) uses it to put frames
in windows, and golden-image tests and MCP screenshots build on it later. Today only
`SoftwareRenderBackend::frontBuffer` exists, outside the port.

## 2. Scope
- Port: `RenderBackend::capture(RenderTargetId, std::optional<Rect> region)` returning
  `tasks::AsyncResult<CapturedFrame>`.
- `CapturedFrame`: the image and its display scale.
- Software backend: captures from the front buffer; ready at once.
- Recording backend: a transparent image of the right size (it draws nothing).
- Contract suite: new tests CP1–CP3 for every implementation; software-specific tests.

## 3. Out of Scope
- Golden-image files, their format and comparison tolerance (§9 Q5: a separate spec).
- Masking sensitive widgets and the automation-id overlay (ADR-0017; they need the automation tree:
  EPIC-014 and the MCP work).
- PNG encoding (`ImageEncoder`, with MCP).
- GPU readback (with the GPU backends).
- Changing the `boxes` example (it keeps `frontBuffer`; the UI host uses `capture`).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/render/include/coil/render/render_backend.hpp (additions)
namespace coil::render {

/// Pixels read back from a render target (ADR-0017).
struct CapturedFrame {
    Image image;        ///< RGBA8, sRGB, straight alpha; row 0 is the top.
    float scale = 1.0F; ///< The target's display scale: image pixels per DIP.
};

class RenderBackend {
public:
    // … existing members …

    /// Reads back the last frame present() showed on the target (transparent before any present).
    /// `region`, in DIPs, limits it to that rectangle, rounded out to whole pixels and clipped to
    /// the target. The result may complete later (GPU readback); it fails with NotFound for an
    /// unknown target and InvalidArgument for a region that is not finite or has a negative size.
    [[nodiscard]] virtual tasks::AsyncResult<CapturedFrame>
    capture(RenderTargetId id, std::optional<Rect> region = std::nullopt) = 0;
};

/// A rectangle of whole pixels.
struct PixelRect { std::uint32_t x = 0, y = 0, width = 0, height = 0; };

/// For implementations: the pixels a capture of `region` covers on a target described by `desc`
/// (rules 1–3). InvalidArgument for an invalid region.
[[nodiscard]] Expected<PixelRect> captureRect(const RenderTargetDesc& desc,
                                              std::optional<Rect> region);

} // namespace coil::render
```

### 4.2 Behavioral rules
1. Without a region, the captured image has the target's pixel size
   (`ceil(width × scale) × ceil(height × scale)`, SPEC-0020) and `scale` is the target's scale.
2. **Region:** the pixel rectangle is `floor(left × scale)`, `floor(top × scale)` to
   `ceil(right × scale)`, `ceil(bottom × scale)`, intersected with the target's pixel bounds. An
   empty intersection gives a 0×0 image. A 0×0 region gives a 0×0 image.
3. A region with any non-finite value, or a negative width or height, fails with `InvalidArgument`.
4. An unknown or destroyed target fails with `NotFound`. Errors are delivered **in the result** (it
   completes failed), not as a separate return value, so callers have one path.
5. **Software:** the image is the front buffer (`frontBuffer`), or its sub-rectangle for a region.
   `submit` without `present` does not change what is captured. The result is ready when `capture`
   returns.
6. **Recording:** the image is fully transparent, with the size of rules 1–2. The result is ready
   when `capture` returns.
7. The captured frame is a copy: later frames, resizes or destroying the target do not change it.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Unknown or destroyed target | Result fails with `NotFound` |
| Region with NaN or infinity, or negative size | Result fails with `InvalidArgument` |
| Region outside the target | 0×0 image, success |
| Region partly outside | Clipped |
| Capture before any present | Transparent image (software and recording) |
| 0×0 target | 0×0 image |

### 4.4 Threading and lifetime
UI thread, like the rest of the port. GPU backends may complete the result from another thread
later (allowed by SPEC-0024 rule 8).

### 4.5 Async behavior
Both implementations here complete at once. Contract tests read the result with `take()` right
after the call and must not assume more than "ready eventually"; they assert readiness only for
software and recording (rules 5, 6).

### 4.6 Automation and agent exposure
This is the basis for screenshots (ADR-0017); masking comes later (§3).

## 5. Acceptance Criteria
- **AC-1 (CP1, contract)** — `capture` on a target that never existed, and on a destroyed one, gives
  a result that fails with `NotFound`.
- **AC-2 (CP2, contract)** — For a target of 100.5×50 DIPs at scale 2, `capture()` gives a 201×100
  image with `scale == 2`; after `resizeTarget` to 30×20 at scale 1, a 30×20 image with scale 1.
- **AC-3 (CP3, contract)** — On a 100×50 target at scale 2: region (10, 10) 20×10 gives 40×20;
  region (10.25, 0) 1×1 gives 3×2 (rounded out); region (90, 40) 20×20 gives 20×20 (clipped);
  region (200, 0) 10×10 gives 0×0; a region with NaN, infinity or a negative size fails with
  `InvalidArgument`.
- **AC-4 (software)** — After `submit` and `present` of a frame with a red rect, `capture()` equals
  `frontBuffer`; a region capture equals the matching sub-rectangle of `frontBuffer` pixel for pixel.
- **AC-5 (software)** — Before any present, the capture is transparent; after a second `submit`
  without `present`, the capture still shows the first presented frame; the result is ready when
  `capture` returns.
- **AC-6 (software)** — A captured frame does not change when a later frame is presented or the
  target is destroyed.
- **AC-7 (recording)** — `RecordingRenderBackend::capture` gives a ready, fully transparent image
  of the size of rules 1–2.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `RenderBackendContract/{Recording,Software}.CP1_CaptureOfUnknownTargetIsNotFound` | unit | |
| AC-2 | `…CP2_CaptureHasTargetPixelSizeAndScale` | unit | |
| AC-3 | `…CP3_RegionsAreRoundedOutAndClipped` | unit | |
| AC-4 | `SoftwareRenderBackend.SC1_CaptureIsTheFrontBuffer` | unit | |
| AC-5 | `SoftwareRenderBackend.SC2_CaptureShowsThePresentedFrame` | unit | |
| AC-6 | `SoftwareRenderBackend.SC3_CaptureIsACopy` | unit | |
| AC-7 | `RecordingRenderBackend.SC4_CaptureIsTransparent` | unit | |

## 7. Non-Functional Requirements
- One copy of the captured pixels per call; no work when nobody captures.

## 8. Architectural Impact
- `RenderBackend` gains a pure virtual `capture`; both implementations are updated.
- `render` links `coil::tasks` (ADR-0009 Amendment 3).
- **New edge `render_software → tasks`** in `tools/layering.json`: `render_software` must include
  `coil/tasks/async_result.hpp` to implement `capture` (include what you use). `tasks` is already
  reachable through `render`, and the edge creates no cycle, so per CLAUDE.md §1 it needs only your
  approval in this spec.
- ADR-0017 says `WindowTargetId`; the port's id is `RenderTargetId` (SPEC-0020). ADR-0017 gets a
  one-line note.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — What a capture returns.** `Image` has no scale, and ADR-0017 asks for one. *Recommended:*
  `CapturedFrame { Image image; float scale; }`.
- **Q2 — Region units.** *Recommended:* DIPs, like every rectangle above the backends (ADR-0006),
  rounded out so that a region always covers what was drawn in it.
- **Q3 — How errors are delivered.** *Recommended:* inside the result (rule 4), so callers handle
  synchronous and asynchronous failures the same way.
- **Q4 — What the recording backend captures.** It draws nothing. *Recommended:* a transparent image
  of the right size, so code under test that captures keeps working; tests that need pixels use the
  software backend.
- **Q5 — Golden images.** SPEC-0023 deferred the stored golden image "to spec 5". Doing it here means
  choosing a file format (PNG needs a decoder; PAM, a plain netpbm format, needs none), a comparison
  with tolerance and an update workflow, and touching `examples/boxes`: a third area. *Recommended:*
  split EPIC-006 spec 5 into **5a capture** (this spec) and **5b golden images** (format, compare,
  update, the `boxes` golden). 5b is not on the path to the button and can follow Milestone 2.
- **Q6 — The `render_software → tasks` edge** (§8). *Recommended:* approve it with this spec.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0025): …`):
   - **Stubs:** `CapturedFrame`; `capture` in the port; both implementations return a ready
     `CapturedFrame` with a 0×0 image and scale 0. `render` links `tasks`; `render_software` edge
     added.
   - **Tests:** CP1–CP3 in the contract suite; SC1–SC4.
   - **Expected Red:** CP1–CP3 fail on both implementations (6) and SC1–SC4 fail (4), all on
     assertions; the other 219 pass.
2. **Green:** a small public helper in `render` turns a description and a region into a pixel
   rectangle (rules 2–3), used by both implementations (§10.3); software copies from the front
   buffer; recording makes a transparent image.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; `ctest --preset tsan`.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/render/include/coil/render/render_backend.hpp`, `src/render_backend.cpp` | modify | `CapturedFrame`, `capture`, region helper |
| `modules/render/CMakeLists.txt` | modify | Link `coil::tasks` |
| `modules/render/testing/…/recording_render_backend.{hpp,cpp}` | modify | `capture` |
| `modules/render/tests/contract/coil/render/render_backend_contract.hpp` | modify | CP1–CP3 |
| `modules/render/tests/recording_render_backend_test.cpp` | modify | SC4 |
| `backends/render_software/{include/…/software_render_backend.hpp, src/software_render_backend.cpp}` | modify | `capture` |
| `backends/render_software/tests/software_render_backend_test.cpp` | modify | SC1–SC3 |
| `tools/layering.json` | modify | `render_software → tasks` |
| `docs/adr/0017-frame-capture.md`, `docs/epics/EPIC-006-first-pixels.md` | modify | Note; spec table (Q5) |

### 10.3 Risks / alternatives considered
- **Where the region rule lives:** both backends need rules 2–3, and GPU backends will too. A small
  public helper in `render` keeps them identical; it is covered by CP3 through every implementation.
- **Synchronous `Expected<CapturedFrame>`** was rejected with ADR-0009 Amendment 3.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes on `dev`, `asan` and `tsan`
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `RenderBackendContract/{Recording,Software}.CP1_CaptureOfUnknownTargetIsNotFound`
- [x] AC-2 → `RenderBackendContract/{Recording,Software}.CP2_CaptureHasTargetPixelSizeAndScale`
- [x] AC-3 → `RenderBackendContract/{Recording,Software}.CP3_RegionsAreRoundedOutAndClipped`
- [x] AC-4 → `SoftwareRenderBackend.SC1_CaptureIsTheFrontBuffer`
- [x] AC-5 → `SoftwareRenderBackend.SC2_CaptureShowsThePresentedFrame`
- [x] AC-6 → `SoftwareRenderBackend.SC3_CaptureIsACopy`
- [x] AC-7 → `RecordingRenderBackend.SC4_CaptureIsTransparent`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q6 approved as recommended (including the `render_software → tasks` edge and the split of EPIC-006 spec 5) | Emilio Pomares |
| 2026-11-07 | Red: CP1–CP3 fail on both implementations (6) and SC1–SC4 fail (4), all on assertions (0×0 images, scale 0, errors not reported); the other 219 pass; format, layering and tidy clean | — |
| 2026-11-07 | Green (`37c9d83`): 229/229 on `dev`, `asan` and `tsan`; tests unchanged since Red (`3551dac`); all gates pass. Status → Done | — |
