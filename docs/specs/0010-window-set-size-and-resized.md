# SPEC-0010: `WindowSystem::setSize` and the `Resized` event

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-005](../epics/EPIC-005-window-resizing.md) |
| Related ADRs | ADR-0004 (ports and adapters), ADR-0006 / ADR-0019 (logical units), ADR-0018 (SDL3) |
| Depends on | SPEC-0003 (port), SPEC-0006 (events), SPEC-0009 (`waitEvents`) |

> **Scope Gate checklist**
> - [x] One concern: a window's size can change (by the program or the user) and the application is told
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (`platform`, `backends/platform_sdl3`); no new dependency or edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
Windows have a fixed size today. EPIC-005 makes them resizable. This first spec adds the two basic
pieces: the program can **resize** a window (`setSize`), and **every** size change, whether the program
asked for it or the user dragged an edge, reaches the application as a `Resized` event with the new
logical size. Later specs add the resizable flag and limits (spec 2) and pixel size and display scale
(spec 3).

## 2. Scope
- Port: a `Resized` event (a new `WindowEvent` alternative) and `setSize(id, size)`.
- Fake: `setSize`, plus the test control `userResize(id, size)`, which simulates the user dragging an
  edge.
- Contract suite: new tests RS1–RS6, and a new trait `userResize` for each implementation.
- SDL3 adapter: `setSize` through `SDL_SetWindowSize`; `SDL_EVENT_WINDOW_RESIZED` translated to
  `Resized`.

## 3. Out of Scope
- The resizable flag, minimum and maximum size (EPIC-005 spec 2). Until then, SDL windows are created
  fixed-size, so only the program (or a test) can resize them.
