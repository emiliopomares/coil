# SPEC-0007: `hello_window` example: open a titled window until the user closes it

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `examples/hello_window` (new); `tools/check.sh` (format and tidy cover `examples/`) |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0004 (the example is a composition root), ADR-0010 (UI thread) |
| Depends on | SPEC-0005, SPEC-0006, SPEC-0009 |

> **Scope Gate checklist**
> - [x] One concern: a runnable program that shows a window titled "Coil" and exits when it is closed
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (`examples/hello_window`, `tools/check.sh`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
This is the last spec of EPIC-002. Every piece exists (window port, SDL3 adapter, close events,
`waitEvents`), but there is no program a person can run. This spec adds `examples/hello_window`,
Coil's first executable and first **composition root**: it picks the SDL3 backend and wires it to the
app logic. The logic lives in a testable function, so tests drive it through the fake and through SDL's
`dummy` driver without a display and without any test-only mode in the program (CLAUDE.md §3).

## 2. Scope
- `examples/hello_window/`:
  - `hello_window.hpp/.cpp`: `runHelloWindow(WindowSystem&)`, the whole app logic. It blocks in
    `waitEvents(kIdleTimeout)` between events.
  - `main.cpp`: creates the SDL window system with default options, calls `runHelloWindow`, and
    returns its exit code.
  - Tests: unit tests against `FakeWindowSystem` (through a forwarding test double), a failing test
    double, and an integration test on SDL's `dummy` driver.
- `CMakeLists.txt`: `COIL_BUILD_EXAMPLES` option (default `ON`), `add_subdirectory(examples/hello_window)`.
- `tools/check.sh`: the format and tidy gates also cover `examples/`.
- A **manual visual check** on macOS, recorded in the Change Log. It's not an AC (§11).

## 3. Out of Scope
- Drawing anything inside the window (rendering epic). The window's content is whatever the OS shows
  for an empty window.
- A general application or main-loop abstraction (`coil::app`), frame pacing, vsync (EPIC-003).
- Command-line options (driver selection, size, title).
- Layering rules for `examples/`. The layering checker scans `modules/` and `backends/` only
  (SPEC-0004), and examples, as composition roots, may use any module or backend.
- App bundles, icons, installers, and running on Windows or Linux (CI matrix spec).

## 4. Behavior Specification

### 4.1 Public API (example-internal)
```cpp
// examples/hello_window/hello_window.hpp
#pragma once

#include <coil/platform/clock.hpp>
#include <coil/platform/window_system.hpp>

#include <chrono>
#include <string_view>

namespace hello_window {

inline constexpr std::string_view kTitle = "Coil";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};
/// How long one wait may block when nothing happens. Nothing runs on timeout; the value only bounds
/// how long the loop sleeps between checks.
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};

/// Opens one window (kTitle, kSize) and blocks in waitEvents(kIdleTimeout) until the user asks to
/// close it. Returns 0 after the window was closed, or 1 if it could not be created (error on stderr).
[[nodiscard]] int runHelloWindow(coil::platform::WindowSystem& windows);

} // namespace hello_window
```
```cpp
// examples/hello_window/main.cpp (sketch)
int main() {
    auto windows = coil::platform_sdl3::createSdlWindowSystem();
    if (!windows) { /* print error to stderr */ return 1; }
    return hello_window::runHelloWindow(**windows);
}
```

### 4.2 Behavioral rules
1. `runHelloWindow` MUST create exactly one window with title `kTitle` and size `kSize` before it
   asks for events the first time.
2. Each loop iteration MUST call `waitEvents(kIdleTimeout)` exactly once, and MUST NOT call
   `pollEvents`. If the result contains `CloseRequested` for its window, it MUST destroy that window
   and return 0. Otherwise it MUST loop.
3. Events for other windows MUST be ignored (not acted on, and those windows are not destroyed).
4. If `createWindow` fails, `runHelloWindow` MUST print the error to stderr, return 1, and never ask
   for events.
5. `main` MUST return 1 (with the error on stderr) if the SDL window system cannot be created, and
   otherwise return `runHelloWindow`'s result.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| No display or SDL video unavailable | `main` prints the error and returns 1 |
| `createWindow` fails | stderr message, return 1, no event calls |
| `destroyWindow` fails after a close request | ignored; still returns 0 (the program is ending) |
| `waitEvents` times out (empty result) | loop again |

### 4.4 Threading and lifetime
Single thread, the main thread (SDL requirement, ADR-0010). `runHelloWindow` borrows the window
system; `main` owns it and destroys it after `runHelloWindow` returns, which shuts down SDL video
(SPEC-0005).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets yet).

## 5. Acceptance Criteria
Tests drive the program through `ObservedWindowSystem`, a test double that forwards every port call to
a real implementation (the fake or SDL), counts event calls, records the timeouts it is given, and runs
a test callback before forwarding each event call. That callback is where a test plays the user.

