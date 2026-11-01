# SPEC-0006: Window close-request events through the window port

| Field | Value |
|-------|-------|
| Status | Draft |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0004 (ports & adapters), ADR-0007 (device-agnostic input: raw OS events stay in the platform layer), ADR-0010 (UI thread) |
| Depends on | SPEC-0003, SPEC-0005 |

> **Scope Gate checklist**
> - [x] One concern: the application learns that the user asked to close a window
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (`platform`, `backends/platform_sdl3`); no new dependency or edge
> - [x] Out-of-scope section is filled in
> - [ ] Open Questions section is empty (four confirmations pending, see §9)

---

## 1. Problem / Motivation
Windows can be created but the application cannot react to anything the user does to them. The
smallest useful event is the **close request**: the user clicked the close button, pressed ⌘W/Alt+F4,
or the OS asked the window to close. Without it, `hello_window` cannot exit cleanly. This spec adds a
way to **poll** window events through the port, starting with this one event type, implemented by the
fake and by the SDL3 adapter, and pinned down by new contract tests.

## 2. Scope
- Port: `WindowEvent` (a `std::variant`, initially only `CloseRequested{WindowId}`) and
  `WindowSystem::pollEvents()`.
- Fake: implements `pollEvents()`, plus a test-control method `requestClose(WindowId)` that queues a
  close request, as if the OS had sent one.
- SDL3 adapter: `pollEvents()` drains SDL's event queue and translates
  `SDL_EVENT_WINDOW_CLOSE_REQUESTED`. A small public helper `sdlWindowId(system, id)` returns SDL's
  window id (a plain integer) so tests and apps can address SDL directly (§9 Q2).
- Contract suite: new tests for event behavior, run against **both** the fake and SDL. The contract
  traits gain `requestClose(WindowSystem&, WindowId)` so each implementation can simulate the OS request
  its own way.

## 3. Out of Scope
- Any other event: resize, move, focus, minimize, mouse, keyboard, gamepad, text. Each comes in its own
  spec. Input events will feed the intent pipeline (ADR-0007), not this list.
- Callbacks, listeners, or async delivery (event push). `pollEvents()` is pull-based. Higher layers
  (core) can build push delivery on top later.
- Automatically closing windows on request: the application decides (§4.2 rule 3).
- SDL's application-level `SDL_EVENT_QUIT`. It's discarded for now and comes with the app main-loop
  spec.
- Blocking or timeout-based waiting (`waitEvents`).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp (additions)
namespace coil::platform {

/// The OS or the user asked to close a window (close button, ⌘W, Alt+F4, …).
/// The window stays open until the application destroys it.
struct CloseRequested {
    WindowId window;

    friend constexpr bool operator==(CloseRequested, CloseRequested) = default;
};

/// An event about a window. New alternatives are added by later specs.
using WindowEvent = std::variant<CloseRequested>;

class WindowSystem {
public:
    // … existing members …

    /// Returns all events that occurred since the previous call, in order. Each event is returned once.
    [[nodiscard]] virtual std::vector<WindowEvent> pollEvents() = 0;
};

} // namespace coil::platform
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp (addition)
/// Test control: queues CloseRequested for `id`, as if the OS had sent it.
/// Fails with NotFound if `id` is not a live window.
[[nodiscard]] Expected<void> requestClose(WindowId id);
```
```cpp
// backends/platform_sdl3/include/coil/platform_sdl3/sdl_window_system.hpp (addition)
/// SDL's window id (SDL_WindowID) of a window created by an SDL window system, so callers can address
/// the window through SDL directly. Fails with InvalidArgument if `system` was not created by
/// createSdlWindowSystem, and with NotFound if `id` is not a live window.
[[nodiscard]] Expected<std::uint32_t> sdlWindowId(const platform::WindowSystem& system,
                                                  platform::WindowId id);
