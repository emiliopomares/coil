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
