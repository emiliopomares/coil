# ADR-0019: Layout engine module, box constraints and modifier chains
Status: Accepted (2026-11-01). Amends ADR-0006.

## Context
EPIC-004 adds rows, columns and grids. ADR-0006 fixed the two-pass layout (measure, arrange) in DIPs,
but placed layout in `core` and attached layout parameters to the parent's slots (Slate style). Three
models were compared (discussion 2026-11-01):
- **A. Per-slot parameters (Slate).** Shallow and well typed, but not composable: every container
  re-implements padding, size and alignment, they cannot be applied to a root, and plugins cannot add
  new ones.
- **B. Wrapper nodes (Flutter).** Freely composable and order-explicit, but they add 3–5 meaningless
  nodes per element. That is costly where Coil depends on its tree: the automation tree for agents
  (ADR-0015), serialized workspaces (ADR-0012), remote frontends and widget handles (ADR-0005).
  Adding or removing a modifier also changes the tree structure.
- **C. Modifier chains (Jetpack Compose).** An element carries an ordered list of modifiers as data.
  This gives B's composability and ordering with A's shallow tree.

Layout is also pure computation. Waiting for the widget tree (`core`) would delay it for no benefit.

## Decision
1. **Module.** A new module `layout` depends on `foundation` only. `core` gains `layout` as a direct
   dependency, and `widgets` builds on it through `core`. Geometry value types (`Size`, `Point`,
   `Rect`, `Insets`) live in `foundation`, as OVERVIEW §3 already plans, so `render` can use them too.
2. **Constraint model (Flutter).** `BoxConstraints` (min/max width and height; max may be infinite) go
   down and sizes come up, and then the parent positions its children. This keeps ADR-0006's two
   passes:
   - `measure(BoxConstraints) → Size`. The result always satisfies the constraints.
   - `arrange(Rect)`.

   All values are `float` DIPs.
3. **Modifiers are an ordered chain on the element (Compose), not nodes in the tree.**
   - Every layout element (leaf or container) has a `ModifierChain`.
   - Order matters. The first modifier is outermost, so `padding(8).size(100)` means 100×100 inside
     8 DIPs of padding.
   - There are two kinds:
     - **Layout modifiers** (`padding`, `size`, `minSize`/`maxSize`, `align`, …) change the
       constraints going down, the size coming up, and the child's position.
     - **Parent-data modifiers** (`weight` for rows and columns, `gridCell` for grids) are read by the
       parent container. Other parents ignore them, and the layout tree dump reports them as unused.
4. **Containers are elements with children.** `Row`, `Column` and `Grid` have container-wide settings
   (spacing, alignment, tracks). Per-child settings come from the child's parent-data modifiers.
   ADR-0006's per-slot parameters are superseded.
5. **Extensible and serializable.**
   - A modifier implements a small `LayoutModifier` interface. The built-in modifiers use the same
     interface, so plugins can add modifiers.
   - Each modifier type has a stable type id (for example `"coil.padding"`) and immutable values.
     This is enough to serialize a chain later (ADR-0005 registry).
   - Changing modifiers replaces the chain and marks the element for layout.
6. **Widgets expose the same chain** (later `core` / `widgets` epics). The automation tree,
   serialization and handles see one widget with a modifier list, never wrapper nodes.

**Implementation latitude:** internally, the engine may run a chain as nested stages, each one
transforming constraints, size and position like a wrapper would. That structure is never visible in
the element tree.

## Consequences
+ Flutter-style composition and explicit order, one implementation per modifier, plugin-extensible.
+ One element per visible thing: agents, workspaces and remote frontends see meaningful nodes.
+ Layout is testable now, headless and without widgets (EPIC-004).
− The engine must run modifier chains, which is more machinery than plain wrapper nodes.
− A misplaced parent-data modifier is a runtime finding (reported by the dump), not a compile error.
− `layout` is a new module: `tools/layering.json` and OVERVIEW §3 change when its first spec lands.