```
```cpp
// modules/platform/tests/contract/coil/platform/window_system_contract.hpp (traits extended)
template <> struct WindowSystemContractTraits<Impl> {
    static std::unique_ptr<WindowSystem> make();
    /// Simulates the OS asking to close `id` (fake: requestClose; SDL: SDL_PushEvent).
    static void requestClose(WindowSystem& system, WindowId id);
};
```

### 4.2 Behavioral rules
1. `pollEvents()` MUST return every event that occurred since the previous call, in the order they
   occurred, and MUST NOT return any event twice.
2. A close request for a live window MUST produce exactly one `CloseRequested{thatWindow}`.
3. A close request MUST NOT destroy, hide or otherwise change the window. It stays in `windows()` and
   keeps its title and size until the application calls `destroyWindow`.
4. Events for a window destroyed before `pollEvents()` is called MUST NOT be returned. No event ever
   names a window that is not live at poll time.
5. SDL adapter: events SDL reports that this spec does not map (including `SDL_EVENT_QUIT` and events
   for windows this system did not create) are discarded.
6. `pollEvents()` is non-blocking and is called on the UI/main thread (SDL requirement, ADR-0010).
7. `sdlWindowId` MUST return the id SDL uses for that window (`SDL_GetWindowID`), so that an SDL event
   with that `windowID` is attributed to the matching `WindowId`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| No events | `pollEvents()` returns an empty vector |
| Several close requests for the same window before polling | One event per request, in order (not deduplicated) |
| Fake `requestClose` on an unknown or destroyed id | `NotFound`; nothing queued |
| `sdlWindowId` with a non-SDL system (e.g. the fake) | `InvalidArgument` |
| `sdlWindowId` with an unknown id | `NotFound` |

### 4.4 Threading and lifetime
Same as SPEC-0003/0005: UI/main thread only. Events reference windows by `WindowId`, which never
dangles (rule 4 plus never-reused ids).

### 4.5 Async behavior
Not applicable (pull-based, non-blocking).

### 4.6 Automation and agent exposure
Not applicable yet.

## 5. Acceptance Criteria
Contract criteria (AC-1…AC-5) are new `WindowSystemContract` tests, run against both the fake and SDL.

- **AC-1** — Given a window system with one window and nothing requested, `pollEvents()` is empty.
- **AC-2** — Given window A and a close request for A, `pollEvents()` returns exactly
  `{CloseRequested{A}}`, and a second `pollEvents()` is empty.
- **AC-3** — Given windows A and B, close requests for B then A, `pollEvents()` returns
  `{CloseRequested{B}, CloseRequested{A}}`.
- **AC-4** — After a close request for A has been polled, A is still in `windows()` and `title(A)`
  still succeeds.
- **AC-5** — Given a close request for A and then `destroyWindow(A)` before polling, `pollEvents()`
  returns no event for A. An event for live window B queued alongside it is still returned.
- **AC-6** — Fake: `requestClose` on an unknown id (`WindowId{999}`) returns `NotFound` and queues
  nothing.
- **AC-7** — SDL: `sdlWindowId` returns a non-zero id equal to the `windowID` that SDL reports for that
  window, `NotFound` for an unknown id, and `InvalidArgument` when given a `FakeWindowSystem`.
- **AC-8** — The existing contract tests (SPEC-0003 AC-1…AC-7) are unchanged and still pass for both
  implementations.

## 6. Test Plan
| AC | Test name | Type | Implementations |
|----|-----------|------|-----------------|
| AC-1 | `WindowSystemContract.EV1_EventsEmptyWhenNothingHappened` | contract | Fake, Sdl3 |
| AC-2 | `WindowSystemContract.EV2_CloseRequestIsDeliveredOnce` | contract | Fake, Sdl3 |
| AC-3 | `WindowSystemContract.EV3_EventsKeepOrder` | contract | Fake, Sdl3 |
| AC-4 | `WindowSystemContract.EV4_CloseRequestDoesNotCloseWindow` | contract | Fake, Sdl3 |
| AC-5 | `WindowSystemContract.EV5_EventsForDestroyedWindowsAreDropped` | contract | Fake, Sdl3 |
| AC-6 | `FakeWindowSystem.AC6_RequestCloseOnUnknownIdIsNotFound` | unit | Fake |
| AC-7 | `SdlWindowSystem.AC7_SdlWindowIdMatchesSdl` | backend unit | SDL (`dummy` driver) |
| AC-8 | existing `Fake/…` and `Sdl3/…` AC1…AC7 contract tests | contract | both |

**Naming:** the contract suite already has `AC1_…AC7_` tests from SPEC-0003. To keep names unique and
traceable, this spec's contract tests use the prefix **`EV<n>_`** (event criteria), and
`modules/platform/tests/.clang-tidy`'s `ClassIgnoredRegexp` is widened to `^(AC|EV)[0-9]+_…$`.

The SDL traits simulate the OS by pushing a `SDL_EVENT_WINDOW_CLOSE_REQUESTED` event with
`windowID = sdlWindowId(…)` via `SDL_PushEvent`.

## 7. Non-Functional Requirements
- `pollEvents()` with an empty queue does not allocate beyond the returned empty vector.
- No new dependency.

## 8. Architectural Impact
- **Breaking change to the port:** a new pure virtual. All implementations (fake and SDL) are updated
  in this spec. There are no external implementations yet.
- `WindowEvent` is a `std::variant`. Adding alternatives in later specs is source-compatible for code
  using `std::get_if`/`std::holds_alternative`, and intentionally breaks exhaustive `std::visit` code so
  new events can't be ignored silently.
- First backend-specific public helper (`sdlWindowId`). It exposes a plain integer, never SDL types
  (rule 7 of SPEC-0005 still holds).

## 9. Open Questions
Recommended answers are written into the spec. **Approving the spec confirms them.**
- **Q1 — How are events represented?** *Recommended: `std::variant` of small structs.* It's type-safe
  and each event carries exactly its own data. The alternative, one struct with an `enum kind` plus
  optional fields, gets messy as events with different payloads arrive.
- **Q2 — How does the SDL test simulate an OS close request?** *Recommended: a public helper
  `sdlWindowId(system, id)`.* The test pushes a real SDL event for that window. It's also useful for
  apps mixing raw SDL code (for example an SDL-based debug UI). The alternative, finding the SDL window
  by its title inside the test, is fragile and leaves apps without a mapping.
- **Q3 — Events for windows destroyed before polling?** *Recommended: dropped (rule 4).* An event never
  names a dead window, so handlers never get `NotFound` surprises.
- **Q4 — Pull (`pollEvents`) or push (callbacks)?** *Recommended: pull, for now.* The UI thread's frame
  loop polls (ADR-0010). Push delivery to widgets belongs in `core` later, built on this.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0006): …`):
   - **Port:** add `CloseRequested`, `WindowEvent` and `pollEvents()` to the header.
   - **Stubs:**
     - Fake: `pollEvents()` returns `{}`, and `requestClose` returns success without queuing.
     - SDL: `pollEvents()` returns `{}`, and `sdlWindowId` returns `0`.
   - **Tests:** new `EV1…EV5` contract tests appended to the suite (existing tests untouched),
     `requestClose` added to both traits specializations, the AC-6 and AC-7 tests, and the widened
     `ClassIgnoredRegexp`. Lint and simulate before freezing.
   - **Expected Red:**
     - Fail: EV2, EV3 and EV4 for both implementations (no events delivered), AC-6, AC-7.
     - Pass as guards: EV1 and EV5 (an empty result is correct there), and AC-8 (existing tests).
