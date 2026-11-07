# SPEC-0029: `intents` example: print the intents produced by real input

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `examples/intents` only (new) |
| Epic | [EPIC-007](../epics/EPIC-007-input-and-intents.md) (spec 4) |
| Related ADRs | ADR-0004 (the example is a composition root), ADR-0006 (DIPs vs pixels), ADR-0007 (device-agnostic input) |
| Depends on | SPEC-0026, SPEC-0027 (raw input), SPEC-0028 (`InputMapper`), SPEC-0022 (`presentImage`) |

> **Scope Gate checklist**
> - [x] One concern: an example window prints, one line each, the intents that the default mapping produces from the user's real mouse, trackpad and keyboard
> - [x] Every acceptance criterion is checkable by an automated test (plus a recorded manual check)
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 area (`examples/intents`); no library change, no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
EPIC-007's last Done-when item: an example that prints the intents produced by real input. So far
every input path (SDL translation, the mapping) has been tested only with synthetic events. Running
this example on a real Retina Mac with a real keyboard, mouse and trackpad is the first check that
positions arrive in DIPs (not pixels), that scroll directions and keys come out as specified, and
that text input works, before widgets start depending on all of it.

## 2. Scope
- New example `examples/intents`. As with `boxes`, the app logic is a library (`intents.hpp/.cpp`)
  so tests can drive it, and `main.cpp` is the only place that picks SDL3.
- One resizable window filled with a plain background, so it is visible on every platform.
- A line per intent on the log stream, in the format of §4.2, using `InputMapper` with the default
  bindings.
- A line with the window's logical size and scale at start and after each size or scale change, so
  printed positions can be compared against it.
- Tests I1–I7 on the fake window system and SDL's `dummy` driver, plus the build gate (I8).

## 3. Out of Scope
- Printing raw (unmapped) events, timestamps or device ids beyond `PointerId` (Q3).
- Changing the bindings at run time, command-line options.
- Coalescing pointer moves (Q4).
- Any library change: if the manual check finds a bug in SDL translation or the mapping, it is
  recorded and fixed by its own spec.
- Drawing anything beyond the background (no text in windows until EPIC-010).

## 4. Behavior Specification

### 4.1 Public API (example only)
```cpp
// examples/intents/intents.hpp
namespace intents {

inline constexpr std::string_view kTitle = "Coil intents";
inline constexpr coil::platform::WindowSize kSize{.width = 640, .height = 400};
inline constexpr coil::platform::Clock::Duration kIdleTimeout = std::chrono::milliseconds{250};
/// The window's fill (opaque, sRGB #1E1E24, as in boxes).
inline constexpr coil::foundation::Pixel kBackground{.r = 0x1E, .g = 0x1E, .b = 0x24, .a = 0xFF};

/// Formats one intent as a log line, without the trailing newline (§4.2).
[[nodiscard]] std::string describe(const coil::core::Intent& intent);

/// Opens one resizable window (kTitle, kSize) filled with kBackground and, until the user asks to
/// close it, writes one line per intent that the default InputMapper produces from `input` for that
/// window. Returns 0 after the window was closed, or 1 if it could not be created (error on stderr).
[[nodiscard]] int runIntents(coil::platform::WindowSystem& windows,
                             coil::platform::InputSource& input, std::ostream& log);

} // namespace intents
```

### 4.2 Line formats
Numbers are written with `std::format("{}")` (shortest round-trip form: `3`, `1.5`, `-0.25`).
Points are written `(x, y)`. Every line starts with `intents: `.

| Intent / event | Line |
|----------------|------|
| (window state) | `intents: window <w>x<h> at scale <s>` (logical size, as SPEC-0014 prints scale) |
| `Activate` | `intents: Activate` |
| `Cancel` | `intents: Cancel` |
| `Navigate` | `intents: Navigate <Up\|Down\|Left\|Right\|Next\|Previous>` |
| `Scroll` | `intents: Scroll <dx>, <dy> at (<x>, <y>)` |
| `PointerMove` | `intents: PointerMove <pointer> at (<x>, <y>)` |
| `PointerDown` | `intents: PointerDown <pointer> <Primary\|Secondary\|Middle\|Back\|Forward> at (<x>, <y>)` |
| `PointerUp` | `intents: PointerUp <pointer> <button> at (<x>, <y>)` |
| `TextInput` | `intents: TextInput "<text>"` (the UTF-8 bytes unchanged) |

`<pointer>` is `PointerId::value`. Examples: `intents: Scroll 0, -1.5 at (320, 200)`,
`intents: PointerDown 1 Primary at (10.5, 20)`, `intents: TextInput "é"`.

