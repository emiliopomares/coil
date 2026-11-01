# EPIC-005: Window resizing

| Field | Value |
|-------|-------|
| Status | Approved |
| Date | 2026-11-01 |
| Origin | "Window resizing controls" (2026-11-01). Separate small epic, done before EPIC-004. |

## Goal
Windows can be resized, by the user dragging their edges or by the program. The application learns
each new size as an event, can set limits, and knows how logical sizes (DIPs) map to pixels on the
window's display.

## Done when
- A window can be created resizable or fixed-size, with optional minimum and maximum sizes that the OS
  enforces.
- The program can resize a window, and every size change (by the user or the program) reaches the
  application as a `Resized` event carrying the new logical size.
- The application can query a window's pixel size and display scale, and is told when the scale
  changes (for example, when the window moves to a different monitor).
- All of this is covered by the window contract tests on the fake and on SDL's `dummy` driver.
- `hello_window` is resizable, and a recorded manual check shows its size events while you drag.

## Out of scope
- Laying anything out at the new size (EPIC-004; joined to window size by the app loop, EPIC-003).
- Window position, fullscreen, maximize/minimize, multi-monitor placement, and window icons.
- Interactive resizing of elements inside a window (splitters, resize handles): the later
  "Interactive layout" epic.
- Throttling of resize events, and redrawing during a live resize (render epics).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0010 `setSize` and the `Resized` event](../specs/0010-window-set-size-and-resized.md) | Done | EPIC-002 |
| 2 | [SPEC-0011 Resizable flag and minimum/maximum size](../specs/0011-window-resizable-and-size-limits.md) | In Progress | 1 |
| 3 | Pixel size, display scale and the `ScaleChanged` event | Planned | 1 |
| 4 | `hello_window`: resizable window that logs its size events (manual check) | Planned | 2 |

## Notes
- Builds on the `WindowSystem` port (SPEC-0003), its events (SPEC-0006) and `waitEvents` (SPEC-0009).
  New events become alternatives of the `WindowEvent` variant, which SPEC-0006 left open for this.
- `WindowSize` stays in integer logical units (as SDL3 reports them). Conversion to EPIC-004's `float`
  DIP geometry happens where layout meets the window (the app loop), not in the platform port.
- **Open questions for spec 1** (to be settled in its draft):
  - Does `setSize` produce a `Resized` event? Recommended: yes, so there is one code path.
  - What do the dummy driver and the fake do? Probably resize at once.
- **Risk:** what SDL's `dummy` driver does with sizes and scales must be checked while drafting specs 1
  and 3. Anything it can't simulate is covered by the fake plus the manual check, and recorded in the
  spec.
- Relies on ADR-0004 (ports and adapters), ADR-0006 and ADR-0019 (logical units), ADR-0018 (SDL3).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (separate epic, ordered before EPIC-004) | Emilio Pomares |
| 2026-11-01 | Epic approved | Emilio Pomares |
