# ADR-0015: Semantic automation tree (for agents, tests and accessibility)
Status: Accepted (2026-11-01)

## Context
AI agents (via MCP, ADR-0016), integration tests and screen readers all need the same thing: a
machine-readable description of what is on screen and a way to act on it, without pixel guessing.

## Decision
- **Every widget publishes semantic info. This is a mandatory part of the widget contract**, not an
  add-on: `AutomationNode{ AutomationId id; Role role; std::string name, description; Value value;
  StateFlags state (enabled, focused, selected, expanded, checked, busy, hidden, sensitive);
  Rect bounds (logical, window-relative); std::vector<ActionId> actions; }`.
- **`AutomationId`** is stable and human-meaningful (`"inspector/transform/position.x"`). It can be set
  explicitly; otherwise it is derived from type and path. It is unique per window and survives
  re-layout. Workspace and dock panels contribute their panel instance id as a path segment.
- **Roles** are a closed core set (button, slider, text field, label, list, tree, tab, panel, window,
  viewport, menu, menu item, …) plus namespaced custom roles from plugins.
- **Actions go through the normal input path.** `AutomationService::perform(id, action, args)`
  injects intents as a virtual input device (`DeviceKind::Automation`) at the target widget. Agent,
  test and human therefore exercise identical code. Direct property or command access is offered
  separately (ADR-0016) and labeled as such.
- **Snapshots, not live pointers.** The service produces immutable tree snapshots on the UI thread
  (full, subtree, or filtered by role, name or visibility) with a version counter, plus change events
  (`nodeAdded/Removed/Changed`) for subscribers.
- **Waiting is a service feature.** `waitFor(predicate, timeout) → AsyncResult` (for example "node X
  exists and is not busy"), because commands are async (ADR-0014). Polling with sleeps is never needed.
- **Sensitive data.** Widgets flagged `sensitive` (passwords, tokens) never expose their `value` and are
  masked in captures (ADR-0017).
- **Accessibility** platform bridges (UIA, NSAccessibility, AT-SPI, e.g. via AccessKit) are future
  adapters over this same tree.

## Consequences
+ One investment serves agents, end-to-end tests and accessibility.
+ Integration tests become readable: find by id, perform, then assert on the tree.
− Every widget spec must define its role, name, value and actions (enforced by the spec template's DoD).
