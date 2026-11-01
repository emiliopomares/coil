# ADR-0012: Docking as model/view; serializable workspaces
Status: Accepted (2026-11-01)

## Decision
- **`DockModel`** (pure data, no widgets or windows): a tree of `Split{orientation, ratios}`,
  `TabStack{panels, active}` and `FloatingWindow{rect, root}` nodes referencing
  `PanelInstance{PanelInstanceId, PanelTypeId, state: Value}`. Operations (dock, undock/detach,
  move, close, resize, activate) are pure model transforms with validation. These are unit-tested in
  depth.
- **`DockView`** reconciles widgets and OS windows with the model through the `PanelFactory` registry
  (built-in and plugin panel types) and the `WindowSystem` port.
- **Workspace** = named, serialized `DockModel` plus per-panel state. Format: versioned JSON with a
  migration function per version. Unknown panel types (missing plugin) are kept as placeholders and
  never dropped.
- Drag-to-dock interaction is input-agnostic: it is driven by `DragBegin/Update/End` intents
  (ADR-0007), so it also works with gamepad or touch.

## Consequences
+ Layout logic is fully testable without windows. Layouts survive missing plugins.
− View reconciliation needs care, especially when moving a widget subtree between OS windows.
