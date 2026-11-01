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
| 1 | [SPEC-0002 `coil::Error`](../specs/0002-coil-error.md) | In Progress | SPEC-0001 |
| 2 | Window port + fake + contract tests | Planned | `coil::Error` |
| 3 | SDL3 window adapter | Planned | Window port |
| 4 | Window events (`CloseRequested`) | Planned | SDL3 window adapter |
| 5 | `hello_window` example | Planned | Window events |

## Notes
- Open questions for the window port spec: window ownership (system-owned `WindowId`, recommended,
  vs caller-owned `unique_ptr<Window>`); empty title allowed (recommended) or error; size limits
  (reject ≤ 0 only, recommended).
- Relies on ADR-0004 (ports & adapters), ADR-0006 (logical units), ADR-0013 (errors), ADR-0018 (SDL3).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed and approved | Emilio Pomares |
