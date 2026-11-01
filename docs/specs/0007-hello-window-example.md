# SPEC-0007: `hello_window` example: open a titled window until the user closes it

| Field | Value |
|-------|-------|
| Status | Draft |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `examples/hello_window` (new); `tools/check.sh` (format and tidy cover `examples/`) |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0004 (the example is a composition root), ADR-0010 (UI thread) |
| Depends on | SPEC-0005, SPEC-0006 |

> **Scope Gate checklist**
> - [x] One concern: a runnable program that shows a window titled "Coil" and exits when it is closed
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (`examples/hello_window`, `tools/check.sh`); no new dependency
> - [x] Out-of-scope section is filled in
> - [ ] Open Questions section is empty (three confirmations pending, see §9)

---

## 1. Problem / Motivation
This is the last spec of EPIC-002. Every piece exists (window port, SDL3 adapter, close events), but
there is no program a person can run. This spec adds `examples/hello_window`, Coil's first executable
and first **composition root**: it picks the SDL3 backend and wires it to the app logic. The logic
lives in a testable function, so tests drive it through the fake and through SDL's `dummy` driver
without a display and without any test-only mode in the program (CLAUDE.md §3).

## 2. Scope
- `examples/hello_window/`:
  - `hello_window.hpp/.cpp`: `runHelloWindow(WindowSystem&, idle)`, the whole app logic.
  - `main.cpp`: creates the SDL window system with default options, calls `runHelloWindow` with an
    idle step that sleeps about 16 ms, and returns its exit code.
  - Tests: unit tests against `FakeWindowSystem` and a failing test double, plus an integration test
    on SDL's `dummy` driver.
- `CMakeLists.txt`: `COIL_BUILD_EXAMPLES` option (default `ON`), `add_subdirectory(examples/hello_window)`.
- `tools/check.sh`: the format and tidy gates also cover `examples/`.
- A **manual visual check** on macOS, recorded in the Change Log. It's not an AC (§11).

## 3. Out of Scope
- Drawing anything inside the window (rendering epic). The window's content is whatever the OS shows
  for an empty window.
- A general application or main-loop abstraction (`coil::app`), frame pacing, vsync, `waitEvents`.
  The idle step is the example's own simple pacing (§9 Q1).
- Command-line options (driver selection, size, title).
- Layering rules for `examples/`. The layering checker scans `modules/` and `backends/` only
  (SPEC-0004), and examples, as composition roots, may use any module or backend.
- App bundles, icons, installers, and running on Windows or Linux (CI matrix spec).

## 4. Behavior Specification

### 4.1 Public API (example-internal)
```cpp
// examples/hello_window/hello_window.hpp
#pragma once

#include <coil/platform/window_system.hpp>

#include <functional>
#include <string_view>

namespace hello_window {

inline constexpr std::string_view kTitle = "Coil";
inline constexpr coil::platform::WindowSize kSize{.width = 800, .height = 600};

/// Opens one window (kTitle, kSize) and processes window events until the user asks to close it.
/// `idle` runs once after every poll that did not end the program (main: sleep ~16 ms).
/// Returns 0 after the window was closed, or 1 if it could not be created (error on stderr).
[[nodiscard]] int runHelloWindow(coil::platform::WindowSystem& windows,
                                 const std::function<void()>& idle);

} // namespace hello_window
```
```cpp
// examples/hello_window/main.cpp (sketch)
int main() {
    auto windows = coil::platform_sdl3::createSdlWindowSystem();
    if (!windows) { /* print error to stderr */ return 1; }
    return hello_window::runHelloWindow(**windows, [] { std::this_thread::sleep_for(16ms); });
}
```

### 4.2 Behavioral rules
1. `runHelloWindow` MUST create exactly one window with title `kTitle` and size `kSize`, before
   calling `idle` for the first time.
2. Each loop iteration MUST call `pollEvents()` once. If the result contains `CloseRequested` for its
   window, it MUST destroy that window and return 0 **without** calling `idle` again. Otherwise it MUST
   call `idle` exactly once and loop.
3. Events for other windows MUST be ignored.
4. If `createWindow` fails, `runHelloWindow` MUST print the error to stderr, return 1, and never call
   `idle` or `pollEvents`.
5. `main` MUST return 1 (with the error on stderr) if the SDL window system cannot be created, and
   otherwise return `runHelloWindow`'s result.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| No display or SDL video unavailable | `main` prints the error and returns 1 |
| `createWindow` fails | stderr message, return 1, `idle` never called |
| `destroyWindow` fails after a close request | ignored; still returns 0 (the program is ending) |

### 4.4 Threading and lifetime
Single thread, the main thread (SDL requirement, ADR-0010). `runHelloWindow` borrows the window
system; `main` owns it and destroys it after `runHelloWindow` returns, which shuts down SDL video
(SPEC-0005).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets yet).

## 5. Acceptance Criteria
- **AC-1** — Given a `FakeWindowSystem`, when `runHelloWindow` runs, then by the first `idle` call
  exactly one window exists, titled `"Coil"` with size 800×600.
- **AC-2** — Given a `FakeWindowSystem` and an `idle` step that requests close of that window on its
  3rd call, `runHelloWindow` returns 0, `idle` was called exactly 3 times, and no window remains.
- **AC-3** — Given a close request for a window the program did not create (a second window created
  by the test on the fake), the program keeps running, and it stops only when its own window gets a
  close request.
- **AC-4** — Given a window system whose `createWindow` fails (a test double returning `Unavailable`),
  `runHelloWindow` returns 1 and `idle` is never called.
- **AC-5** — Integration: given an SDL window system on the `dummy` driver and an `idle` step that pushes
  an SDL close-request event for the program's window (via `sdlWindowId`), `runHelloWindow` returns 0
  and the SDL system has no windows left.
