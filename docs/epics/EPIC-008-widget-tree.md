# EPIC-008: Widget tree

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01) |

## Goal
Coil has a retained widget tree (ADR-0005). Widgets are referenced by generational `WidgetHandle`s,
carry modifier chains (ADR-0019), lay out, paint into a `DrawList`, receive routed intents
(hit-tested pointer intents, focused keyboard intents), and publish semantic automation data
(ADR-0015).

## Done when
- `UiContext`, a `Widget` base and `WidgetHandle` exist; stale handles resolve to empty.
- Parents own children. Adding and removing children works, and handles of removed widgets go stale.
- Widgets lay out through the layout engine using their modifier chains.
- A paint traversal produces a `DrawList` with correct clipping.
- Pointer intents are hit-tested and bubble from the target to its ancestors until handled (`Reply`).
  Keyboard intents go to the focused widget, and `Navigate` moves focus.
- Every widget publishes an `AutomationNode` (role, name, value, state, actions, stable
  `AutomationId`), and a tree snapshot can be taken.
- An example shows plain coloured panels that react to hover and focus (manual check plus a golden
  test).

## Out of scope
- Standard widgets beyond a minimal panel (`widgets` module: Label in EPIC-010, Button in EPIC-011).
- Commands and property binding (EPIC-009), the `AutomationService`, MCP, styling and theming,
  animation, dirty-region rendering.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | `UiContext`, `Widget` base, `WidgetHandle` (generational registry) | Planned | EPIC-007 spec 3 (`core` exists) |
| 2 | Tree ownership and mutation; stale handles | Planned | 1 |
| 3 | Widget layout through modifier chains | Planned | 2; EPIC-004 |
| 4 | Paint traversal into a `DrawList`, with clipping | Planned | 3; EPIC-006 spec 1 |
| 5 | Hit testing and pointer-intent routing (bubbling, `Reply`) | Planned | 4; EPIC-007 |
| 6 | Focus and keyboard-intent routing (`Navigate`) | Planned | 5 |
| 7 | `AutomationNode` per widget and tree snapshot | Planned | 2 |
| 8 | Example: hover- and focus-aware panels (manual check, golden test) | Planned | 4–7; EPIC-006 |

## Notes
- ADR-0005, ADR-0006/0019, ADR-0007, ADR-0010 (UI thread), ADR-0015.
- Eight specs is at the epic's practical limit. If one grows during drafting, split the epic rather
  than the rules.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
