# Epics

An epic is a goal too large for one work item. It is broken down into specs (work items), which are
implemented one at a time. An epic has no code of its own; it is done when all its specs are done.

| Epic | Goal | Status |
|------|------|--------|
| [EPIC-001](EPIC-001-engineering-foundation.md) | Engineering foundation | In Progress |
| [EPIC-002](EPIC-002-hello-window.md) | Hello window | Done |
| [EPIC-003](EPIC-003-app-main-loop.md) | App main loop | Approved |
| [EPIC-004](EPIC-004-layout-engine.md) | Layout engine: rows, columns, grids and modifiers | Approved |
| [EPIC-005](EPIC-005-window-resizing.md) | Window resizing | In Progress |

Epic numbers are identifiers in order of creation, not a work order. **Current work order:**
EPIC-005 (window resizing) → EPIC-004 (layout engine). EPIC-001's remaining specs (CI matrix,
compiler feature check) and EPIC-003 are scheduled as needed.