- **AC-6** — `cmake --build --preset dev` builds the `hello_window` executable by default, with
  warnings as errors, and `tools/check.sh` (format and tidy) covers `examples/`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `HelloWindow.AC1_OpensOneTitledWindow` | unit | fake; assertions inside `idle`, then close |
| AC-2 | `HelloWindow.AC2_ExitsWhenItsWindowIsClosed` | unit | fake |
| AC-3 | `HelloWindow.AC3_IgnoresCloseRequestsForOtherWindows` | unit | fake |
| AC-4 | `HelloWindow.AC4_FailsWhenWindowCannotBeCreated` | unit | `FailingWindowSystem` test double (implements the port) |
| AC-5 | `HelloWindow.AC5_RunsOnSdlUntilClosed` | integration | SDL `dummy`, `SDL_PushEvent` |
| AC-6 | `gate.examples.AC6_HelloWindowBuilds` + `tools/check.sh` | gate | CTest checks the executable exists after the build |

**Infinite-loop guard:** every test's `idle` step fails the test and requests close after 100 calls,
so a broken loop ends with a clear failure instead of hanging.

## 7. Non-Functional Requirements
- With the 16 ms idle step, an idle `hello_window` uses a negligible amount of CPU (no busy loop).
- No new dependency.

## 8. Architectural Impact
- First executable and first composition root (ADR-0004): only `main.cpp` names the SDL backend;
  `runHelloWindow` depends only on the port.
- `tools/check.sh` gate coverage grows to `examples/` (SPEC-0001 rule 6 is extended by this spec).

## 9. Open Questions
Recommended answers are written into the spec. **Approving the spec confirms them.**
- **Q1 — How does the loop avoid burning a CPU core?** *Recommended: an injected `idle` step* (main
  sleeps ~16 ms, about 60 iterations a second). It's simple, and tests control it. The alternative, a
  blocking `waitEvents(timeout)` on the port, changes the port and belongs with real frame pacing in
  the app main-loop spec.
- **Q2 — Build examples by default?** *Recommended: yes* (`COIL_BUILD_EXAMPLES=ON`), so they never rot.
  Library users can turn them off.
- **Q3 — Lint and format coverage for `examples/`?** *Recommended: yes for format and tidy, no for
  layering* (composition roots may use anything).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0007): …`):
   - **Build:** `COIL_BUILD_EXAMPLES`; `examples/hello_window` with the library target
     `coil_example_hello_window`, the `hello_window` executable and tests; `check.sh` covers
     `examples/`.
   - **Stub:** `runHelloWindow` returns 0 immediately without touching the window system.
   - **Tests:** AC-1 to AC-6, with the infinite-loop guard. Lint and simulate before freezing.
   - **Expected Red:** AC-1, AC-2, AC-3 and AC-5 fail on assertions (no window, `idle` never called).
     AC-4 fails (returns 0, expected 1). AC-6 passes as a guard (the build infrastructure is part of
     Red).
2. **Green:** implement `runHelloWindow` (§4.2) and the real `main.cpp`.
3. **Refactor:** `tools/check.sh`, `asan`, macOS `leaks` on AC-5. **Manual check:** run
   `build/dev/examples/hello_window/hello_window`, confirm the "Coil" window appears, close it, confirm
   exit code 0, and record this in the Change Log. Update EPIC-002 (all specs done → epic Done after
   your acknowledgement), CLAUDE.md §7 (how to run the example).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | modify | `COIL_BUILD_EXAMPLES`, `add_subdirectory(examples/hello_window)` |
| `examples/hello_window/CMakeLists.txt` | create | library, executable, tests |
| `examples/hello_window/hello_window.hpp` | create | §4.1 |
| `examples/hello_window/hello_window.cpp` | create | logic |
| `examples/hello_window/main.cpp` | create | composition root |
| `examples/hello_window/tests/CMakeLists.txt` | create | test executable, AC-6 gate |
| `examples/hello_window/tests/hello_window_test.cpp` | create | AC-1 to AC-5 |
| `examples/hello_window/tests/.clang-tidy` | create | test override (copy) |
| `tools/check.sh` | modify | `examples` in the source roots |
| `CLAUDE.md`, `docs/epics/EPIC-002-hello-window.md` | modify | docs |

### 10.3 Risks / alternatives considered
- **macOS and terminal-launched GUI apps:** SDL makes a non-bundled process a regular app, so the
  window should appear and be focusable. The manual check confirms this. If it doesn't, that's a
  finding for a follow-up spec, not a test change.
- **Empty window content:** with no renderer, macOS may show a black or uninitialized window. That's
  expected and out of scope.
- **Alternative considered:** testing `main` itself by launching the executable. It would need a way
  to make it exit without user input, meaning a test-only flag or environment variable, which CLAUDE.md
  forbids. The logic is fully tested through `runHelloWindow`; `main` is 4 lines plus the manual check.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guard listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes; macOS `leaks` reports 0 leaks for AC-5
- [ ] Every AC ticked below with its proving test
- [ ] **Manual visual check** done on macOS and recorded in the Change Log

### AC verification
- [ ] AC-1 → `HelloWindow.AC1_OpensOneTitledWindow`
- [ ] AC-2 → `HelloWindow.AC2_ExitsWhenItsWindowIsClosed`
- [ ] AC-3 → `HelloWindow.AC3_IgnoresCloseRequestsForOtherWindows`
- [ ] AC-4 → `HelloWindow.AC4_FailsWhenWindowCannotBeCreated`
- [ ] AC-5 → `HelloWindow.AC5_RunsOnSdlUntilClosed`
- [ ] AC-6 → `gate.examples.AC6_HelloWindowBuilds`, `tools/check.sh`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
