# SPEC-0009: `WindowSystem::waitEvents(timeout)`: block until an event or a timeout

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) (also the base of EPIC-003's idle mode) |
| Related ADRs | ADR-0003 (deterministic tests), ADR-0010 (UI thread) |
| Depends on | SPEC-0006 (events), SPEC-0008 (`Clock`, `FakeClock`) |

> **Scope Gate checklist**
> - [x] One concern: wait for window events without busy-looping, bounded by a timeout
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (`platform`, `backends/platform_sdl3`); no new dependency or edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
`pollEvents()` never blocks, so a loop built on it either spins a CPU core or sleeps blindly. The
decided approach (EPIC-002 notes, option B) is to **block until something happens**, bounded by a
timeout. This is the idle mode of the future main loop (EPIC-003). This spec adds `waitEvents(timeout)`
to the window port, implements it for SDL3 (`SDL_WaitEventTimeout`) and for the fake, and makes the
fake's waiting deterministic through `FakeClock`.

## 2. Scope
- Port: `waitEvents(Clock::Duration timeout)`.
- SDL3 adapter: returns pending events at once; otherwise waits in `SDL_WaitEventTimeout`, then drains.
- Fake:
  - "Waiting" advances a `FakeClock` instead of sleeping.
  - An optional **wait hook** (`setWaitHook`) runs when the fake would block, so a test can make an event
    arrive "during" the wait.
  - Constructors: the existing default one (using its own clock) and one taking an external
    `FakeClock&`.
- Contract tests (both implementations), plus fake-specific tests for time and the hook.

## 3. Out of Scope
- Waiting forever (no timeout), and waking the wait from another thread (`wake()`). Both belong to
  EPIC-003's main loop.
- Any change to which events exist (still only `CloseRequested`).
- Measuring real elapsed time for SDL (no wall-clock assertions in tests; §10.3).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp (addition)
#include <coil/platform/clock.hpp>

class WindowSystem {
public:
    // … existing members …

    /// Like pollEvents(), but if no event is pending it blocks until one arrives or `timeout`
    /// elapses. May return early with no events (e.g. woken by an event this port does not map);
    /// callers loop. A zero or negative timeout never blocks.
    [[nodiscard]] virtual std::vector<WindowEvent> waitEvents(Clock::Duration timeout) = 0;
};
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp (additions)
class FakeWindowSystem final : public WindowSystem {
public:
    FakeWindowSystem();                          // uses its own FakeClock (existing behavior)
    explicit FakeWindowSystem(FakeClock& clock); // waits advance `clock`

    [[nodiscard]] std::vector<WindowEvent> waitEvents(Clock::Duration timeout) override;

    /// Test control: runs each time waitEvents would block (no pending events, timeout > 0),
    /// before time advances. If it queues events, waitEvents returns them without advancing time.
    void setWaitHook(std::function<void()> hook);

    /// The clock that waits advance.
    [[nodiscard]] const FakeClock& clock() const;
};
```

### 4.2 Behavioral rules
1. If events are pending, `waitEvents` MUST return them immediately (same content, order and
   once-only rule as `pollEvents`, SPEC-0006).
2. If none are pending and `timeout > 0`, it MUST block until an event is available or `timeout` has
   elapsed, then return the available events (possibly none).
3. If `timeout <= 0`, it MUST NOT block. It behaves exactly like `pollEvents()`.
4. It MAY return early with an empty result. Callers must not assume an empty result means the full
   timeout elapsed. The fake never returns early, so tests stay exact.
5. Fake: with no pending events and `timeout > 0`, it MUST call the wait hook once (if set). If events
   are now pending, it returns them **without** advancing the clock. Otherwise it MUST advance its clock
   by exactly `timeout` and return empty.
6. Fake: with pending events, or `timeout <= 0`, the hook MUST NOT be called and the clock MUST NOT
   move.
7. SDL: the timeout is rounded **up** to whole milliseconds and clamped to `Sint32` range. Negative or
   zero values never reach `SDL_WaitEventTimeout` (SDL treats `-1` as "wait forever").

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `timeout` = 0 or negative | Non-blocking poll |
| Huge `timeout` (above ~24 days) | SDL: clamped to the `Sint32` maximum in milliseconds |
| `timeout` of 1 ns (SDL) | Rounded up to 1 ms (never 0, which would be a poll) |
| Woken by an unmapped SDL event | Returns early, possibly empty (rule 4) |

### 4.4 Threading and lifetime
UI/main thread only, like the rest of the port. An external `FakeClock&` must outlive the fake.

### 4.5 Async behavior
Not applicable (synchronous blocking call with a bounded timeout).

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
Contract criteria (AC-1…AC-4) run against **both** the fake and SDL (`dummy` driver).
- **AC-1** — Given a pending close request for window A, `waitEvents(1s)` returns exactly
  `{CloseRequested{A}}`.
- **AC-2** — Given no pending events, `waitEvents(10ms)` returns an empty result, and `waitEvents(0)`
  returns an empty result.
- **AC-3** — An event returned by `waitEvents` is not returned again by a following `pollEvents()`.
- **AC-4** — Given a negative timeout (`-1ms`), `waitEvents` returns pending events if there are any,
  and returns empty immediately otherwise. It never blocks. (Each SDL test has a CTest `TIMEOUT` of
  30 s, so a wrong "wait forever" fails instead of hanging.)
- **AC-5** — Fake: with no events, `waitEvents(100ms)` advances its clock by exactly 100 ms, and
  `waitEvents(0ms)` and `waitEvents(-5ms)` do not move it.
- **AC-6** — Fake: with a pending event, `waitEvents(100ms)` returns it, the clock does not move, and
  the wait hook is not called.
- **AC-7** — Fake: a wait hook that calls `requestClose(A)` makes `waitEvents(100ms)` return
  `{CloseRequested{A}}` with the clock unchanged. The hook was called exactly once.
- **AC-8** — Fake: constructed with an external `FakeClock`, waits advance that clock (it's the one
  `clock()` returns). Default-constructed, it uses its own clock, starting at the time point zero.

## 6. Test Plan
| AC | Test name | Type | Implementations |
|----|-----------|------|-----------------|
| AC-1 | `WindowSystemContract.WE1_WaitReturnsPendingEvents` | contract | Fake, Sdl3 |
| AC-2 | `WindowSystemContract.WE2_WaitTimesOutEmpty` | contract | Fake, Sdl3 |
| AC-3 | `WindowSystemContract.WE3_WaitedEventsAreDeliveredOnce` | contract | Fake, Sdl3 |
| AC-4 | `WindowSystemContract.WE4_NegativeTimeoutNeverBlocks` | contract | Fake, Sdl3 |
| AC-5 | `FakeWindowSystem.AC5_WaitAdvancesClockByTimeout` | unit | Fake |
| AC-6 | `FakeWindowSystem.AC6_PendingEventsReturnWithoutWaiting` | unit | Fake |
| AC-7 | `FakeWindowSystem.AC7_WaitHookCanDeliverAnEvent` | unit | Fake |
| AC-8 | `FakeWindowSystem.AC8_UsesInjectedOrOwnClock` | unit | Fake |

Contract tests reuse the existing `requestClose` traits (SPEC-0006). No existing test changes.

## 7. Non-Functional Requirements
- An idle loop built on `waitEvents` uses no CPU while waiting (SDL blocks in the OS).
- No new dependency.

## 8. Architectural Impact
- Breaking port change (new pure virtual). Both implementations are updated here; there are no
  external implementations.
- `platform`'s window header now includes `clock.hpp` (same module).
- First fake that depends on another fake (`FakeWindowSystem` → `FakeClock`), both in
  `coil::platform_testing`.

## 9. Open Questions
*None.* Q1–Q4 confirmed as recommended by approval on 2026-11-01. Rationale kept below.
- **Q1 — Negative timeout?** *Recommended: treat it as zero (non-blocking).* It's the common
  convention, and it avoids SDL's "-1 = forever" trap. The alternative, an error, would force
  `Expected` on a hot-path call.
- **Q2 — May `waitEvents` return early and empty?** *Recommended: yes (rule 4).* SDL wakes up for any OS
  event, including ones we don't map yet. Hiding that would mean re-waiting with recalculated deadlines
  in the adapter. Callers loop anyway.
- **Q3 — How do tests make an event arrive "during" a fake wait?** *Recommended: a wait hook
  (`setWaitHook`).* It's simple and deterministic. The alternative, scheduling events at fake times
  ("at t+30ms, close A"), is more powerful but more machinery than needed now.
- **Q4 — Keep the default `FakeWindowSystem()` constructor?** *Recommended: yes, with its own clock.*
  All existing tests keep compiling unchanged; tests that care about time pass an external
  `FakeClock`.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0009): …`):
   - **Port** method.
   - **Stubs:**
     - Fake: `waitEvents` returns `{}`, never calls the hook or moves the clock; constructors and
       `clock()` work, so AC-8 can fail on behavior, not on construction.
     - SDL: `waitEvents` returns `{}`.
   - **Tests:** WE1–WE4 appended to the contract suite (existing tests untouched), fake tests
     AC-5…AC-8, and CTest `TIMEOUT 30` on the SDL test executable's tests. Lint and simulate before
     freezing.
   - **Expected Red:**
     - Fail: WE1 and WE3 (both implementations), AC-5, AC-6, AC-7, and AC-8 (the external clock is
       not advanced).
     - Pass as guards: WE2 and WE4 (an empty result is correct when nothing is pending, which is the
       case those tests start in).
