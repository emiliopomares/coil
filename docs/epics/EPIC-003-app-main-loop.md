# EPIC-003: App main loop

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | Pacing discussion for `hello_window` (EPIC-002): "this combination of B, D and E is the right way to go" |

## Goal
Coil applications run on a main loop that uses no CPU while idle, renders continuously at the
display's rate while something animates, and also works on platforms where the host (not the
application) owns the loop.

## Done when
- **Idle (B):** with nothing animating, the loop blocks in `waitEvents` and uses ~0% CPU, reacting to
  input immediately.
- **Continuous (E):** while any viewport or animation requests frames, the loop renders every frame,
  paced by VSync, and returns to idle when no frame is requested.
- **Host-owned (D):** the same application code runs under a callback-driven runner (e.g. SDL3 main
  callbacks, required for the web and some mobile platforms).
- Mode switching and pacing are testable with fakes (`FakeClock`, fake window system and renderer).

## Out of scope
- Rendering itself (render epics); this epic only schedules frames.
- Multi-threaded rendering (ADR-0010 keeps submission on the UI thread for now).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | App runner port: own-loop vs callback-driven (`coil::app`) | Planned | EPIC-002 (`waitEvents`, `Clock`) |
| 2 | Frame scheduler: idle-wait vs continuous mode, frame requests | Planned | 1 |
| 3 | SDL3 main-callbacks runner adapter | Planned | 1 |
| 4 | VSync-paced continuous mode | Planned | 2; first render backend |

## Notes
- Builds on EPIC-002's `Clock` port and `WindowSystem::waitEvents(timeout)`.
- Spec 4 depends on a render epic that does not exist yet; the epic can be partly done before it.
- ADR-0010 (threading) and ADR-0001 apply. A dedicated ADR for the loop model may be needed when spec 1
  is drafted.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed | |
