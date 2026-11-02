# SPEC-0020: `RenderBackend` port, recording backend and contract suite

| Field | Value |
|-------|-------|
| Status | Approved (complete; awaiting acknowledgement) |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::render` (port, `render_testing`, contract suite) |
| Epic | [EPIC-006](../epics/EPIC-006-first-pixels.md) (spec 2) |
| Related ADRs | ADR-0009 (`RenderBackend` port), ADR-0004 (ports and adapters), ADR-0003 (fakes and contract suites) |
| Depends on | SPEC-0019 (`DrawList`) |

> **Scope Gate checklist**
> - [x] One concern: the port through which `DrawList`s reach a renderer, with a test double that obeys the same contract
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`render`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
A `DrawList` is only data. The `RenderBackend` port (ADR-0009) is how it reaches pixels: the software
renderer (spec 3), its on-screen presentation (spec 4), and later Vulkan and OpenGL. Like the
`WindowSystem` port, it comes with a **contract suite** that every backend must pass, and a test
double, `RecordingRenderBackend`, which keeps what it is given so that higher layers (widgets, the app
loop) can be tested without pixels.

## 2. Scope
- Port `RenderBackend`: create, resize and destroy **render targets**; `submit` a `DrawList` to a target;
  `present` it.
- `RenderTargetId`, `RenderTargetDesc` (logical size in DIPs and scale).
- `RecordingRenderBackend` (in `modules/render/testing`, library `coil::render_testing`), with
  inspection: the submitted frames, the present count and the current description per target.
- Contract suite (`coil::render_contract`, header-only typed tests), instantiated for the recording
  backend.

## 3. Out of Scope
- Rasterizing (spec 3); showing frames in windows and how a target is tied to a window (spec 4).
- Frame capture (spec 5); textures and `uploadTexture` (EPIC-012); `GpuSurface`.
- Threading of submission (single-threaded for now, like the rest of the UI).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/render/include/coil/render/render_backend.hpp
namespace coil::render {

/// Identifies a render target. Unique per backend instance; never reused.
struct RenderTargetId {
    std::uint64_t value = 0;
    friend constexpr bool operator==(RenderTargetId, RenderTargetId) = default;
};

/// A target's logical size (DIPs) and display scale. Its pixel size is
/// ceil(width x scale) x ceil(height x scale), like WindowSystem::pixelSize.
struct RenderTargetDesc {
    Size size;
    float scale = 1.0F;
    friend constexpr bool operator==(RenderTargetDesc, RenderTargetDesc) = default;
};

/// Port: turns DrawLists into pixels on render targets (ADR-0009). One instance per renderer.
class RenderBackend {
public:
    RenderBackend() = default;
    virtual ~RenderBackend() = default;
    // copy and move deleted

    /// Fails with InvalidArgument if the size is negative or not finite, or the scale is not
    /// finite and > 0. A 0x0 size is valid (for example, a minimized window).
    [[nodiscard]] virtual Expected<RenderTargetId> createTarget(const RenderTargetDesc& desc) = 0;
    /// NotFound for an unknown or destroyed id.
    [[nodiscard]] virtual Expected<void> destroyTarget(RenderTargetId id) = 0;
    /// New size or scale (for example, after a window resize). Same errors as createTarget, plus
    /// NotFound.
    [[nodiscard]] virtual Expected<void> resizeTarget(RenderTargetId id,
                                                      const RenderTargetDesc& desc) = 0;
    /// Renders `frame` into the target's back buffer. The backend does not keep a reference to
    /// `frame`. NotFound for an unknown id.
    [[nodiscard]] virtual Expected<void> submit(RenderTargetId id, const DrawList& frame) = 0;
    /// Shows the last submitted frame. NotFound for an unknown id.
    [[nodiscard]] virtual Expected<void> present(RenderTargetId id) = 0;
};

} // namespace coil::render
```
```cpp
// modules/render/testing/include/coil/render/testing/recording_render_backend.hpp
namespace coil::render::testing {

/// Test double for RenderBackend (ADR-0003): keeps everything it is given.
class RecordingRenderBackend final : public RenderBackend {
public:
    // RenderBackend overrides …

    /// Frames submitted to `id`, oldest first. NotFound for an unknown or destroyed id.
    [[nodiscard]] Expected<std::vector<DrawList>> frames(RenderTargetId id) const;
    /// Number of present() calls for `id`. NotFound as above.
    [[nodiscard]] Expected<std::size_t> presentCount(RenderTargetId id) const;
    /// The target's current description. NotFound as above.
    [[nodiscard]] Expected<RenderTargetDesc> desc(RenderTargetId id) const;
};

} // namespace coil::render::testing
```