2. **Green:**
   - Fake: pending events → return; timeout ≤ 0 → poll; otherwise hook, re-check, advance the clock.
   - SDL: poll first; if empty and timeout > 0, compute milliseconds (ceil, clamp), call
     `SDL_WaitEventTimeout`, translate that event plus anything else pending (shared translation with
     `pollEvents`).
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on SDL WE tests, docs (EPIC-002, Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/window_system.hpp` | modify | `waitEvents` |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | Clock, hook, `waitEvents` |
| `modules/platform/tests/contract/…/window_system_contract.hpp` | modify | Append WE1–WE4 |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | AC-5…AC-8 |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | `waitEvents` |
| `backends/platform_sdl3/tests/CMakeLists.txt` | modify | `TIMEOUT 30` for discovered tests |
| `docs/epics/EPIC-002-hello-window.md` | modify | Status |

### 10.3 Risks / alternatives considered
- **Real waiting in SDL contract tests:** WE2 makes the SDL implementation wait up to 10 ms for real.
  That's a bounded wait inside a backend test, not a sleep used for synchronization, and nothing
  asserts on elapsed time. The deterministic timing assertions are on the fake (AC-5…AC-7).
- **`dummy` driver and `SDL_WaitEventTimeout`:** pushed events wake the wait under any driver; this will
  be confirmed by WE1 in Green. If the dummy driver can't wait (e.g. it returns immediately), WE2 still
  passes because rule 4 allows early returns.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes; macOS `leaks` reports 0 leaks for the SDL WE tests
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `…WE1_WaitReturnsPendingEvents` (Fake, Sdl3)
- [ ] AC-2 → `…WE2_WaitTimesOutEmpty` (Fake, Sdl3)
- [ ] AC-3 → `…WE3_WaitedEventsAreDeliveredOnce` (Fake, Sdl3)
- [ ] AC-4 → `…WE4_NegativeTimeoutNeverBlocks` (Fake, Sdl3)
- [ ] AC-5 → `FakeWindowSystem.AC5_WaitAdvancesClockByTimeout`
- [ ] AC-6 → `FakeWindowSystem.AC6_PendingEventsReturnWithoutWaiting`
- [ ] AC-7 → `FakeWindowSystem.AC7_WaitHookCanDeliverAnEvent`
- [ ] AC-8 → `FakeWindowSystem.AC8_UsesInjectedOrOwnClock`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: 10 tests fail on assertions: WE1, WE3 and WE4 for Fake and Sdl3, and AC-5…AC-8. WE4 is stronger than planned (it also checks that a pending event is returned with a negative timeout, so it fails instead of being a guard); WE2 (×2) passes as a guard. SDL tests get CTest `TIMEOUT 30` via a new `coil_add_tests(... TIMEOUT)` option. Test code clang-tidy clean before freezing. | Emilio Pomares |