### 4.3 Behavioral rules
1. `runIntents` creates the window (`kTitle`, `kSize`, resizable), presents a `kBackground` image of
   the window's pixel size, and writes the window line, all before its first wait.
2. **Loop:** each iteration calls `waitEvents(kIdleTimeout)`, then `input.pollInput()`, maps each raw
   event in order with one `InputMapper` holding `Bindings::defaults()`, and writes `describe()` of
   each resulting intent whose window is the example's window, in order. Intents for other windows
   are not written.
3. **Window changes:** if the batch from `waitEvents` contains a `Resized` or `ScaleChanged` for its
   window, the example presents a new background image at the current pixel size and writes the
   window line once (after that batch's intent lines).
4. **Intents do nothing else.** No intent closes the window or changes anything: `Cancel` (Escape)
   is only printed.
5. **Close:** on `CloseRequested` for its window, after the lines of that iteration, destroy the
   window and return 0. A failure to destroy goes to stderr and does not change the exit code.
6. **Errors:** if the window cannot be created, report on stderr and return 1 without writing any
   line. If querying the size or scale, or presenting, fails, report on stderr, skip that window line
   and keep running.

### 4.4 Threading and lifetime
UI thread only. `windows` and `input` are borrowed for the call; in `main.cpp` both are the same SDL
object (`inputSourceOf`).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets).

## 5. Acceptance Criteria
"Injecting" means `FakeWindowSystem::injectInput` from the fake's wait hook; the example's window is
the fake's first window.

- **AC-1 (I1)** — On the fake at 640×400 and scale 1, before the first wait the log is exactly
  `intents: window 640x400 at scale 1` and the framebuffer is 640×400 of `kBackground`. After
  `changeDisplayScale(2)` the framebuffer is 1280×800 of `kBackground` and one more line
  `intents: window 640x400 at scale 2` is written; after a user resize to 500×300, one line
  `intents: window 500x300 at scale 2`.
- **AC-2 (I2)** — Injecting presses of Enter, Space, Escape, Tab, Shift+Tab, Up, Down, Left, Right
  and A (in one batch) writes, in order, `Activate`, `Activate`, `Cancel`, `Navigate Next`,
  `Navigate Previous`, `Navigate Up`, `Navigate Down`, `Navigate Left`, `Navigate Right` and nothing
  for A; key releases write nothing; the program keeps running after Escape.
- **AC-3 (I3)** — Injecting a pointer move to (10.5, 20), a Primary press and a Secondary release at
  the same point, and a scroll of (0, -1.5) there, all with `PointerId{1}`, writes exactly the lines
  `PointerMove 1 at (10.5, 20)`, `PointerDown 1 Primary at (10.5, 20)`,
  `PointerUp 1 Secondary at (10.5, 20)`, `Scroll 0, -1.5 at (10.5, 20)` (each prefixed).
- **AC-4 (I4)** — Injecting text `"é"` then `"ab"` writes `TextInput "é"` and `TextInput "ab"`.
- **AC-5 (I5)** — `describe()` writes every alternative and enumerator of §4.2: all six
  `NavigationDirection`s and all five `PointerButton`s, as in the table.
- **AC-6 (I6)** — When input for the example's window and a `CloseRequested` arrive in the same
  iteration, the intent lines are written, then `runIntents` returns 0 and the fake has no windows.
  Input injected for a second window (created by the test) writes nothing.
- **AC-7 (I7)** — If the window cannot be created (a failing double), `runIntents` returns 1 and
  writes nothing to the log. On SDL's `dummy` driver, a pushed SDL Enter key-down and a pushed close
  request give a log ending in `intents: Activate` and a return of 0.
- **AC-8 (I8)** — The `intents` executable is part of the default build.

**Manual check (recorded in the Change Log, not automated):** run `build/dev/examples/intents/intents`
on a Retina Mac and check: the window line says `at scale 2`; moving to the bottom-right corner gives
positions near the logical size (about 640, 400), not double; the trackpad's two-finger scroll gives
fractional `Scroll` values with a positive `dy` when the content would move up (rule 4 of SPEC-0026);
Enter, Space, Escape, Tab, Shift+Tab and the arrows print as in AC-2; typing `é` (Option-e, e) prints
`TextInput "é"`; left and right clicks print `Primary` and `Secondary`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `Intents.I1_WindowLineAndBackground` | unit | Fake; `changeDisplayScale`, `userResize` in the wait hook |
| AC-2 | `Intents.I2_KeysPrintTheirIntents` | unit | Fake; one batch of presses and releases |
| AC-3 | `Intents.I3_PointerAndScrollPrintInDips` | unit | Fake |
| AC-4 | `Intents.I4_TextPrintsUnchanged` | unit | Fake |
| AC-5 | `Intents.I5_DescribeEveryAlternative` | unit | Calls `describe` directly; expected strings written out |
| AC-6 | `Intents.I6_CloseAfterTheBatchAndOtherWindowsIgnored` | unit | Fake; second window created in the hook |
| AC-7 | `Intents.I7_FailureAndSdlDummyDriver` | unit + integration | Failing `WindowSystem` double; SDL decorator pushes events |
| AC-8 | `gate.examples.AC8_IntentsBuilds` | build | As `boxes`' B8 |

