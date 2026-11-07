# EPIC-008: Widget tree

| Field | Value |
|-------|-------|
| Status | Approved |
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
  A widget can define its own hit shape (for example, a curve), and can capture the pointer so a drag
  keeps reaching it outside its bounds (ADR-0007).
  Keyboard intents go to the focused widget, and `Navigate` moves focus.
- Every widget publishes an `AutomationNode` (role, name, value, state, actions, stable
  `AutomationId`), and a tree snapshot can be taken. A widget can also publish **virtual
  sub-elements** for parts that are not widgets (for example, the links of a node editor).
- An example shows plain coloured panels that react to hover and focus (manual check plus a golden
  test). The panel widget is defined **in the example**, outside the library, and is registered and
  used exactly like a built-in (ADR-0005 Amendment 1).

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
| 5 | Hit testing (custom hit shapes), pointer-intent routing (bubbling, `Reply`) and pointer capture | Planned | 4; EPIC-007 |
| 6 | Focus and keyboard-intent routing (`Navigate`) | Planned | 5 |
| 7 | `AutomationNode` per widget, virtual sub-elements, and tree snapshot | Planned | 2 |
| 8 | Example: hover- and focus-aware panels, defined as a custom widget in the example (manual check, golden test) | Planned | 4–7; EPIC-006 |

## Notes
- ADR-0005 (incl. Amendment 1: built-ins are ordinary clients), ADR-0006/0019, ADR-0007, ADR-0010 (UI thread), ADR-0015.
- Specs 5 and 7 grew on 2026-11-02 (reference use cases). If either is too large for one spec when
  drafted, it splits, and so does the epic.
- Eight specs is at the epic's practical limit. If one grows during drafting, split the epic rather
  than the rules.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
| 2026-11-02 | Spec 8's panel is a custom widget defined in the example, proving ADR-0005 Amendment 1 (folded into spec 8 to keep the epic at eight specs) | Emilio Pomares |
| 2026-11-02 | Custom hit shapes and pointer capture (spec 5) and virtual automation sub-elements (spec 7), for the node-editor use case | Emilio Pomares |
| 2026-11-07 | Epic approved (roadmap to Milestone 2) | Emilio Pomares |
