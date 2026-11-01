# SPEC-0012: `hello_window` is resizable and logs its size changes

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `examples/hello_window` only |
| Epic | [EPIC-005](../epics/EPIC-005-window-resizing.md) (spec 4, done before spec 3 at the user's request) |
| Related ADRs | ADR-0004 (the example is a composition root) |
| Depends on | SPEC-0007 (`hello_window`), SPEC-0010 (`Resized`), SPEC-0011 (resizable, limits) |

> **Scope Gate checklist**
> - [x] One concern: the example's window can be resized by the user, and each new size is printed
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 area (`examples/hello_window`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
EPIC-005 specs 1 and 2 added `setSize`, `Resized`, the resizable flag and size limits. None of it is
visible yet. This spec makes `hello_window` show the result:
- its window is **resizable**, with a **minimum size**;
- each time its size changes, it prints a line such as `hello_window: resized to 1024x768`.

The manual check is to drag the window's edges and watch those lines appear.

## 2. Scope
- `hello_window.hpp/.cpp`:
  - the window is created with `resizable = true` and a minimum size `kMinSize` (320×240);
  - `runHelloWindow` takes an output stream and writes one line per `Resized` event for its own
    window.
- `main.cpp` passes `std::cout`.
- Tests:
  - new tests AC-1 to AC-5;
  - the five SPEC-0007 tests updated **mechanically** for the new parameter (they pass a local
    `std::ostringstream`). Their assertions are unchanged.

## 3. Out of Scope
- Drawing anything (the window content is still whatever the OS shows).
- A maximum size, aspect ratio, maximize and fullscreen.
- Smooth live-resize feedback on macOS (see §10.3). The program reacts to the events SDL delivers.
- Pixel size and display scale (EPIC-005 spec 3).

## 4. Behavior Specification

### 4.1 Public API (example-internal)
```cpp
// examples/hello_window/hello_window.hpp (changes)
inline constexpr coil::platform::WindowSize kMinSize{.width = 320, .height = 240};

/// Opens one resizable window (kTitle, kSize, minimum kMinSize) and blocks in
/// waitEvents(kIdleTimeout) until the user asks to close it. Writes one line to `log` for each
/// size change of that window: "hello_window: resized to <w>x<h>". Returns 0 after the window was
/// closed, or 1 if it could not be created (error on stderr).
[[nodiscard]] int runHelloWindow(coil::platform::WindowSystem& windows, std::ostream& log);
```
```cpp
// examples/hello_window/main.cpp (change)
return hello_window::runHelloWindow(**windows, std::cout);
```

### 4.2 Behavioral rules
1. The window MUST be created with `resizable = true` and limits `{.min = kMinSize}` (no maximum),
   plus everything SPEC-0007 rule 1 already requires (title `kTitle`, size `kSize`).
2. For each `Resized` event for its own window, in order, `runHelloWindow` MUST write exactly
   `hello_window: resized to <width>x<height>\n` to `log`, before handling any close request in the
   same batch of events.
3. `Resized` events for other windows MUST NOT be logged.
4. Nothing else is written to `log`. Errors still go to stderr (SPEC-0007 rule 4).
5. All SPEC-0007 rules (one `waitEvents(kIdleTimeout)` per iteration, close handling, exit codes)
   still hold.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| The user drags below the minimum | The OS keeps 320×240; the logged size is the clamped one |
| Several resizes between two waits | One line for the latest size (SPEC-0010 coalescing) |
| `Resized` and `CloseRequested` in the same batch | The resize line is written, then the program exits 0 |

### 4.4 Threading and lifetime
Unchanged (SPEC-0007). `log` is borrowed for the duration of the call.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
Tests use SPEC-0007's `ObservedWindowSystem`. Its callback runs before each event call.

- **AC-1** — Given a `FakeWindowSystem`, by the first event call the program's window is resizable,
  and its limits are exactly `{.min = 320×240}`.
- **AC-2** — Given a callback that resizes the program's window as the user would (`userResize`) to
  1024×768 before call 1, to 640×480 before call 2, and requests close before call 3,
  `runHelloWindow` returns 0, and `log` is exactly
  `"hello_window: resized to 1024x768\nhello_window: resized to 640x480\n"`.
- **AC-3** — Given a second window created by the test and resized before call 1, and a close request
  for the program's window before call 2, `log` is empty.
- **AC-4** — Given a user resize to 100×100 (below the minimum) before call 1 and a close request
  before call 2, `log` is exactly `"hello_window: resized to 320x240\n"`.
- **AC-5** — Integration: given an SDL window system on the `dummy` driver and a callback that, before
  call 1, resizes the program's SDL window directly to 1024×768 (`SDL_SetWindowSize`, as a user drag
  would) and, before call 2, pushes a close request, `runHelloWindow` returns 0 and `log` is exactly
  `"hello_window: resized to 1024x768\n"`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `HelloWindow.R1_WindowIsResizableWithMinimumSize` | unit | fake |
| AC-2 | `HelloWindow.R2_LogsEachResizeOfItsWindow` | unit | fake `userResize` |
| AC-3 | `HelloWindow.R3_IgnoresResizesOfOtherWindows` | unit | fake |
| AC-4 | `HelloWindow.R4_LogsTheClampedSize` | unit | fake clamps to the limits |
| AC-5 | `HelloWindow.R5_LogsResizesOnSdl` | integration | SDL `dummy` |

The `R<n>` prefix (R for "resizable") keeps these tests distinct from SPEC-0007's `AC<n>_` tests in the
same `HelloWindow` suite.

## 7. Non-Functional Requirements
- No new dependency.

## 8. Architectural Impact
None. The example remains the only composition root, and only `main.cpp` names SDL or `std::cout`.

## 9. Open Questions
All resolved as recommended (2026-11-01, Emilio Pomares).
- **Q1 — How does the program write its log?** *Recommended: an injected `std::ostream&`*, with `main`
  passing `std::cout`. Tests can then compare the exact text, with no globals and no captured stdout.
  The alternative is a callback per resize, which leaves the printing in untested `main`.
- **Q2 — Which limits?** *Recommended: minimum 320×240, no maximum.* This makes spec 2 visible during
  the manual check: you can't drag the window smaller than that.
- **Q3 — Log line format?** *Recommended: `hello_window: resized to 1024x768`*, the same prefix as
  the error messages.
- **Q4 — The five SPEC-0007 tests?** *Recommended:* update them mechanically to pass a local
  `std::ostringstream`, and leave their assertions untouched. They go through this spec's Red with the
  new tests.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0012): …`):
   - **Header:** `kMinSize`, plus the new `runHelloWindow` signature.
   - **Stub:** `runHelloWindow` keeps SPEC-0007's behavior: a non-resizable window without limits,
     and it never writes to `log`.
   - **Tests:** `main.cpp` passes `std::cout`. The SPEC-0007 tests get a stream argument. Add
     R1–R5, with an SDL resize helper next to the existing `closeOnSdl`. Lint the test code before
     freezing.
   - **Expected Red:**
     - R1 fails (not resizable, no limits). R2, R4 and R5 fail because `log` is empty.
     - R3 passes as a guard (an empty `log` is correct).
     - The SPEC-0007 tests keep passing.
2. **Green:** create the window with `resizable` and limits; in the loop, write a line for each own
   `Resized`, then check for the close request.
3. **Refactor:**
   - `tools/check.sh`, `asan`, macOS `leaks` on R5.
   - **Manual check:**
     1. Run `hello_window`.
     2. Drag the window's edges: lines appear, and the window can't get smaller than 320×240.
     3. Close the window: the exit code is 0.

     Record the result in the Change Log.
   - Docs: EPIC-005.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `examples/hello_window/hello_window.hpp` | modify | `kMinSize`, signature |
| `examples/hello_window/hello_window.cpp` | modify | Resizable window, logging |
| `examples/hello_window/main.cpp` | modify | Pass `std::cout` |
| `examples/hello_window/tests/hello_window_test.cpp` | modify | Stream argument; R1–R5 |
| `docs/specs/0007-hello-window-example.md` | modify | Note in Change Log: signature changed by SPEC-0012 |
| `docs/epics/EPIC-005-window-resizing.md` | modify | Order and status |

### 10.3 Risks / alternatives considered
- **macOS live resize:** while the user drags an edge, Cocoa runs its own event loop inside SDL's
  event pump. The program may then only see the size when the drag pauses or ends, and coalescing
  reports only the latest one. So during the manual check, **at least the final size of each drag** is
  expected to print. Intermediate sizes may or may not appear. Continuous updates during a drag
  belong with the main-loop epic (EPIC-003: SDL main callbacks and event watchers), not here.
- **`std::cout` in `main`:** streams don't throw unless exceptions are enabled on them, so `main`
  stays non-throwing (`bugprone-exception-escape` will confirm).

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guard listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes; macOS `leaks` reports 0 leaks for R5
- [ ] Every AC ticked below with its proving test
- [ ] **Manual check** (resize by dragging, minimum enforced, lines printed) done on macOS and recorded

### AC verification
- [ ] AC-1 → `HelloWindow.R1_WindowIsResizableWithMinimumSize`
- [ ] AC-2 → `HelloWindow.R2_LogsEachResizeOfItsWindow`
- [ ] AC-3 → `HelloWindow.R3_IgnoresResizesOfOtherWindows`
- [ ] AC-4 → `HelloWindow.R4_LogsTheClampedSize`
- [ ] AC-5 → `HelloWindow.R5_LogsResizesOnSdl`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Q1–Q4 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-01 | **Red**: R1 (not resizable, no limits), R2, R4 and R5 (empty log) fail on assertions; R3 passes as the planned guard; the five SPEC-0007 tests pass with the added stream argument (assertions unchanged); the other 107 tests pass. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