- **AC-1** — Given a `FakeWindowSystem`, when `runHelloWindow` asks for events the first time, then
  exactly one window exists, titled `"Coil"` with size 800×600.
- **AC-2** — Given a `FakeWindowSystem` and a callback that requests close of the program's window
  before the 3rd event call, `runHelloWindow` returns 0, there were exactly 3 event calls, all of them
  `waitEvents(kIdleTimeout)` (no `pollEvents`), the fake's clock advanced by exactly 2 × `kIdleTimeout`
  (two waits timed out), and no window remains.
- **AC-3** — Given a close request for a window the program did not create (a second window created
  by the test on the fake before the 1st event call), the program keeps running and that window still
  exists; it stops only when its own window gets a close request (before the 2nd event call).
- **AC-4** — Given a window system whose `createWindow` fails (a test double returning `Unavailable`),
  `runHelloWindow` returns 1 and makes no event calls.
- **AC-5** — Integration: given an SDL window system on the `dummy` driver and a callback that, before
  the 1st event call, pushes an SDL close-request event for the program's window (found through
  `sdlWindowId`), `runHelloWindow` returns 0, exactly one window was created, and the SDL system has no
  windows left.
- **AC-6** — `cmake --build --preset dev` builds the `hello_window` executable by default, with
  warnings as errors, and `tools/check.sh` (format and tidy) covers `examples/`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `HelloWindow.AC1_OpensOneTitledWindow` | unit | fake; assertions in the callback, then close |
| AC-2 | `HelloWindow.AC2_WaitsUntilItsWindowIsClosed` | unit | fake + its `FakeClock` |
| AC-3 | `HelloWindow.AC3_IgnoresCloseRequestsForOtherWindows` | unit | fake |
| AC-4 | `HelloWindow.AC4_FailsWhenWindowCannotBeCreated` | unit | `FailingWindowSystem` test double (implements the port) |
| AC-5 | `HelloWindow.AC5_RunsOnSdlUntilClosed` | integration | SDL `dummy`, `SDL_PushEvent` |
| AC-6 | `gate.examples.AC6_HelloWindowBuilds` + `tools/check.sh` | gate | CTest checks the executable exists after the build |

**Infinite-loop guards:** `ObservedWindowSystem` records a test failure and queues a close request for
every live window after 100 event calls, so a broken loop ends with a clear failure instead of hanging.
All tests in this executable also get CTest `TIMEOUT 30` (SPEC-0009) as a backstop.

## 7. Non-Functional Requirements
- An idle `hello_window` blocks in the OS (`SDL_WaitEventTimeout`) and wakes at most about 4 times a
  second, so it uses a negligible amount of CPU (no busy loop). Checked by eye during the manual check,
  not asserted.
- No new dependency.

## 8. Architectural Impact
- First executable and first composition root (ADR-0004): only `main.cpp` names the SDL backend;
  `runHelloWindow` depends only on the port.
- `tools/check.sh` gate coverage grows to `examples/` (SPEC-0001 rule 6 is extended by this spec).

## 9. Open Questions
All resolved (2026-11-01, Emilio Pomares).
- **Q1 — How does the loop avoid burning a CPU core?** Decided: **option B**, block in
  `waitEvents(timeout)` (SPEC-0009).
- **Q2 — Build examples by default?** Decided: **yes** (`COIL_BUILD_EXAMPLES=ON`), so they never rot.
  Library users can turn them off.
- **Q3 — Lint and format coverage for `examples/`?** Decided: **yes for format and tidy, no for
  layering** (composition roots may use anything).
- **Q4 — Idle timeout value?** Decided: **a finite 250 ms**. Nothing happens on timeout today, so any
  bounded value works. The port has no "wait forever" (SPEC-0009); whether it should get one is left to
  EPIC-003, together with cross-thread wakeups. 250 ms keeps wakeups rare while keeping the loop
  responsive to future periodic work.
