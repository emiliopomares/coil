# SPEC-0014: `hello_window` prints its pixel size and display scale

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `examples/hello_window` only |
| Epic | [EPIC-005](../epics/EPIC-005-window-resizing.md) (spec 5) |
| Related ADRs | ADR-0004 (the example is a composition root), ADR-0006 (DIPs vs pixels) |
| Depends on | SPEC-0012 (`hello_window` log), SPEC-0013 (`pixelSize`, `displayScale`, `ScaleChanged`) |

> **Scope Gate checklist**
> - [x] One concern: the example reports how many physical pixels its window has, and at what scale
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 area (`examples/hello_window`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
SPEC-0013 added `pixelSize`, `displayScale` and `ScaleChanged`, but the SDL `dummy` driver always
reports scale 1.0, so no automated test sees a real Retina value. This spec makes `hello_window` print
them. Run it on a Retina Mac and it should say `hello_window: 1600x1200 pixels at scale 2`. Drag it to
a display with a different density and it prints the new values.

## 2. Scope
- `hello_window.cpp`:
  - once the window is created, write the scale line;
  - write it again for each `ScaleChanged` of its own window.
- Tests: new S1–S5. SPEC-0012's R2–R5 expectations gain the start line (an amendment, §9 Q2).

## 3. Out of Scope
- Printing pixel sizes on every resize (the resize line stays as it is).
- Drawing anything (EPIC-006).
- The Windows/X11 DIP gap (SPEC-0013 Q3).

## 4. Behavior Specification

### 4.1 Public API
No signature change. The header documentation of `runHelloWindow` gains the new line:
```cpp
/// … Writes to `log`: "hello_window: <pw>x<ph> pixels at scale <s>" once the window exists and on
/// each display-scale change of that window, and "hello_window: resized to <w>x<h>" on each size
/// change of that window. …
```

### 4.2 Behavioral rules
1. Right after creating its window, and before the first event call, `runHelloWindow` MUST write
   `hello_window: <pw>x<ph> pixels at scale <s>\n`. Here `<pw>x<ph>` is `pixelSize`, and `<s>` is
   `displayScale` formatted with `std::format("{}")`, so `1`, `1.5`, `2`.
2. For each `ScaleChanged` event for its own window, it MUST write the same line, with the event's
   scale and the current `pixelSize`.
3. All lines for one batch of events are written in event order (resize and scale lines interleaved
   as they occurred), before the close check (SPEC-0012 rule 2).
4. `ScaleChanged` events for other windows MUST NOT be logged.
5. If `pixelSize` or `displayScale` fails, the error goes to stderr, no scale line is written for
   that occasion, and the program continues.
6. SPEC-0012 rule 4 ("nothing else is written to `log`") is amended: the scale line is the other
   allowed line.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| A query fails | Error on stderr; no scale line; the program keeps running |
| Scale changes twice between waits | One line (coalesced, SPEC-0013 rule 4) |
| Window created on a Retina display | Start line `1600x1200 pixels at scale 2` (manual check) |

### 4.4 Threading and lifetime
Unchanged.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1 (S1)** — Given a `FakeWindowSystem`, by the first event call `log` is exactly
  `"hello_window: 800x600 pixels at scale 1\n"`.
- **AC-2 (S2)** — Given `changeDisplayScale(program, 2.0)` before call 1 and a close request before
  call 2, `log` is the start line followed by `"hello_window: 1600x1200 pixels at scale 2\n"`.
- **AC-3 (S3)** — Given a second window created by the test whose scale changes before call 1, and a
  close request for the program's window before call 2, `log` is only the start line.
- **AC-4 (S4)** — Given a user resize to 1024×768 and then `changeDisplayScale(program, 1.5)`, both
  before call 1, and a close request before call 2, `log` is the start line, then
  `"hello_window: resized to 1024x768\n"`, then `"hello_window: 1536x1152 pixels at scale 1.5\n"`.
- **AC-5 (S5)** — Integration: on SDL's `dummy` driver, `log` starts with
  `"hello_window: 800x600 pixels at scale 1\n"`, and the program exits 0 on a close request.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `HelloWindow.S1_PrintsPixelSizeAndScaleAtStart` | unit | fake; log checked in the callback |
| AC-2 | `HelloWindow.S2_PrintsScaleChanges` | unit | fake `changeDisplayScale` |
| AC-3 | `HelloWindow.S3_IgnoresScaleChangesOfOtherWindows` | unit | fake |
| AC-4 | `HelloWindow.S4_LinesFollowEventOrder` | unit | fake |
| AC-5 | `HelloWindow.S5_PrintsScaleOnSdl` | integration | SDL `dummy` |

**SPEC-0012 test amendment (§9 Q2):** R2, R4 and R5 expect the start line before their resize lines,
and R3 expects exactly the start line. Nothing else in those tests changes. Their names stay, and
they go through this spec's Red.

## 7. Non-Functional Requirements
- No new dependency.

## 8. Architectural Impact
None.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares), including Q2 (SPEC-0012 test expectations amended).
- **Q1 — Line format?** *Recommended: `hello_window: 1600x1200 pixels at scale 2`* (scale through
  `std::format("{}")`, so no trailing zeros).