Expected lines are written out in the tests, not produced by `describe()`, so a formatting bug
cannot make a test agree with itself (except AC-5, which tests `describe()` itself against literals).

## 7. Non-Functional Requirements
- Deterministic: no wall-clock waits in tests (fake hooks; the SDL test pushes its events).
- No library changes: everything is example code.

## 8. Architectural Impact
None. The example links `core`, `platform` and `foundation`, and (in `main.cpp` and the SDL test only)
`platform_sdl3`. Examples are composition roots (ADR-0004) and are outside `tools/layering.json`.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — Show something in the window?** *Recommended:* a plain `kBackground` fill, presented at start
  and on size or scale changes (rules 1 and 3), so the window is not left with undefined contents.
  It reuses `presentImage` (SPEC-0022) with a `foundation::Image`; no renderer is needed.
- **Q2 — Where the output goes.** *Recommended:* stdout in `main.cpp` (the `log` stream), as `boxes`
  does; errors on stderr.
- **Q3 — Raw events too?** *Recommended:* intents only. The point is the default mapping as widgets
  will see it; a key that maps to nothing printing nothing is part of what the check shows.
- **Q4 — Pointer-move flood.** Moving the mouse prints many lines. *Recommended:* print every move;
  coalescing would hide what the platform actually delivers, which is what this check is for.
- **Q5 — Window line on changes (rule 3).** *Recommended:* yes, so after dragging the window to a
  non-Retina display the scale and the positions can be re-checked together.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0029): …`):
   - **Stubs:** `intents.hpp` as in §4.1; `describe` returns an empty string; `runIntents` creates
     the window, returns 0 on the first `CloseRequested` without writing anything, and returns 1 if
     creation fails. `main.cpp` and CMake targets.
   - **Tests:** I1–I8.
   - **Expected Red:** I1–I6 fail on assertions (no lines, black framebuffer, empty descriptions).
     I7's SDL half fails (no `Activate` line); its failure half and I8 pass with the stub, since a
     stub must create a window and the executable must build. They stay as regression tests and are
     listed as such in the Red output.
2. **Green:** `describe`; the background image; the loop (rules 2–5); error handling.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; manual check on a Retina display.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `examples/intents/{intents.hpp, intents.cpp, main.cpp, CMakeLists.txt}` | create | The example |
| `examples/intents/tests/{intents_test.cpp, CMakeLists.txt}` | create | I1–I8 |
| `CMakeLists.txt` | modify | `add_subdirectory(examples/intents)` |
| `docs/epics/EPIC-007-input-and-intents.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **SDL keyboard events on the `dummy` driver** may need the window to have keyboard focus. SPEC-0027's
  SK tests push key events on the same driver, so I7 follows their approach; if it does not carry over,
  I stop and propose an amendment rather than weaken I7.
- **Text input on macOS** depends on `SDL_StartTextInput` (SPEC-0027). If `é` does not arrive in the
  manual check, it is recorded and fixed in a platform spec, not here.

## 11. Definition of Done
- [ ] Red phase: new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes
- [ ] Manual check recorded
- [ ] Every AC ticked below with its proving test

### AC verification
- [ ] AC-1 → `Intents.I1_WindowLineAndBackground`
- [ ] AC-2 → `Intents.I2_KeysPrintTheirIntents`
- [ ] AC-3 → `Intents.I3_PointerAndScrollPrintInDips`
- [ ] AC-4 → `Intents.I4_TextPrintsUnchanged`
- [ ] AC-5 → `Intents.I5_DescribeEveryAlternative`
- [ ] AC-6 → `Intents.I6_CloseAfterTheBatchAndOtherWindowsIgnored`
- [ ] AC-7 → `Intents.I7_FailureAndSdlDummyDriver`
- [ ] AC-8 → `gate.examples.AC8_IntentsBuilds`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q5 approved as recommended | Emilio Pomares |