2. **Green:**
   - Fake: event queue; `requestClose` validates and queues; `pollEvents` filters by live windows and
     drains.
   - SDL: `SDL_PumpEvents` and `SDL_PollEvent` loop; map `SDL_EVENT_WINDOW_CLOSE_REQUESTED` by
     `windowID`; keep only live windows; `sdlWindowId` via `dynamic_cast` to the private class plus
     `SDL_GetWindowID`.
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks`, docs (EPIC-002, Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/window_system.hpp` | modify | Events API |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | Queue, `requestClose` |
| `modules/platform/tests/contract/…/window_system_contract.hpp` | modify | Append EV1…EV5; traits doc |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | Traits `requestClose`; AC-6 |
| `modules/platform/tests/.clang-tidy`, `backends/platform_sdl3/tests/.clang-tidy` | modify | `^(AC\|EV)[0-9]+_…$` |
| `backends/platform_sdl3/include/…/sdl_window_system.hpp` | modify | `sdlWindowId` |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | Event translation, `sdlWindowId` |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | Traits `requestClose` (SDL_PushEvent); AC-7 |
| `docs/epics/EPIC-002-hello-window.md` | modify | Status |

### 10.3 Risks / alternatives considered
- **The `dummy` driver and `SDL_PushEvent`:** pushed events go through SDL's normal queue with any
  driver, so the dummy driver is enough. This will be verified with a probe before freezing.
- **`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE`:** SDL may add an `SDL_EVENT_QUIT` when the last window gets
  a close request. It's discarded (rule 5), and AC-2's "exactly one event" proves nothing leaks
  through.
- **Typed-test regex widening** is a test-config change; it's listed here so approving the spec
  approves it.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL event tests
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `Fake/…EV1_EventsEmptyWhenNothingHappened`, `Sdl3/…EV1_…`
- [ ] AC-2 → `…EV2_CloseRequestIsDeliveredOnce` (both)
- [ ] AC-3 → `…EV3_EventsKeepOrder` (both)
- [ ] AC-4 → `…EV4_CloseRequestDoesNotCloseWindow` (both)
- [ ] AC-5 → `…EV5_EventsForDestroyedWindowsAreDropped` (both)
- [ ] AC-6 → `FakeWindowSystem.AC6_RequestCloseOnUnknownIdIsNotFound`
- [ ] AC-7 → `SdlWindowSystem.AC7_SdlWindowIdMatchesSdl`
- [ ] AC-8 → existing `Fake/…` and `Sdl3/…` AC1…AC7

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