- **Q5 — How do tests inject the "user" into the loop?** Decided: **a forwarding test double**
  (`ObservedWindowSystem`) rather than the fake's own wait hook, because the SDL integration test needs
  the same injection point, and the double can also count calls for the loop guard and assert "no
  `pollEvents`". It's a fake of the **port**, not of the unit under test (CLAUDE.md §3).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0007): …`):
   - **Build:** `COIL_BUILD_EXAMPLES`; `examples/hello_window` with the library target
     `coil_example_hello_window`, the `hello_window` executable and tests (`TIMEOUT 30`);
     `check.sh` covers `examples/`.
   - **Stub:** `runHelloWindow` returns 0 immediately without touching the window system; `main.cpp`
     is already the real composition root (it just forwards the stub's result).
   - **Tests:** AC-1 to AC-6 with `ObservedWindowSystem` and `FailingWindowSystem` in the test
     directory. Lint and simulate the gate regex before freezing.
   - **Expected Red:** AC-1, AC-2, AC-3 and AC-5 fail on assertions (no window created, no event
     calls). AC-4 fails (returns 0, expected 1). AC-6 passes as a guard (the build infrastructure is
     part of Red).
2. **Green:** implement `runHelloWindow` (§4.2).
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on AC-5. **Manual check:** run
   `build/dev/examples/hello_window/hello_window`, confirm the "Coil" window appears and the process is
   idle, close it, confirm exit code 0, and record this in the Change Log. Update EPIC-002 (all specs
   done → epic Done after your acknowledgement), CLAUDE.md §7 (how to run the example).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | modify | `COIL_BUILD_EXAMPLES`, `add_subdirectory(examples/hello_window)` |
| `examples/hello_window/CMakeLists.txt` | create | library, executable, tests |
| `examples/hello_window/hello_window.hpp` | create | §4.1 |
| `examples/hello_window/hello_window.cpp` | create | logic |
| `examples/hello_window/main.cpp` | create | composition root |
| `examples/hello_window/tests/CMakeLists.txt` | create | test executable, `TIMEOUT 30`, AC-6 gate |
| `examples/hello_window/tests/hello_window_test.cpp` | create | AC-1 to AC-5, test doubles |
| `examples/hello_window/tests/.clang-tidy` | create | test override (copy) |
| `tools/check.sh` | modify | `examples` in the source roots |
| `CLAUDE.md`, `docs/epics/EPIC-002-hello-window.md` | modify | docs |

### 10.3 Risks / alternatives considered
- **macOS and terminal-launched GUI apps:** SDL makes a non-bundled process a regular app, so the
  window should appear and be focusable. The manual check confirms this. If it doesn't, that's a
  finding for a follow-up spec, not a test change.
- **Empty window content:** with no renderer, macOS may show a black or uninitialized window. That's
  expected and out of scope.
- **AC-5 really waits:** if the pushed close event were missed, the SDL test would wait 250 ms per
  iteration until the loop guard fires after 100 calls (about 25 s, under the 30 s timeout). In the
  passing case the event is already pending, so the first `waitEvents` returns at once.
- **Alternative considered:** testing `main` itself by launching the executable. It would need a way
  to make it exit without user input, meaning a test-only flag or environment variable, which CLAUDE.md
  forbids. The logic is fully tested through `runHelloWindow`; `main` is 4 lines plus the manual check.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guard listed in §10.1
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for AC-5
- [x] Every AC ticked below with its proving test
- [x] **Manual visual check** done on macOS and recorded in the Change Log

### AC verification
- [x] AC-1 → `HelloWindow.AC1_OpensOneTitledWindow`
- [x] AC-2 → `HelloWindow.AC2_WaitsUntilItsWindowIsClosed`
- [x] AC-3 → `HelloWindow.AC3_IgnoresCloseRequestsForOtherWindows`
- [x] AC-4 → `HelloWindow.AC4_FailsWhenWindowCannotBeCreated`
- [x] AC-5 → `HelloWindow.AC5_RunsOnSdlUntilClosed`
- [x] AC-6 → `gate.examples.AC6_HelloWindowBuilds`, `tools/check.sh`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Q1 decided: **option B** (`waitEvents(timeout)`) instead of the injected `idle` sleep. Spec on hold; §4, §5 and §10 will be revised after the `Clock` and `waitEvents` specs. | Emilio Pomares |
| 2026-11-01 | Revised for `waitEvents`: `idle` parameter removed, `kIdleTimeout` (250 ms) added; tests inject the user through a forwarding `ObservedWindowSystem` double; AC-2 also checks the timeouts and the fake clock; Q4 and Q5 added. | Emilio Pomares |
| 2026-11-01 | Q2–Q5 decided (Q4: finite 250 ms timeout; "wait forever" deferred to EPIC-003) | Emilio Pomares |
| 2026-11-01 | Spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: AC-1…AC-5 fail on assertions (stub returns 0 without touching the window system); AC-6 passes as the planned guard; the other 78 tests pass. Loop guard checked by temporarily running a never-ending loop: AC-2 and AC-5 fail with "loop guard" (AC-5 after 24.5 s, under the 30 s timeout); the AC-6 gate command was checked to fail for a missing file. `main` reports errors with a non-throwing `std::fwrite` helper (`bugprone-exception-escape`). Test code clang-tidy clean before freezing. | Emilio Pomares |
| 2026-11-01 | **Green + Refactor**: `runHelloWindow` creates the window, loops on `waitEvents(kIdleTimeout)`, destroys its window and returns 0 on its own `CloseRequested` (a failed destroy is printed to stderr, exit code unchanged). `tools/check.sh` exit 0 (84/84 tests); `ctest --preset asan` 84/84; macOS `leaks`: 0 leaks for AC-5. `git diff aae66e6 -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
| 2026-11-01 | **Manual visual check** (macOS 27.2, arm64): `build/dev/examples/hello_window/hello_window` showed the "Coil" window; while idle the process was sleeping at 0.1% CPU; closing the window exited with code 0 and no stderr output. Acknowledged; Status → Done. | Emilio Pomares |
