# EPIC-004: Layout engine: rows, columns, grids and modifiers

| Field | Value |
|-------|-------|
| Status | In Progress |
| Date | 2026-11-01 |
| Origin | "Layouts: horizontal, vertical, grid, with their respective modifiers … along the lines of Flutter: wrap a component into a behavior modifier" |

## Goal
Coil can compute where every element of a UI goes. Given a window size and a tree of horizontal,
vertical and grid layouts, whose elements carry ordered modifier chains (padding, fixed or bounded size,
alignment, flex weight, grid cell; ADR-0019), it produces each element's rectangle in logical units
(DIPs). It is pure and headless,
so every rule is proven by unit tests without a window or a GPU.

## Done when
- Geometry value types (`Size`, `Point`, `Rect`, `Insets`) and Flutter-style `BoxConstraints` exist in
  DIPs.
- A layout node protocol (measure under constraints, then arrange into a rectangle) runs over a tree,
  starting from a root size such as a window's.
- Elements carry ordered modifier chains (ADR-0019). The layout modifiers are `padding`, `size` /
  `minSize` / `maxSize` and `align` / `center`; the parent-data modifier is `weight`, for flex inside
  rows and columns.
- `Row` (horizontal) and `Column` (vertical) layouts support main-axis and cross-axis alignment,
  spacing and flex weights.
- `Grid` supports fixed, flexible and content-sized tracks, with cell placement and spans set through
  the `gridCell` modifier.
- Every layout rule has unit tests, including overflow, zero space and min/max conflicts.

## Out of scope
- **Drawing** layouts. Nothing appears on screen until a render epic; until then results are checked as
  rectangles in tests.
- **Interactive resizing** (drag a splitter or a resize handle). It needs pointer intents, hit testing
  and the widget tree, so it gets its own epic after the input epic.
- **Window resizing.** Platform events and size controls are a separate small epic, sketched in Notes.
- The widget tree, `WidgetHandle`, styling and automation nodes (`core` / `widgets` epics). Layout nodes
  are designed so widgets can own them later.
- Layout caching and dirty-flag invalidation (ADR-0006). This is a follow-up spec once widgets exist.
- Scrolling, overlays/stacks, wrapping (flow) layouts, text measurement, right-to-left layout.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0015 Geometry types (`foundation`) and `BoxConstraints` (new `layout` module)](../specs/0015-geometry-and-box-constraints.md) | Done | ADR-0019 |
| 2 | [SPEC-0016 Layout element protocol (public: custom containers work like built-ins), tree pass, modifier chain, and the first modifier (`padding`)](../specs/0016-layout-elements-and-modifier-chains.md) | Done | 1 |
| 3 | [SPEC-0017 Size and position modifiers: `size`, `minSize` / `maxSize`, `align`, `center`](../specs/0017-size-and-alignment-modifiers.md) | Done | 2 |
| 4 | [SPEC-0018 `Row` / `Column` with alignment, spacing, and the `weight` modifier](../specs/0018-row-column-and-weight.md) | Done | 3 |
| 5 | `Grid` with fixed, flexible and content-sized tracks, and the `gridCell` modifier | Planned | 3 |
| 6 | Layout tree dump (text, for test failure messages and later for automation) | Planned | 2 |

## Notes
- **ADR needed first** (CLAUDE.md §1: new module, change in layering):
  [ADR-0019](../adr/0019-layout-engine-and-modifiers.md), which amends ADR-0006. Modifier model chosen
  2026-11-01: **Compose-style modifier chains** (option C), over Slate per-slot parameters (A) and
  Flutter wrapper nodes (B).
- **Custom containers (reference use case: node editor).** The element protocol of spec 2 is public.
  A container defined outside `layout` (for example a canvas that places nodes at free coordinates
  read from its children's parent data) measures and arranges its children exactly as `Row`,
  `Column` and `Grid` do (ADR-0005 Amendment 1). Spec 2 proves it with a test-defined container.
- **Candidate follow-up epic, "Window resizing":**
  - a `resizable` flag and minimum/maximum size on `WindowDesc`;
  - `setSize`;
  - a `WindowResized` event in DIPs, plus the DPI scale (ADR-0006);
  - SDL3 adapter and contract tests.
  
  It is independent of this epic. The two meet when the app loop lays out the root with the window's
  size (EPIC-003 / `core`).
- **Candidate follow-up epic, "Interactive layout":** a `Splitter` and a `Resizable` modifier dragged
  by pointer intents (device-agnostic, ADR-0007). Depends on the input and widget epics.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed | Emilio Pomares |
| 2026-11-01 | Modifier model changed to Compose-style chains (ADR-0019); geometry types moved to `foundation` | Emilio Pomares |
| 2026-11-01 | Epic approved; ADR-0019 accepted. Work order: after EPIC-005 (window resizing) | Emilio Pomares |
| 2026-11-02 | Spec 2: the element protocol is public, so custom containers work like built-ins (node-editor use case) | Emilio Pomares |
| 2026-11-02 | Paused after spec 4: specs 5 (`Grid`) and 6 (dump) are not needed for Milestone 1 or the button; EPIC-006 goes next, and this epic resumes later | Emilio Pomares |
