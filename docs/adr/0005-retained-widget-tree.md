# ADR-0005: Retained widget tree, ownership, handles, type registry
Status: Accepted (2026-11-01)

## Context
Docking, workspace persistence, remote frontends, accessibility and plugins all need a persistent,
introspectable UI structure. Immediate-mode (Dear ImGui style) makes these hard.

## Decision
- **Retained mode.** Widgets form a tree. Each parent owns its children (`std::unique_ptr`).
- **Declarative builder API** for construction (similar in spirit to Slate's `SNew`), built from
  ordinary C++ (fluent builders, designated initializers). No custom preprocessor or code generator.
- **`WidgetHandle`**: a generational index (slot + generation) into a per-`UiContext` registry. Code
  outside a single call holds handles, never raw pointers. A stale handle resolves to empty.
- **Widget type registry.** Each widget class has a `WidgetTypeId` (stable string such as
  `"coil.Button"`) and a factory. This makes plugin widgets, serialization and remote construction
  possible.
- **Properties and dirty flags.** Widget state changes mark layout or paint dirty. Only dirty subtrees
  re-layout or re-paint.
- **Semantic info is mandatory** per widget (role, name, value, actions, `AutomationId`). See ADR-0015.

## Consequences
+ Introspectable, serializable, remote-able UI.
− More bookkeeping than immediate mode. Mitigated by dirty tracking and cached layout.

## Amendment 1 (2026-11-02): built-in widgets are ordinary clients
Status: Accepted (2026-11-02)

### Context
Coil ships built-in primitive widgets (`Label`, `Button`, panels, layouts, …), and applications and
plugins must be able to add their own components with their own rendering and input handling. If
built-ins could reach into `core` internals, custom components would always be second-class, and
nobody would notice until a plugin author hit the wall.

### Decision
- **No privileged access.** Built-in widgets use only the public API of `core` (and of the modules
  they depend on). No `friend` declarations for widget classes, no hooks reserved for built-ins, no
  includes of another module's `src/`.
- **One widget surface.** A widget, built-in or custom, provides the same four things:
  - **layout:** measure and arrange (ADR-0006, ADR-0019);
  - **paint:** records commands into a `Painter` / `DrawList` (ADR-0009);
  - **input:** `onIntent`, synchronous and fast (ADR-0007), emitting commands for real work
    (ADR-0008);
  - **semantics:** its `AutomationNode` (ADR-0015).
- **One registration path.** Custom widgets register in the same type registry as built-ins, whether
  they live in the application or in a plugin (ADR-0011).
- **Composition.** Composite widgets (for example `Button`, which holds a `Label` child) are built
  from other widgets through the public tree API, exactly as an application would build them.
- **Proof.** The widget-tree epic's example defines its widget in the example itself, outside the
  library, and uses it like a built-in.

### Consequences
+ Custom components can do everything built-ins can. Built-ins exercise the public API first,
  so gaps show up in Coil's own code.
− Some conveniences built-ins might want must be designed as public API (and supported) instead of
  being kept private.