- Pixel size, display scale, `PIXEL_SIZE_CHANGED`, `DISPLAY_SCALE_CHANGED` (EPIC-005 spec 3).
- Position, maximize, minimize and fullscreen.
- Throttling resize events during a live drag (beyond the coalescing in rule 9).
- `hello_window` changes (EPIC-005 spec 4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp (additions)
namespace coil::platform {

/// A window's logical size changed: by setSize(), by the user, or by the OS.
struct Resized {
    WindowId window;
    WindowSize size; ///< The new size, as applied (may differ from a request, e.g. OS limits).

    friend constexpr bool operator==(Resized, Resized) = default;
};

/// An event about a window. New alternatives are added by later specs.
using WindowEvent = std::variant<CloseRequested, Resized>;

class WindowSystem {
public:
    // … existing members …

    /// Asks to resize a window. The change is reported by a Resized event; size() returns the new
    /// size by the time that event has been returned. No event if the size does not change.
    /// Fails with InvalidArgument for a non-positive size and NotFound for an unknown window.
    [[nodiscard]] virtual Expected<void> setSize(WindowId id, WindowSize size) = 0;
};

} // namespace coil::platform
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp (additions)
    [[nodiscard]] Expected<void> setSize(WindowId id, WindowSize size) override;

    /// Test control: resizes `id` as if the user dragged its edge. Same rules and errors as setSize.
    [[nodiscard]] Expected<void> userResize(WindowId id, WindowSize size);
```
```cpp
// Contract trait addition (window_system_contract.hpp)
//   // Simulates the user resizing `id` to `size` (SPEC-0010).
//   static void userResize(WindowSystem& system, WindowId id, WindowSize size);
```

### 4.2 Behavioral rules
1. `setSize` with an unknown or destroyed id MUST fail with `NotFound`. A non-positive width or height
   MUST fail with `InvalidArgument`. On failure nothing changes and no event is produced.
2. `setSize` to a size different from the current one MUST lead to exactly one `Resized{id, newSize}`
   event. On the fake and on SDL's `dummy` driver it is pending immediately. On real OS backends it
   may arrive later, so callers and contract tests wait for it with `waitEvents`.
3. Once a `Resized` event has been returned by `pollEvents` or `waitEvents`, `size(id)` MUST report that
   event's size (until the next change).
4. `setSize` to the window's current size MUST succeed and produce **no** event.
5. A size change made outside the program (the user, the OS) MUST produce `Resized` under the same
   rules: one event per actual change, carrying the new size.
6. `Resized` events keep their order with other events (SPEC-0006), and are dropped if their window is
   destroyed before they are returned (SPEC-0006 rule).
7. SDL: `SDL_EVENT_WINDOW_RESIZED` (data1, data2 = logical width, height) maps to `Resized` for a live
   window. Other SDL window events remain ignored. If `SDL_SetWindowSize` fails, `setSize` fails with
   `Internal` and SDL's message.
8. Fake: `userResize` follows rules 1–4 exactly like `setSize` (errors, change, no-op on the same size).
9. **Coalescing** (amendment, 2026-11-01): at most one `Resized` per window is pending at a time. A new
   resize of a window whose previous `Resized` has not been returned yet removes that older event,
   and the new event goes to the end of the queue. The latest size is therefore never lost, but
   intermediate sizes may never be seen. This is SDL's own behavior: `SDL_windowevents.c` drops
   superseded `RESIZED` events "to fix queue overflow". The fake does the same.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Unknown or destroyed id | `NotFound`, no event |
| Width or height ≤ 0 | `InvalidArgument`, no event, size unchanged |
| Same size as current | Success, no event |
| Two resizes before polling | One `Resized` with the last size (rule 9); `size()` = the last one |
| OS adjusts the requested size (e.g. limits, spec 2) | Success; `Resized` carries the applied size |

### 4.4 Threading and lifetime
UI/main thread only, like the rest of the port.

### 4.5 Async behavior
Not applicable (events are pulled with `pollEvents` / `waitEvents`).

### 4.6 Automation and agent exposure
Not applicable (no widgets yet).

## 5. Acceptance Criteria
Contract tests run on both the fake and SDL (`dummy`). RS<n> = AC-<n>.

- **AC-1 (RS1)** — Given a 100×100 window, when `setSize(id, 200×150)` succeeds, then waiting for
  events yields exactly `Resized{id, 200×150}`, and `size(id)` is 200×150.
- **AC-2 (RS2)** — Given a 100×100 window, `setSize(id, 100×100)` succeeds and produces no event.
- **AC-3 (RS3)** — `setSize` with width or height ≤ 0 fails with `InvalidArgument`, and `setSize` on an
  unknown or destroyed id fails with `NotFound`. In both cases there is no event and the size is
  unchanged.
- **AC-4 (RS4)** — Given a window resized by the user (trait `userResize`, to 320×240), waiting for
  events yields exactly `Resized{id, 320×240}`, and `size(id)` is 320×240.
- **AC-5 (RS5)** — Given two `setSize` calls (200×150, then 300×200) before any event call, the events
  are exactly `Resized{300×200}` (coalesced, rule 9), and `size(id)` is 300×200.
- **AC-6 (RS6)** — Given windows A and B, both resized, and A destroyed before events are read, only
  B's `Resized` is delivered.
- **AC-7 (RS7)** — Fake: `userResize` fails with `NotFound` for an unknown id and with
  `InvalidArgument` for a non-positive size, and to the current size it produces no event.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `…/WindowSystemContract.RS1_SetSizeIsReportedAsResized` | contract (Fake, Sdl3) | `waitEvents(1 s)` |
| AC-2 | `…RS2_SetSameSizeProducesNoEvent` | contract | |
| AC-3 | `…RS3_SetSizeRejectsBadInputWithoutEvents` | contract | |
| AC-4 | `…RS4_UserResizeIsReportedAsResized` | contract | trait `userResize` |
| AC-5 | `…RS5_PendingResizesAreCoalesced` | contract | |
| AC-6 | `…RS6_ResizesOfDestroyedWindowsAreDropped` | contract | |
| AC-7 | `FakeWindowSystem.RS7_UserResizeValidatesLikeSetSize` | unit | fake only |

**Traits:**
- **Fake:** `userResize` calls `FakeWindowSystem::userResize`.
- **SDL:** `userResize` calls `SDL_SetWindowSize` directly on the window's SDL handle, found through
  `sdlWindowId`. That is a change that does not go through the port, which is the situation rule 5
  covers. On the `dummy` driver SDL sends `SDL_EVENT_WINDOW_RESIZED` at once, just as a real resize
  does.

**"Waiting for events":** `waitEvents(1 s)`. On the fake and on `dummy` the event is already pending,
so the test never actually waits. On a real OS backend the same test would also pass.

## 7. Non-Functional Requirements
- No new dependency. No additional allocation per event beyond the existing event vector.

## 8. Architectural Impact
- `WindowEvent` gains its second alternative. The fake's `std::visit` over events keeps compiling
  because both alternatives have `.window`. Users who match only `CloseRequested` (such as
  `hello_window`) are unaffected.
- The contract-trait interface grows by `userResize`. Both existing instantiations (fake and SDL) are
  updated in Red.

## 9. Open Questions
All resolved as recommended (2026-11-01, Emilio Pomares).
- **Q1 — Does `setSize` produce a `Resized` event?** *Recommended: yes* (rule 2). Applications then
  have one code path for every size change, and it matches SDL, which sends the event for program
  resizes too.
- **Q2 — `setSize` to the current size?** *Recommended: success, no event* (rule 4). An event means
  the size changed. SDL drops the same-size event itself, and the fake does the same.
- **Q3 — Event name?** *Recommended: `Resized`*, matching `CloseRequested` (the event types carry no
  `Window` prefix; they already live in the `WindowEvent` variant).
- **Q4 — Fake control name?** *Recommended: `userResize(id, size)`*, which reads as "the user resized
  it", parallel to `requestClose`.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0010): …`):
   - **Port:** `Resized`, the `WindowEvent` alternative, and `setSize`.
   - **Stubs:**
     - fake `setSize` and `userResize` return success and do nothing;
     - SDL `setSize` returns success and does nothing;
     - SDL translation stays as it is, so `RESIZED` is ignored.
   - **Tests:** RS1–RS6 appended to the contract suite (existing tests untouched except the traits
     comment); the `userResize` trait added to the fake and SDL instantiations; fake AC-7. Lint test
     code before freezing.
   - **Expected Red:**
     - Fail, on both implementations: RS1, RS4, RS5 (no event, size unchanged), RS6 (B's event
       missing) and RS3 (the stubs succeed instead of returning `InvalidArgument` / `NotFound`).
     - Fail, fake only: RS7 (the `userResize` stub succeeds).
     - Pass as guard: RS2 (no event is correct for a same-size resize, which is where the stubs start).