- **Q2 — Changing SPEC-0012's test expectations.** The start line changes what R2–R5 see in `log`.
  *Recommended: amend SPEC-0012 rule 4 and update those four expected strings* (only adding the start
  line). The alternative is a separate stream for the scale line, which would complicate `main` and
  the API just to avoid touching expectations.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0014): …`):
   - **Stub:** none needed. The current code is the stub, because it never writes the scale line.
   - **Tests:** S1–S5 added; R2–R5 expectations updated as in §6. Lint the test code.
   - **Expected Red:** S1–S5 fail (no scale line), and so do R2–R5, because their amended expectations
     contain the start line. The other tests pass. No guards.
2. **Green:** after `createWindow`, write the scale line. In the event loop, handle `Resized` and
   `ScaleChanged` in order through one visitor.
3. **Refactor:**
   - `tools/check.sh`, `asan`, `leaks` on S5.
   - **Manual check on your Retina screen:**
     1. Run `hello_window`.
     2. Expect `1600x1200 pixels at scale 2` (or your display's values).
     3. Optionally, drag the window to an external display and watch the new line.

     Record the result.
   - Docs: EPIC-005 (all specs done → Done after your acknowledgement), and SPEC-0012's Change Log.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `examples/hello_window/hello_window.{hpp,cpp}` | modify | Doc comment, behavior |
| `examples/hello_window/tests/hello_window_test.cpp` | modify | S1–S5; R2–R5 expectations |
| `docs/specs/0012-hello-window-resizable.md` | modify | Change Log: rule 4 amended by SPEC-0014 |
| `docs/epics/EPIC-005-window-resizing.md` | modify | Status |

### 10.3 Risks / alternatives considered
- **Float formatting:** `std::format("{}", 2.0F)` prints `2`, and `1.5F` prints `1.5`. A display
  scale such as 1.25 prints exactly, but an unusual one (for example 1.3333334) prints all its
  digits. That's acceptable for a diagnostic line.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes; macOS `leaks` reports 0 leaks for S5
- [x] Every AC ticked below with its proving test
- [x] **Manual check** on a Retina display recorded

### AC verification
- [x] AC-1 → `HelloWindow.S1_PrintsPixelSizeAndScaleAtStart`
- [x] AC-2 → `HelloWindow.S2_PrintsScaleChanges`
- [x] AC-3 → `HelloWindow.S3_IgnoresScaleChangesOfOtherWindows`
- [x] AC-4 → `HelloWindow.S4_LinesFollowEventOrder`
- [x] AC-5 → `HelloWindow.S5_PrintsScaleOnSdl`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q2 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: S1–S5 fail on assertions (no scale line), and R2–R5 fail with their amended expectations (start line missing); the other 122 tests pass. No stub needed (the current code is the stub). `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green + Refactor**: the scale line is written after `createWindow` (query errors → stderr) and on each own `ScaleChanged`, with resize and scale lines in event order. `tools/check.sh` exit 0 (131/131); `ctest --preset asan` 131/131; macOS `leaks`: 0 leaks for S5. `git diff 83a17d0 -- ':(glob)**/tests/**'` empty. **Manual check (first run):** printed `800x600 pixels at scale 1` on a Retina display. Cause: SDL windows lack `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so macOS gives them a 1× back buffer (SPEC-0013 amendment B). Manual check pending that fix. | Emilio Pomares |
| 2026-11-02 | **Manual check** (after SPEC-0013 amendments B and C): on a Retina Mac, `hello_window` prints exactly one line, `hello_window: 1600x1200 pixels at scale 2`, and exits 0 on close. Moving the window to a display of another density was not tried (no second display). Awaiting acknowledgement to set Status → Done. | Emilio Pomares |
| 2026-11-02 | Acknowledged; Status → Done | Emilio Pomares |
