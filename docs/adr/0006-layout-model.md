# ADR-0006: Two-pass layout in logical units
Status: Accepted (2026-11-01)

## Decision
- Layout has two passes: **measure** (bottom-up desired size given constraints) and **arrange**
  (top-down final rects). Both are pure functions of widget state and constraints, which makes them
  easy to test.
- Layout containers are widgets: `HBox`, `VBox`, `Grid`, `Overlay`, `Splitter`, `ScrollView`, … with
  per-slot parameters (padding, alignment, fill/stretch weight, min/max).
- All core geometry is in **logical units (DIPs)**. Each window has a DPI scale from the platform port.
  Conversion to pixels happens in render backends only. Pixel-snapping rules are defined by the render
  backend spec.
- Layout is cached and invalidated by dirty flags.

## Consequences
+ Deterministic and fast; DPI-independent tests.
− Flexbox-level expressiveness grows over time, one spec per container.
