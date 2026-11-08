# EPIC-015: Editor scale: settle the load-bearing decisions and prove them under load

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-07 |
| Origin | Assessment of 2026-11-07: "I expect this library to be powerful and flexible enough to build applications on top of it like video editors, node-based FX software or game engines … something that at least comes close to Qt" |

## Goal
Before the standard widget set grows, settle the decisions that shape every widget at editor scale
(frame model, rendering, styling, virtualization, performance gates), and prove them with a
node-graph demo holding thousands of items that stays within a measured frame budget.

## Done when
- ADRs A–E (Notes) are Accepted.
- A benchmark harness runs in `tools/check.sh` and CI, and fails the build when a budget is exceeded.
- A change to one widget redraws only its damaged region, and unchanged subtrees reuse their
  recorded paint from the previous frame.
- A widget can draw and hit-test thousands of its own items through the canvas-item API, with pan
  and zoom.
- A virtualized list materializes only its visible rows, whatever the item count.
- The node-graph example (2,000 nodes and 3,000 links) pans, zooms and drags nodes within the budgets
  set by ADR E, and its integration test drives it through `AutomationService`.

## Out of scope
- **Implementing a GPU backend.** It is its own epic, planned after ADR B is accepted. This epic's
  budgets cover UI-side work (layout, paint recording, hit testing) and software rasterization.
- **Styling and theming implementation.** ADR C sets the model; a later epic implements it.
- Text editing, IME composition, bidirectional text (EPIC-010 follow-ups).
- Gaps found in the 2026-11-07 assessment (ranked in `docs/architecture/PRODUCT-SCOPE.md`), each a future epic: multiple windows; drag and drop
  (including OS file drops); clipboard; cursor shapes; native menus and file dialogs; undo/redo
  and the editor property model (ADR-0008 follow-up).

## Specs (in dependency order)
*Planned specs are listed by name. A spec gets its number only when its file is created.*

| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | Benchmark harness and budget gate | Planned | ADR E |
| 2 | Damage tracking: invalidated widgets become dirty regions; partial redraw in the software backend | Planned | ADR A; EPIC-008, EPIC-013 |
| 3 | Paint caching: unchanged subtrees reuse their recorded `DrawList` fragments | Planned | 2 |
| 4 | Canvas-item API: a widget draws and hit-tests N items through a spatial index | Planned | ADR D; EPIC-008 |
| 5 | Canvas pan and zoom (view transform, zoom-aware hit testing) | Planned | 4; EPIC-012 (transforms) |
| 6 | Virtualized list: only visible rows are materialized and automated | Planned | ADR D; EPIC-010 (`Label`) |
| 7 | Example: node graph at scale, within budget, tested through `AutomationService` | Planned | 1, 3, 5; EPIC-012 (Béziers), EPIC-014 |

## Notes
**Prerequisite ADRs (each drafted, then accepted by the user before the specs that need it):**
- **ADR A: frame model and invalidation.** What marks a widget dirty (layout or paint only), how
  dirty regions are computed and merged, which subtrees cache their paint, and how this fits the
  frame scheduler (EPIC-003) and immutable `DrawList` snapshots (ADR-0010).
- **ADR B: first GPU backend and streamed content.** Which API comes first (ADR-0009 says Vulkan,
  MoltenVK on macOS; Metal is the alternative), and how frequently changing content (decoded video
  frames, engine viewports at 60 fps) reaches the screen: texture updates, synchronization, and
  ownership across the plugin boundary. It extends ADR-0009's `GpuSurface` rather than replacing it.
- **ADR C: styling and theming.** How widgets get colours, metrics and fonts (tokens, a theme
  object, per-widget overrides), so built-in and custom widgets (ADR-0005 Amendment 1) share one
  model and nothing hard-codes its look.
- **ADR D: item virtualization and the canvas-item API.** When a widget owns lightweight items
  instead of child widgets; how those items are hit-tested, focused and exposed to automation
  (virtual automation sub-elements, ADR-0015); and the data-source contract for virtualized lists,
  trees and tables.
- **ADR E: performance budgets as gates.** Amends ADR-0003: benchmarks are a separate test kind
  that may use wall-clock time (unit tests still may not). Covers the reference machine, budgets
  checked as medians over N runs, and how CI noise is handled. Any benchmark library goes through
  ADR-0018.

**Placement:** after Milestone 2 (EPIC-011), before the standard widget set grows. It needs EPIC-003,
EPIC-008, EPIC-012, EPIC-013 and EPIC-014.

**Open decision for the user (not part of this epic's approval):** whether to allow throwaway
*spikes*. These would be exploratory prototypes outside `modules/` and `backends/`, which never merge,
for areas where the right design is unknown (GPU backends, text shaping, caching strategies). They
would inform ADRs A, B and D. Allowing them needs a change to CLAUDE.md §1.

**Risks:** budgets set too early may be arbitrary. ADR E should set them from measurements of
spec 1's harness, not guesses.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Epic proposed (from the editor-scale assessment) | |
| 2026-11-08 | Goal confirmed; done-when and spec list still to approve. Gaps recorded in `docs/architecture/PRODUCT-SCOPE.md` | Emilio Pomares |