### 4.2 Behavioral rules
1. **Validity** of a `RenderTargetDesc`: width and height finite and ≥ 0; scale finite and > 0.
2. `createTarget` returns a new id for each call, distinct from every earlier id of that backend.
3. After `destroyTarget(id)`, every call with `id` fails with `NotFound`.
4. `submit` and `present` succeed for any existing target, including a 0×0 one, and in any order
   (presenting before any submit is allowed).
5. **Recording backend:** `frames` holds a copy of each submitted list, so later changes to the caller's
   list do not affect it; `presentCount` counts `present` calls; `desc` is the latest valid description.
   A failed call changes nothing.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Negative, NaN or infinite size; scale ≤ 0, NaN or infinite | `InvalidArgument` (create, resize) |
| Unknown or destroyed id | `NotFound` (destroy, resize, submit, present; recording inspection too) |
| 0×0 target | Valid; submit and present succeed |
| `present` before `submit` | Succeeds |

### 4.4 Threading and lifetime
UI thread only for now (ADR-0010 allows a render thread later; that would be its own spec). The
backend owns its targets; destroying the backend destroys them.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (capture is spec 5).

## 5. Acceptance Criteria
Contract tests run for every backend; AC-6–AC-8 are specific to the recording backend.

- **AC-1 (contract)** — Creating two targets with valid descriptions (including 0×0, and scale 2)
  succeeds with distinct ids.
- **AC-2 (contract)** — `createTarget` fails with `InvalidArgument` for width −1, a NaN height, an
  infinite width, and scales 0, −1, NaN and ∞.
- **AC-3 (contract)** — With an id that was never created, and with a destroyed one, `destroyTarget`,
  `resizeTarget`, `submit` and `present` fail with `NotFound`.
- **AC-4 (contract)** — On an existing target: `present` before any submit, `submit` of an empty and of a
  non-empty list, `present`, and `resizeTarget` to a valid description all succeed; `resizeTarget` with
  an invalid description fails with `InvalidArgument`.
- **AC-5 (contract)** — Two targets are independent: destroying one leaves the other usable.
- **AC-6** — The recording backend's `frames` lists submitted lists in order; changing or clearing the
  caller's list afterwards does not change them.
- **AC-7** — `presentCount` counts presents per target; `desc` follows create and successful resizes,
  and is unchanged by a failed resize.
- **AC-8** — The recording inspection calls fail with `NotFound` for unknown and destroyed ids.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `RenderBackendContract/Recording.AC1_CreatesDistinctTargets` | unit | typed contract suite |
| AC-2 | `…AC2_RejectsInvalidDescriptions` | unit | |
| AC-3 | `…AC3_UnknownTargetsAreNotFound` | unit | |
| AC-4 | `…AC4_TargetLifecycle` | unit | |
| AC-5 | `…AC5_TargetsAreIndependent` | unit | |
| AC-6 | `RecordingRenderBackend.AC6_KeepsCopiesOfFrames` | unit | |
| AC-7 | `RecordingRenderBackend.AC7_CountsPresentsAndTracksDescriptions` | unit | |
| AC-8 | `RecordingRenderBackend.AC8_InspectionOfUnknownTargetsFails` | unit | |

## 7. Non-Functional Requirements
- None beyond the port rules (CLAUDE.md §5: abstract class, virtual destructor, no data members,
  copy and move deleted).

## 8. Architectural Impact
- `render` gains a port, a test-support library (`coil::render_testing`) and a contract library
  (`coil::render_contract`), mirroring `platform`. No new dependency or layering edge.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — Port surface for now.** *Recommended:* create, destroy, resize, submit, present. `capture`
  comes with spec 5 and `uploadTexture` with EPIC-012, each extending the port and the contract.
- **Q2 — Target size.** *Recommended:* logical size in DIPs plus scale, with pixels computed by the
  backend as for windows (`ceil(size × scale)`), so layout and windows speak the same units. Alternative:
  pixel size directly (simpler for backends, but every caller converts).
- **Q3 — Tying a target to a window.** *Recommended: decided in spec 4*, which may extend
  `RenderTargetDesc` or add an adapter-specific way to create window targets (EPIC-006 notes). This spec
  keeps the port window-agnostic.
- **Q4 — Where the recording backend lives.** *Recommended:* `modules/render/testing` as
  `coil::render_testing`, like `FakeWindowSystem` in `modules/platform/testing` (CLAUDE.md lists
  `RecordingRenderBackend` among the fakes). Not under `backends/`, since it is not a production adapter.
- **Q5 — `submit` and `present` as separate calls.** *Recommended: yes* (ADR-0009): a frame can be
  rendered without being shown (capture, tests), and presentation can later be paced by vsync (EPIC-003).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0020): …`):
   - **Stubs:** port header complete; `RecordingRenderBackend` whose calls all succeed without storing
     anything (ids all 0, inspection returns empty values).
   - **Tests:** contract suite header; `recording_render_backend_test.cpp` (instantiates the suite and
     holds AC-6–AC-8). Lint the test code.
   - **Expected Red:** all 8 fail on assertions; the other 174 pass.
2. **Green:** the recording backend (a map from id to target state; a counter for ids).
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-006 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/render/include/coil/render/render_backend.hpp` | create | Port |
| `modules/render/src/render_backend.cpp` | create | Out-of-line anchor, if lint needs it (as `window_system.cpp`) |
| `modules/render/testing/{include/coil/render/testing/recording_render_backend.hpp, src/recording_render_backend.cpp}` | create | Test double |
| `modules/render/tests/contract/coil/render/render_backend_contract.hpp` | create | Contract suite |
| `modules/render/tests/recording_render_backend_test.cpp` | create | Instantiation, AC-6–AC-8 |
| `modules/render/CMakeLists.txt`, `tests/CMakeLists.txt` | modify | New targets |
| `docs/epics/EPIC-006-first-pixels.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Copying every submitted frame** in the recording backend costs memory in long tests. Acceptable for a
  test double; tests that submit many frames can destroy and recreate targets.

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
- [x] AC-1 → `RenderBackendContract/Recording.AC1_CreatesDistinctTargets`
- [x] AC-2 → `RenderBackendContract/Recording.AC2_RejectsInvalidDescriptions`
- [x] AC-3 → `RenderBackendContract/Recording.AC3_UnknownTargetsAreNotFound`
- [x] AC-4 → `RenderBackendContract/Recording.AC4_TargetLifecycle`
- [x] AC-5 → `RenderBackendContract/Recording.AC5_TargetsAreIndependent`
- [x] AC-6 → `RecordingRenderBackend.AC6_KeepsCopiesOfFrames`
- [x] AC-7 → `RecordingRenderBackend.AC7_CountsPresentsAndTracksDescriptions`
- [x] AC-8 → `RecordingRenderBackend.AC8_InspectionOfUnknownTargetsFails`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q5 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: all 8 new tests fail on assertions (stub recording backend: every call succeeds, ids are all 0, nothing is stored); the other 174 pass. `modules/render/tests/.clang-tidy` now matches `modules/platform/tests/.clang-tidy`, including the typed-test naming exception approved in SPEC-0003 (the copy from `foundation` made in SPEC-0019 lacked it); no new relaxation. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green + Refactor**: `RecordingRenderBackend` keeps a map from id to target (description, copied frames, present count), validates descriptions, never reuses ids. `tools/check.sh` exit 0 (182/182); `ctest --preset asan` 182/182. `git diff fcc449c -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
