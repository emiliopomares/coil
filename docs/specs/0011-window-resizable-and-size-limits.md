# SPEC-0011: Resizable windows and minimum/maximum size

| Field | Value |
|-------|-------|
| Status | Done (pending acknowledgement) |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-005](../epics/EPIC-005-window-resizing.md) |
| Related ADRs | ADR-0004 (ports and adapters), ADR-0018 (SDL3) |
| Depends on | SPEC-0010 (`setSize`, `Resized`) |

> **Scope Gate checklist**
> - [x] One concern: whether the user may resize a window, and the size range any resize must stay in
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules (`platform`, `backends/platform_sdl3`); no new dependency or edge. The
>       `hello_window` test doubles get mechanical overrides, as in SPEC-0010.
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
SPEC-0010 lets the program resize a window, but the user can't: SDL windows are created fixed-size.
This spec adds:
- a **resizable** flag, chosen when the window is created;
- optional **minimum and maximum sizes**, which every size change respects, whether it comes from
  `setSize`, from the user dragging an edge, or from changing the limits themselves.

## 2. Scope
- Port:
  - a `WindowSizeLimits` type (optional minimum and maximum);
  - `WindowDesc` gains `resizable` and `limits`;
  - new queries `resizable(id)` and `sizeLimits(id)`, and the setter `setSizeLimits(id, limits)`.
- Fake: the same behavior, including clamping for `setSize` and `userResize`.
- Contract suite: SL1–SL7.
- SDL3 adapter:
  - `SDL_WINDOW_RESIZABLE` at creation;
  - limits through `SDL_SetWindowMinimumSize` and `SDL_SetWindowMaximumSize`;
  - queries through `SDL_GetWindowFlags` and Coil's own record of the limits.
- `hello_window` test doubles: overrides for the three new pure virtuals (forwarding, and `NotFound`).

## 3. Out of Scope
- Changing `resizable` after creation (`setResizable`). SDL's `dummy` driver doesn't support it, so it
  can't be tested headless, and nothing needs it yet (§9 Q2).
- A limit on one dimension only (for example, a maximum width with any height). Limits are whole
  sizes.
- Aspect-ratio constraints, maximize/minimize, fullscreen.
- Making `hello_window` resizable (EPIC-005 spec 4).
- Pixel size and display scale (EPIC-005 spec 3).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp (additions)
namespace coil::platform {

/// The range a window's size must stay in. Each bound is optional; both dimensions of a bound
/// must be > 0, and min must not exceed max in either dimension.
struct WindowSizeLimits {
    std::optional<WindowSize> min; ///< No minimum when empty.
    std::optional<WindowSize> max; ///< No maximum when empty.

    friend constexpr bool operator==(const WindowSizeLimits&, const WindowSizeLimits&) = default;
};

struct WindowDesc {
    std::string title;         ///< UTF-8. May be empty.
    WindowSize size;           ///< Both dimensions must be > 0, and within `limits`.
    bool resizable = false;    ///< Whether the user may resize the window (e.g. by dragging edges).
    WindowSizeLimits limits{}; ///< Applies to every size change.
};

class WindowSystem {
public:
    // … existing members …

    /// Whether the user may resize the window. Fails with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<bool> resizable(WindowId id) const = 0;

    /// The window's current size limits. Fails with NotFound for an unknown window.
    [[nodiscard]] virtual Expected<WindowSizeLimits> sizeLimits(WindowId id) const = 0;

