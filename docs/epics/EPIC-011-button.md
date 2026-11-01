# EPIC-011: Button

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | "Can we plan forward into placing some interesting content? (say a button …)" (2026-11-01) |

## Goal
Coil has its first interactive widget: a labelled `Button`. It reacts visually to hover, press, focus
and disabled state, emits a command when activated by any device (pointer click or `Activate`
intent), and is fully automatable.

## Done when
- `Button` (`widgets` module) paints its states: normal, hover, pressed, focused and disabled.
- Activation emits its `Command` through the `CommandRouter` for a pointer click (down and up inside)
  or an `Activate` intent while focused, and never while disabled.
- Its `AutomationNode` has role button, name from its label, a `press` action and its enabled state.
- **Milestone 2:** an example shows a "Click me" button and a label bound to a counter in the
  `PropertyStore`, reading "Clicked N times". Recorded manual check plus a golden-image test.

## Out of scope
- Styling and theming systems, icons, toggle and radio buttons, keyboard shortcuts and accelerators,
  tooltips, pressing through MCP (later automation/MCP epics).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | `Button` widget: visual states and painting | Planned | EPIC-008, EPIC-010 |
| 2 | Activation by pointer click or `Activate` intent emits a command | Planned | 1; EPIC-009 |
| 3 | Button automation node (role, name, `press` action, enabled state) | Planned | 1 |
| 4 | Example: "Click me" counter (manual check, golden test) | Planned | 2, 3 |

## Notes
- ADR-0007 (activation from any device), ADR-0008/0014 (commands), ADR-0015 (automation).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
