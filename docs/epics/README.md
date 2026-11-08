# Epics

An epic is a goal too large for one work item. It is broken down into specs (work items), which are
implemented one at a time. An epic has no code of its own; it is done when all its specs are done.

| Epic | Goal | Status |
|------|------|--------|
| [EPIC-001](EPIC-001-engineering-foundation.md) | Engineering foundation | In Progress |
| [EPIC-002](EPIC-002-hello-window.md) | Hello window | Done |
| [EPIC-003](EPIC-003-app-main-loop.md) | App main loop | Approved |
| [EPIC-004](EPIC-004-layout-engine.md) | Layout engine: rows, columns, grids and modifiers | In Progress |
| [EPIC-005](EPIC-005-window-resizing.md) | Window resizing | Done |
| [EPIC-006](EPIC-006-first-pixels.md) | First pixels: DrawList, software renderer, on screen | In Progress |
| [EPIC-007](EPIC-007-input-and-intents.md) | Input and intents | Done |
| [EPIC-008](EPIC-008-widget-tree.md) | Widget tree | Approved |
| [EPIC-009](EPIC-009-commands-and-state.md) | Commands and state | Approved |
| [EPIC-010](EPIC-010-text.md) | Text | Approved |
| [EPIC-011](EPIC-011-button.md) | Button | Approved |
| [EPIC-012](EPIC-012-vector-drawing.md) | Vector drawing: paths, curves, transforms, images | Proposed |
| [EPIC-013](EPIC-013-ui-host.md) | UI host: windows that drive widget trees | Approved |
| [EPIC-014](EPIC-014-automation-service.md) | In-process automation service | Approved |
| [EPIC-015](EPIC-015-editor-scale.md) | Editor scale: load-bearing decisions, proven under load | Proposed |

Epic numbers are identifiers in order of creation, not a work order. **Current work order** (roadmap
to a button, 2026-11-01; EPIC-005 done 2026-11-02):

1. EPIC-004 (layout engine): specs 1–4 done; `Grid` and the dump paused until after Milestone 1.
2. EPIC-006 (first pixels): **Milestone 1**, coloured boxes laid out and redrawn on resize.
3. EPIC-007 (input and intents).
4. EPIC-008 (widget tree).
5. EPIC-009 (commands and state). Specs 1–2 can run earlier, in parallel.
6. EPIC-010 (text). Specs 1–4 can run after EPIC-006, before or alongside EPIC-008.
7. EPIC-011 (button): **Milestone 2**, the "Click me" counter.
8. EPIC-012 (vector drawing). Not needed for Milestone 2; can run any time after EPIC-006.

**Roadmap to Milestone 2** (approved 2026-11-07; replaces the order above). Tracks A–C can interleave; each step lists its specs.

1. EPIC-006: spec 6 (SPEC-0023, Milestone 1), then spec 5 (capture). Spec 3b (gradients) moves after
   Milestone 2.
2. Track A, input: EPIC-007 specs 1–3 (spec 4, the intent-printing example, is optional).
3. Track B, async: EPIC-009 specs 1–2 (`tasks`, `UiDispatcher`).
4. Track C, text: EPIC-010 specs 1–4 (ports, FreeType/HarfBuzz, glyph runs, measurement).
5. EPIC-008 specs 1–7 (widget tree).
6. EPIC-003 specs 1–2 (app runner, frame scheduler).
7. EPIC-013 specs 1–3 (UI host), then EPIC-008 spec 8 (hover/focus panels example, on `UiWindow`).
8. EPIC-009 specs 3–5 (commands, continuations, properties).
9. EPIC-010 specs 5–6 (`Label`, labels example).
10. EPIC-014 specs 1–2 (automation service).
11. EPIC-011 specs 1–4: **Milestone 2**, the "Click me" counter, tested through `AutomationService`.

**After Milestone 2 (proposed 2026-11-07):** EPIC-015 (editor scale), with its prerequisite ADRs
A–E drafted first, before the standard widget set grows.

EPIC-001's remaining specs (CI matrix, compiler feature check) and EPIC-003 (app main loop) are
scheduled as needed.

Product scope and the ranked list of future needs (which epics cover them, and which have none
yet): [../architecture/PRODUCT-SCOPE.md](../architecture/PRODUCT-SCOPE.md).

## Reference use cases
Every epic is checked against these before its specs are drafted: could someone build them as
**custom components** (ADR-0005 Amendment 1), with no access that built-ins don't have?

| Use case | What it needs | Where |
|----------|---------------|-------|
| Hue/saturation colour wheel | Custom widget; pointer position → value; keyboard/gamepad adjust; sweep and radial gradients (or a computed image); automation value | EPIC-006 (gradients), EPIC-007/008, EPIC-009, EPIC-012 (`drawImage`) |
| Node-based editor | Free-position custom container; Bézier links; pan and zoom; pointer capture for drags; custom hit shapes for links; virtual automation sub-elements for links | EPIC-004 spec 2, EPIC-012, EPIC-008 specs 5 and 7 |