2. **Green:**
   - Fake: validate the id and size; if the size differs, update it, remove any pending `Resized` for
     that window, and queue the new one. `userResize` shares the same code path.
   - SDL: validate; call `SDL_SetWindowSize` (and report its error if it fails); translate
     `SDL_EVENT_WINDOW_RESIZED`.
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on the SDL RS tests; update docs (EPIC-005, Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/window_system.hpp` | modify | `Resized`, variant, `setSize` |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | `setSize`, `userResize` |
| `modules/platform/tests/contract/…/window_system_contract.hpp` | modify | RS1–RS6, trait doc |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | trait, AC-7 |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | `setSize`, translation |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | trait |
| `examples/hello_window/tests/hello_window_test.cpp` | modify | `setSize` in its two port test doubles (forced by the new pure virtual; added in Red) |
| `docs/epics/EPIC-005-window-resizing.md` | modify | Status |

### 10.3 Risks / alternatives considered
- **Checked in SDL 3.4.18 source:**
  - the `dummy` driver's `SetWindowSize` sends `SDL_EVENT_WINDOW_RESIZED` synchronously;
  - SDL drops `RESIZED` when the size doesn't change;
  - SDL clamps the requested size to the window's min/max limits before applying it.

  So the SDL tests can be exact, and spec 2's limits will show up through `Resized` carrying the
  applied size.
- **Real OS backends are asynchronous** (macOS and Wayland apply the size later). That's why rule 2
  allows a late event and the contract waits for it with `waitEvents`. The contract runs only on the
  fake and `dummy`, so the manual check in EPIC-005 spec 4 is what exercises real behavior.
- **Fixed-size SDL windows:** until spec 2, SDL windows are created without `SDL_WINDOW_RESIZABLE`.
  `SDL_SetWindowSize` still works on them (the flag only controls user dragging).
- **Alternative considered:** `setSize` updating `size()` immediately without an event. Rejected: it
  can't be honored on asynchronous OS backends, and it would give applications two code paths.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.1
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL RS tests
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `…RS1_SetSizeIsReportedAsResized` (Fake, Sdl3)
- [x] AC-2 → `…RS2_SetSameSizeProducesNoEvent` (Fake, Sdl3)
- [x] AC-3 → `…RS3_SetSizeRejectsBadInputWithoutEvents` (Fake, Sdl3)
- [x] AC-4 → `…RS4_UserResizeIsReportedAsResized` (Fake, Sdl3)
- [x] AC-5 → `…RS5_PendingResizesAreCoalesced` (Fake, Sdl3)
- [x] AC-6 → `…RS6_ResizesOfDestroyedWindowsAreDropped` (Fake, Sdl3)
- [x] AC-7 → `FakeWindowSystem.RS7_UserResizeValidatesLikeSetSize`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Q1–Q4 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: 11 tests fail on assertions: RS1, RS3, RS4, RS5 and RS6 for both Fake and Sdl3 (missing `Resized` events; stubs succeed where `InvalidArgument` / `NotFound` is required), and RS7 (fake). RS2 (×2) passes as the planned guard; the other 84 tests pass. File list amended: `hello_window`'s two test doubles gain `setSize` (forwarding, and `NotFound`), forced by the new pure virtual. The fake's `userResize` stub delegates to the `setSize` stub (shared path, rule 8). `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-01 | **Amendment A** (found in Green: SDL coalesces pending `RESIZED` events per window): rule 9 added, §4.3 and AC-5 changed, RS5 renamed `RS5_PendingResizesAreCoalesced` and now expects one coalesced event. Out of Scope keeps only throttling. Per CLAUDE.md §3 the corrected test goes through Red again. | Emilio Pomares |
| 2026-11-01 | **Green + Refactor**: fake `setSize` validates, updates the size, replaces any pending `Resized` for the window and queues the new one (`userResize` shares it). SDL `setSize` validates and calls `SDL_SetWindowSize`; `SDL_EVENT_WINDOW_RESIZED` (data1, data2) translates to `Resized`. The size-validation message is shared by `createWindow` and `setSize`. `tools/check.sh` exit 0 (97/97 tests); `ctest --preset asan` 97/97; macOS `leaks`: 0 leaks for Sdl3 RS1–RS6. `git diff c0920e8 -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
| 2026-11-01 | Acknowledged (including the `hello_window` test-double file-list amendment); Status → Done | Emilio Pomares |
