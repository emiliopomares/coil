# EPIC-013: UI host: windows that drive widget trees

| Field | Value |
|-------|-------|
| Status | Approved |
| Date | 2026-11-07 |
| Origin | Roadmap to Milestone 2 (2026-11-07): "I want to go for D", the "Click me" button. No epic yet connects a window, its widget tree, input and rendering |

## Goal
An application gives Coil a window and a root widget, and Coil does the rest: it lays the tree out
when the window changes size or scale, turns the window's input into routed intents, and repaints and
presents only when something changed. Examples and applications stop wiring this by hand.

## Done when
- A `UiWindow` (in `core`) binds one `WindowSystem` window, one `RenderBackend` target and one root
  widget. A resize or scale change re-lays out the tree and redraws.
- Raw input from that window goes through the input mapping (EPIC-007) and intent routing (EPIC-008)
  to the widgets, in DIPs.
- A widget that changes its appearance (for example, hover) or state invalidates itself. The
  invalidation requests a frame from the frame scheduler (EPIC-003 spec 2); nothing repaints while
  nothing changed.
- Frames reach the screen through `RenderBackend::capture` (EPIC-006 spec 5) and
  `WindowSystem::presentImage` (SPEC-0022), until a GPU backend presents directly.
- Everything is tested on the fake window system, `SoftwareRenderBackend` and `FakeClock`.

## Out of scope
- Multiple windows sharing one tree, docking and floating panels (EPIC docking).
- GPU presentation, vsync and continuous animation (EPIC-003 specs 3–4, GPU epics).
- Dirty-region (partial) repaint.

## Specs (in dependency order)
*Planned specs are listed by name. A spec gets its number only when its file is created.*

| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | `UiWindow`: bind window, render target and root widget; layout and repaint on resize and scale | Planned | EPIC-008 spec 4; EPIC-006 spec 5 |
| 2 | Input pipeline: the window's raw input → intent mapping → routing into the tree | Planned | 1; EPIC-007 spec 3; EPIC-008 specs 5–6 |
| 3 | Invalidation: widgets request repaints; frames are scheduled, coalesced and skipped when idle | Planned | 1; EPIC-003 spec 2 |

## Notes
- No new layering edge: `core` already depends on `platform`, `render` and `tasks`.
- ADR-0005 (retained tree), ADR-0007 (intents), ADR-0009 (rendering), ADR-0010 (UI thread).
- EPIC-008 spec 8's example becomes the first user of `UiWindow` instead of wiring things by hand.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Epic proposed (roadmap to Milestone 2) | |
| 2026-11-07 | Epic approved (roadmap to Milestone 2) | Emilio Pomares |
