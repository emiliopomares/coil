# SPEC-0008: `Clock` port with system and fake clocks

| Field | Value |
|-------|-------|
| Status | Done (pending acknowledgement) |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (port, `SystemClock`), `coil::platform_testing` (`FakeClock`) |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0003 (fakes, deterministic tests), ADR-0004 (ports), ADR-0014 (timeouts will use it) |
| Depends on | SPEC-0002, SPEC-0003 (test-support and contract conventions) |

> **Scope Gate checklist**
> - [x] One concern: a replaceable source of monotonic time
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules (`platform` only, including its test support); no new dependency or edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
`waitEvents(timeout)` (next spec), command timeouts (ADR-0014) and animations all depend on time. Unit
tests must not depend on real time (CLAUDE.md §3: no sleeps, no wall-clock). This spec introduces a
**`Clock` port** so code asks an injected clock for the time: production uses a `SystemClock`, and tests
use a `FakeClock` that only moves when the test advances it.

## 2. Scope
- `Clock` port in `coil::platform`: `now()` returns a monotonic time point.
- `SystemClock`: real implementation over `std::chrono::steady_clock` (standard library only, so it
  lives in `modules/platform`; §9 Q2).
- `FakeClock` in `coil::platform_testing`: starts at a chosen time, moves only through `advance(d)`.
- `ClockContract<T>`: a typed contract suite every clock must pass, instantiated for both clocks.

## 3. Out of Scope
- Sleeping or waiting (`sleepFor`, timers, deadlines). Waiting is done by the component that waits
  (e.g. `waitEvents` in the next spec), which receives a `Clock`.
- Wall-clock or calendar time (dates, time zones, `system_clock`).
- High-resolution profiling clocks, frame timing, animation curves.
- Injecting the clock into existing components. The window system starts using it in the
  `waitEvents` spec.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/clock.hpp
#pragma once

#include <chrono>

namespace coil::platform {

/// Port: source of monotonic time. Inject it wherever code needs "now", so tests can control time.
class Clock {
public:
    using Duration = std::chrono::steady_clock::duration;
    using TimePoint = std::chrono::steady_clock::time_point;

    virtual ~Clock();

    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;
    Clock(Clock&&) = delete;
    Clock& operator=(Clock&&) = delete;

    /// Current time. Never decreases between calls on the same clock.
    [[nodiscard]] virtual TimePoint now() const = 0;

protected:
    Clock() = default;
};

} // namespace coil::platform
```
```cpp
// modules/platform/include/coil/platform/system_clock.hpp
namespace coil::platform {

/// Real monotonic time (std::chrono::steady_clock).
class SystemClock final : public Clock {
public:
    [[nodiscard]] TimePoint now() const override;
};

} // namespace coil::platform
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_clock.hpp
namespace coil::platform::testing {

/// Clock for tests: time stands still until the test calls advance().
class FakeClock final : public Clock {
public:
    explicit FakeClock(TimePoint start = TimePoint{});

    [[nodiscard]] TimePoint now() const override;

    /// Moves time forward by `step`. Fails with InvalidArgument (time unchanged) if `step` is negative.
    [[nodiscard]] Expected<void> advance(Duration step);
};

} // namespace coil::platform::testing
```
```cpp
// modules/platform/tests/contract/coil/platform/clock_contract.hpp
// Typed suite ClockContract<Impl>; an implementation opts in by specializing
//   template <> struct ClockContractTraits<Impl> { static std::unique_ptr<Clock> make(); };
```

### 4.2 Behavioral rules
1. For any clock, successive `now()` calls MUST never return a smaller time point (monotonic).
2. `SystemClock::now()` MUST equal `std::chrono::steady_clock::now()` at the moment of the call, so a
   reading taken between two `steady_clock` readings lies between them.
3. `FakeClock` MUST start at the constructor's `start` (default: the time point zero) and MUST NOT change
   except through `advance`.
4. `FakeClock::advance(step)` with `step >= 0` MUST increase `now()` by exactly `step`. Steps accumulate.
5. `FakeClock::advance(step)` with `step < 0` MUST return `InvalidArgument` and leave time unchanged
   (keeping rule 1).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `advance(Duration::zero())` | Success; time unchanged |
| `advance(negative)` | `InvalidArgument`; time unchanged |
| Values of `FakeClock` vs. real `steady_clock` | Unrelated; never mix time points from different clocks |

### 4.4 Threading and lifetime
`SystemClock::now()` is thread-safe. `FakeClock` is used from the test's thread only. Components hold a
`const Clock&` that must outlive them; the composition root owns the clock.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1** — Contract (both clocks): over 1000 successive `now()` calls, no reading is smaller than the
  previous one.
- **AC-2** — `FakeClock{}` reports the time point zero, and two readings with no `advance` in between are
  equal. `FakeClock{start}` reports `start`.
- **AC-3** — `advance(5ms)` then `advance(0ms)` then `advance(10ms)` moves `now()` by exactly 15 ms in
  total, and each step is visible immediately.
- **AC-4** — `advance(-1ms)` returns `InvalidArgument` and `now()` is unchanged.
- **AC-5** — `SystemClock`: within at most 10,000,000 successive `now()` calls (no sleeping), a reading
  strictly greater than the first appears. Time really moves.
- **AC-6** — `SystemClock`: for `before = steady_clock::now()`, `t = clock.now()`,
  `after = steady_clock::now()`, then `before <= t <= after`.

## 6. Test Plan
| AC | Test name | Type |
|----|-----------|------|
| AC-1 | `Fake/ClockContract.CL1_NowNeverDecreases`, `System/ClockContract.CL1_NowNeverDecreases` | contract |
| AC-2 | `FakeClock.AC2_StartsAtStartAndStandsStill` | unit |
| AC-3 | `FakeClock.AC3_AdvanceMovesByExactlyTheStep` | unit |
| AC-4 | `FakeClock.AC4_NegativeAdvanceIsRejected` | unit |
| AC-5 | `SystemClock.AC5_TimeMoves` | unit (no sleep; bounded loop) |
| AC-6 | `SystemClock.AC6_TracksSteadyClock` | unit |

**Test-name convention for contract suites:** each suite uses its own prefix (`AC`/`EV` in the window
suite, `CL` here). Instead of widening the typed-test exemption once per suite, the test lint override
regex becomes `^[A-Z]+[0-9]+_[A-Za-z0-9]+$` (upper-case prefix + number + `_` + name). It's a test-config
change, approved together with this spec.

## 7. Non-Functional Requirements
- `now()` does not allocate.
- Standard library only (`<chrono>`); no OS headers.

## 8. Architectural Impact
- New port in `platform` (OVERVIEW §3 already lists `Clock` among the platform ports).
- First port implementation that lives in `modules/` rather than `backends/` (§9 Q2). This is allowed
  because it uses only the C++ standard library, which `modules/` may use (ADR-0004, layering
  checker). OVERVIEW §3 notes the exception.

## 9. Open Questions
*None.* Q1–Q3 confirmed as recommended by approval on 2026-11-01. Rationale kept below.
- **Q1 — Time type?** *Recommended: `std::chrono::steady_clock::time_point` and `duration`.* Familiar
  to every C++ developer, and works with `<chrono>` arithmetic and literals (`16ms`). The alternative, a
  custom `TimePoint` struct, is more type-safe across clocks but adds API for little gain now.
- **Q2 — Where does `SystemClock` live?** *Recommended: `modules/platform`.* It needs only `<chrono>`.
  A separate backend would add a target every app must link just to get the time. Real OS adapters
  (with OS headers) still go to `backends/`.
- **Q3 — `FakeClock::advance` with a negative step?** *Recommended: `InvalidArgument`.* Time can't go
  backwards, and the error makes test mistakes visible. Clamping to zero would hide them.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0008): …`):
   - **Headers:** `Clock`, `SystemClock`, `FakeClock`.
   - **Stubs:** `SystemClock::now()` returns `TimePoint{}`. `FakeClock::now()` returns `TimePoint{}`, and
     `advance` returns success without changing time.
   - **Tests:** contract header, its instantiation for both clocks, unit tests, and the generalized
     lint regex. Lint before freezing.
   - **Expected Red:**
     - Fail: AC-2 (the `start` case), AC-3, AC-4, AC-5, AC-6.
     - Pass as guards: AC-1 for both clocks (a constant is trivially monotonic) and the default-start
       part of AC-2.
2. **Green:** implement both clocks.
3. **Refactor:** `tools/check.sh`, `asan`, docs (OVERVIEW §3 note, EPIC-002, Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/clock.hpp` | create | Port |
| `modules/platform/include/coil/platform/system_clock.hpp` | create | `SystemClock` |
| `modules/platform/src/clock.cpp` | create | Out-of-line destructor |
| `modules/platform/src/system_clock.cpp` | create | `SystemClock::now` |
| `modules/platform/testing/include/coil/platform/testing/fake_clock.hpp` | create | `FakeClock` |
| `modules/platform/testing/src/fake_clock.cpp` | create | `FakeClock` |
| `modules/platform/CMakeLists.txt` | modify | New sources |
| `modules/platform/tests/contract/coil/platform/clock_contract.hpp` | create | `ClockContract` (CL1) |
| `modules/platform/tests/clock_test.cpp` | create | Instantiations + AC-2…AC-6 |
| `modules/platform/tests/CMakeLists.txt` | modify | Add `clock_test.cpp` |
| `modules/platform/tests/.clang-tidy`, `backends/platform_sdl3/tests/.clang-tidy` | modify | Generalized regex |
| `docs/architecture/OVERVIEW.md`, `docs/epics/EPIC-002-hello-window.md` | modify | Docs |

### 10.3 Risks / alternatives considered
- **AC-5 loop bound:** `steady_clock` has sub-microsecond resolution on all target platforms, so time
  moves within a few calls. The bound only prevents a broken clock from hanging the test.
- **AC-6 on a coarse clock** is still valid, because the bounds are inclusive.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.1
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `Fake/ClockContract.CL1_NowNeverDecreases`, `System/ClockContract.CL1_NowNeverDecreases`
- [x] AC-2 → `FakeClock.AC2_StartsAtStartAndStandsStill`
- [x] AC-3 → `FakeClock.AC3_AdvanceMovesByExactlyTheStep`
- [x] AC-4 → `FakeClock.AC4_NegativeAdvanceIsRejected`
- [x] AC-5 → `SystemClock.AC5_TimeMoves`
- [x] AC-6 → `SystemClock.AC6_TracksSteadyClock`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: AC-2…AC-6 fail on assertions (stub clocks fixed at zero; `advance` ignored); CL1 passes for both clocks as planned guards. Test code clang-tidy clean before freezing (only finding in stub code). | Emilio Pomares |
| 2026-11-01 | **Green + Refactor**: `SystemClock` returns `steady_clock::now()`; `FakeClock` stores its time, `advance` rejects negative steps with `InvalidArgument`. `tools/check.sh` exit 0 (66/66 tests); `ctest --preset asan` 66/66; `git diff e510518 -- ':(glob)**/tests/**'` empty. OVERVIEW §3 notes std-only port implementations may live in `modules/`. | Emilio Pomares (acknowledgement pending) |
