# Epics

An epic is a goal too large for one work item. It is broken down into specs (work items), which are
implemented one at a time. An epic has no code of its own; it is done when all its specs are done.

| Epic | Goal | Status |
|------|------|--------|
| [EPIC-001](EPIC-001-engineering-foundation.md) | Engineering foundation | In Progress |
| [EPIC-002](EPIC-002-hello-window.md) | Hello window | Done |
| [EPIC-003](EPIC-003-app-main-loop.md) | App main loop | Approved |
| [EPIC-004](EPIC-004-layout-engine.md) | Layout engine: rows, columns, grids and modifiers | Approved |
| [EPIC-005](EPIC-005-window-resizing.md) | Window resizing | Done |
| [EPIC-006](EPIC-006-first-pixels.md) | First pixels: DrawList, software renderer, on screen | Proposed |
| [EPIC-007](EPIC-007-input-and-intents.md) | Input and intents | Proposed |
| [EPIC-008](EPIC-008-widget-tree.md) | Widget tree | Proposed |
| [EPIC-009](EPIC-009-commands-and-state.md) | Commands and state | Proposed |
| [EPIC-010](EPIC-010-text.md) | Text | Approved |
| [EPIC-011](EPIC-011-button.md) | Button | Approved |

Epic numbers are identifiers in order of creation, not a work order. **Current work order** (roadmap
to a button, 2026-11-01; EPIC-005 done 2026-11-02):

1. EPIC-004 (layout engine).
2. EPIC-006 (first pixels): **Milestone 1**, coloured boxes laid out and redrawn on resize.
3. EPIC-007 (input and intents).
4. EPIC-008 (widget tree).
5. EPIC-009 (commands and state). Specs 1–2 can run earlier, in parallel.
6. EPIC-010 (text). Specs 1–4 can run after EPIC-006, before or alongside EPIC-008.
7. EPIC-011 (button): **Milestone 2**, the "Click me" counter.

EPIC-001's remaining specs (CI matrix, compiler feature check) and EPIC-003 (app main loop) are
scheduled as needed.