    /// Replaces the window's size limits. If the current size is outside them, the window is
    /// resized to the nearest size inside (reported by Resized). Fails with InvalidArgument for
    /// invalid limits and NotFound for an unknown window.
    [[nodiscard]] virtual Expected<void> setSizeLimits(WindowId id, WindowSizeLimits limits) = 0;
};

} // namespace coil::platform
```

### 4.2 Behavioral rules
1. **Valid limits:** each present bound has both dimensions > 0, and if both bounds are present,
   `min.width ≤ max.width` and `min.height ≤ max.height`. `min == max` is allowed and pins the size.
2. `createWindow` MUST fail with `InvalidArgument` if `desc.limits` is invalid or `desc.size` is
   outside them. The window is created resizable exactly when `desc.resizable` is true. The default
   is `false`, which matches the behavior so far.
3. `resizable(id)` and `sizeLimits(id)` MUST report what the window was created or last set with.
4. **Clamping:** every size change MUST keep the size within the limits. Each dimension is clamped
   independently to `[min, max]`. This applies to:
   - `setSize` (SPEC-0010);
   - user resizes (`userResize` on the fake; a resize from outside the port on SDL);
   - `setSizeLimits` itself.

   The resulting `Resized` carries the clamped size. If clamping leaves the size unchanged, there is
   no event (SPEC-0010 rule 4).
5. `setSizeLimits` with invalid limits MUST fail with `InvalidArgument`. An unknown id MUST fail with
   `NotFound`. On failure, nothing changes and no event is produced.
6. `setSizeLimits` with an empty bound removes that bound.
7. `setSize` with a non-positive size still fails with `InvalidArgument` (SPEC-0010). Clamping applies
   only to valid sizes.
8. SDL:
   - `resizable` → `SDL_WINDOW_RESIZABLE` at creation, and read back with `SDL_GetWindowFlags`.
   - Limits map to `SDL_SetWindowMinimumSize` / `SDL_SetWindowMaximumSize`, with `0×0` meaning "no
     bound". SDL refuses a minimum above the current maximum, so the adapter applies new limits in
     this order: clear the maximum, set the minimum, set the maximum. Any SDL failure MUST be reported
     as `Internal`.
   - `sizeLimits` returns the limits as last set through the port.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `min` > `max` in either dimension | `InvalidArgument` (create, `setSizeLimits`) |
| A bound with a dimension ≤ 0 | `InvalidArgument` |
| `desc.size` outside `desc.limits` | `createWindow` fails with `InvalidArgument` |
| `setSize` above `max` / below `min` | Success; size clamped; `Resized` with the clamped size |
| New limits exclude the current size | Window resized to the nearest allowed size; `Resized` |
| Limits removed (both empty) | Any positive size allowed again |
| Unknown or destroyed id (`resizable`, `sizeLimits`, `setSizeLimits`) | `NotFound` |

### 4.4 Threading and lifetime
UI/main thread only, like the rest of the port.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets yet).

## 5. Acceptance Criteria
Contract tests run on both the fake and SDL (`dummy`). SL<n> = AC-<n>.

- **AC-1 (SL1)** — A window created with `resizable = true` reports `resizable == true`. One created
  with the default reports `false`.
- **AC-2 (SL2)** — A window created with limits min 200×100, max 800×600 reports exactly those limits.
  One created without limits reports two empty bounds.
- **AC-3 (SL3)** — `createWindow` fails with `InvalidArgument`, creating no window, for each of:
  - min above max in width or in height;
  - a bound with a dimension ≤ 0;
  - a size below the minimum or above the maximum.
- **AC-4 (SL4)** — Given limits min 200×100, max 800×600 and a 400×300 window:
  - `setSize(1000×50)` yields exactly `Resized{800×100}`, and `size()` is 800×100;
  - `setSize(800×100)` again produces no event.
- **AC-5 (SL5)** — Given a 400×300 window without limits, `setSizeLimits(max 300×200)` yields exactly
  `Resized{300×200}`, and `sizeLimits` reports the new limits. Then `setSizeLimits(min 500×400)`, with
  no max, yields `Resized{500×400}`.
- **AC-6 (SL6)** — `setSizeLimits` with invalid limits fails with `InvalidArgument`, with no event and
  the limits unchanged. `resizable`, `sizeLimits` and `setSizeLimits` on an unknown or destroyed id
  fail with `NotFound`.
- **AC-7 (SL7)** — Given a max of 300×200, a user resize (trait `userResize`) to 640×480 yields exactly
  `Resized{300×200}`. After the limits are cleared, a user resize to 640×480 yields `Resized{640×480}`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `…/WindowSystemContract.SL1_ResizableIsSetAtCreation` | contract (Fake, Sdl3) | |
| AC-2 | `…SL2_LimitsAreSetAtCreation` | contract | |
| AC-3 | `…SL3_CreateRejectsInvalidLimitsAndSizes` | contract | table of cases |
| AC-4 | `…SL4_SetSizeIsClampedToLimits` | contract | `waitEvents(1 s)` |
| AC-5 | `…SL5_NewLimitsResizeTheWindow` | contract | |
| AC-6 | `…SL6_SetLimitsRejectsBadInputWithoutEvents` | contract | |
| AC-7 | `…SL7_UserResizeIsClampedToLimits` | contract | trait `userResize` (SPEC-0010) |

No new traits. On SDL, `userResize` calls `SDL_SetWindowSize` directly, which applies SDL's own
clamping to the limits the adapter set. That is exactly the path a user drag takes.

## 7. Non-Functional Requirements
- No new dependency.

## 8. Architectural Impact
- `WindowDesc` grows two fields with defaults, so existing designated initializers stay valid.
- Three new pure virtuals. Every implementation must add them: the fake, SDL and the two
  `hello_window` test doubles.

## 9. Open Questions
All resolved as recommended (2026-11-01, Emilio Pomares).
- **Q1 — Default for `resizable`?** *Recommended: `false`.* It keeps today's behavior, and
  `hello_window` opts in explicitly in spec 4. The alternative, `true`, would match most desktop apps
  but silently changes existing windows.
- **Q2 — Change `resizable` after creation?** *Recommended: not now.* SDL's `dummy` driver returns
  "Unsupported" for `SDL_SetWindowResizable`, so it couldn't be contract-tested headless, and no
  feature needs it yet. It can be added later with a manual check.
- **Q3 — One `setSizeLimits` call, or separate `setMinimumSize` / `setMaximumSize`?** *Recommended:
  one call.* With separate setters, the order of calls matters: raising the minimum above the old
  maximum fails. One call validates the pair together, and the adapter handles SDL's ordering.
- **Q4 — `desc.size` outside `desc.limits` at creation?** *Recommended: reject with
  `InvalidArgument`.* It's almost certainly a caller bug. Clamping silently would hide it, and
  rejecting is consistent with how invalid sizes are already treated.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0011): …`):
   - **Port:** `WindowSizeLimits`, the new `WindowDesc` fields, and the three methods.
   - **Stubs:**
     - fake and SDL `resizable` return `false`, `sizeLimits` returns `{}`, `setSizeLimits` returns
       success;
     - `createWindow` ignores the new fields;
     - `setSize` doesn't clamp.
   - **`hello_window` test doubles:** add the overrides.
   - **Tests:** SL1–SL7 appended to the contract suite, with an `operator<<` for `WindowSizeLimits`.
     Lint test code before freezing.
   - **Expected Red:** on both implementations SL1, SL2, SL3, SL4, SL5 and SL7 fail on assertions,
     and SL6 fails because the stub succeeds where `InvalidArgument` / `NotFound` is expected. No
     guards are expected.
2. **Green:**
   - Fake: store `resizable` and the limits; validate them; one `clamp` helper used by `setSize`,
     `userResize` and `setSizeLimits`.
   - SDL: create with the flag; record the limits; apply them in the order rule 8 gives; validate
     before calling SDL. Clamping on SDL is done by SDL itself.
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on the SDL SL tests, and docs (EPIC-005, Change
   Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/window_system.hpp` | modify | Types and methods |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | Behavior |
| `modules/platform/tests/contract/…/window_system_contract.hpp` | modify | SL1–SL7, printer |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | Behavior |
| `examples/hello_window/tests/hello_window_test.cpp` | modify | Overrides in the two test doubles |
| `docs/epics/EPIC-005-window-resizing.md` | modify | Status |

### 10.3 Risks / alternatives considered
- **Checked in SDL 3.4.18 source:**
  - `SDL_SetWindowMinimumSize` / `SDL_SetWindowMaximumSize` immediately call `SDL_SetWindowSize` with
    the current size, which clamps it, and the `dummy` driver reports the change as `RESIZED`. SL5
    relies on this.
  - SDL rejects a minimum above the current maximum, hence the ordering in rule 8.
  - The `dummy` driver has no `SetWindowResizable`, hence Q2.
- **SDL clamps against the limits it holds.** The adapter validates before calling SDL, so SDL's
  state and Coil's recorded limits never disagree. If an SDL call fails midway (it shouldn't after
  validation), the error is returned as `Internal`, and the recorded limits stay as they were.
- **Resized coalescing (SPEC-0010 rule 9):** applying limits may trigger more than one internal
  resize on SDL. The tests only see the coalesced final event, which is the contract.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL SL tests
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `…SL1_ResizableIsSetAtCreation` (Fake, Sdl3)
- [x] AC-2 → `…SL2_LimitsAreSetAtCreation` (Fake, Sdl3)
- [x] AC-3 → `…SL3_CreateRejectsInvalidLimitsAndSizes` (Fake, Sdl3)
- [x] AC-4 → `…SL4_SetSizeIsClampedToLimits` (Fake, Sdl3)
- [x] AC-5 → `…SL5_NewLimitsResizeTheWindow` (Fake, Sdl3)
- [x] AC-6 → `…SL6_SetLimitsRejectsBadInputWithoutEvents` (Fake, Sdl3)
- [x] AC-7 → `…SL7_UserResizeIsClampedToLimits` (Fake, Sdl3)

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Q1–Q4 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: 14 tests fail on assertions: SL1–SL7 for both Fake and Sdl3 (stubs report `false` / empty limits, ignore limits and clamping, and succeed where `InvalidArgument` / `NotFound` is required). No guards, as planned; the other 97 tests pass. The `hello_window` test doubles gain the three overrides. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-01 | **Green + Refactor**: fake stores `resizable` and limits, validates them, and clamps every size change through one `resize` helper (which also coalesces `Resized`). SDL creates windows with `SDL_WINDOW_RESIZABLE`, validates limits before calling SDL, applies them in the order clear max → min → max, and lets SDL clamp; limits are recorded only after SDL accepts them. `tools/check.sh` exit 0 (111/111 tests); `ctest --preset asan` 111/111; macOS `leaks`: 0 leaks for Sdl3 SL1–SL7. `git diff 8b711f0 -- ':(glob)**/tests/**'` empty. | Emilio Pomares (acknowledgement pending) |
