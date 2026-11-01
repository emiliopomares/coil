# EPIC-002: Hello window

| Field | Value |
|-------|-------|
| Status | In Progress |
| Date | 2026-11-01 |
| Origin | "Make the library just open up a window with a title" |

## Goal
An example application opens a titled OS window through Coil's own `WindowSystem` port, backed by the
SDL3 adapter, and stays open until the user closes it.

## Done when
- `WindowSystem` port exists with a fake and a contract test suite.
- The SDL3 adapter passes the same contract suite headless (SDL offscreen video driver).
- A close request from the OS reaches the application as an event.
- `examples/hello_window` opens "Coil" at 800×600 and exits cleanly when closed: a headless smoke test
  in CI plus one recorded manual visual check.

## Out of scope
- Rendering anything inside the window, input beyond close requests, resizing, DPI scale, position,
  fullscreen, icons, multiple-window behavior beyond what the contract tests need.
- The `app` composition root / main loop abstraction (a later epic).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0002 `coil::Error`](../specs/0002-coil-error.md) | Done | SPEC-0001 |
| 2 | [SPEC-0003 Window port + fake + contract tests](../specs/0003-window-port.md) | Done | SPEC-0002 |
| 3 | [SPEC-0005 SDL3 window adapter](../specs/0005-sdl3-window-adapter.md) | Done | SPEC-0003, SPEC-0004 |
| 4 | [SPEC-0006 Window close events](../specs/0006-window-close-events.md) | Done | SPEC-0003, SPEC-0005 |
| 5 | [SPEC-0008 `Clock` port + `FakeClock`](../specs/0008-clock-port.md) | Done (pending acknowledgement) | SPEC-0002, SPEC-0003 |
| 6 | Window `waitEvents(timeout)` | Planned | `Clock` port, SPEC-0006 |
| 7 | [SPEC-0007 `hello_window` example](../specs/0007-hello-window-example.md) | Draft (on hold: revise to use `waitEvents`) | `waitEvents`, SPEC-0005 |

## Notes
- Open questions for the window port spec: window ownership (system-owned `WindowId`, recommended,
  vs caller-owned `unique_ptr<Window>`); empty title allowed (recommended) or error; size limits
  (reject ≤ 0 only, recommended).
- 2026-11-01: event-loop pacing decided as **option B** (block in `waitEvents(timeout)` until an event
  arrives or the timeout expires) instead of a fixed sleep. Testing the timeout deterministically needs
  a `Clock` port with a `FakeClock` (CLAUDE.md §3: no wall-clock time in unit tests), hence specs 5 and
  6. The full loop design (wait when idle, continuous frames when animating, host-owned loops) is
  EPIC-003.
- Relies on ADR-0004 (ports & adapters), ADR-0006 (logical units), ADR-0013 (errors), ADR-0018 (SDL3).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed and approved | Emilio Pomares |
| 2026-11-01 | Spec list changed: `Clock` port and `waitEvents(timeout)` inserted before `hello_window` (pacing option B) | Emilio Pomares |
