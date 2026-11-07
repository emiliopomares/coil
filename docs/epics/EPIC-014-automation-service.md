# EPIC-014: In-process automation service

| Field | Value |
|-------|-------|
| Status | Approved |
| Date | 2026-11-07 |
| Origin | Roadmap to Milestone 2 (2026-11-07). CLAUDE.md §3: integration tests drive the UI through `AutomationService` (find by id → perform → assert on the snapshot), the same way an agent would |

## Goal
Tests (and later, agents through MCP) can find a widget by its `AutomationId`, perform one of its
actions, and check the result on a fresh snapshot, with no access to widget internals. Milestone 2's
"Click me" test presses the button this way.

## Done when
- The `automation` module provides an `AutomationService` over a `UiWindow`'s tree: `snapshot()`,
  `find(id)` and `perform(id, action)` (ADR-0015).
- `perform` goes through the normal input path as intents from a virtual input device, never by
  calling widget methods (ADR-0015), so an automated press behaves exactly like a real one.
- Unknown ids, unsupported actions and disabled widgets fail with clear errors.

## Out of scope
- `waitFor` and change events (when async tests need them), the MCP server and transport (ADR-0016),
  redaction and masking of sensitive widgets (with MCP), platform accessibility bridges.

## Specs (in dependency order)
*Planned specs are listed by name. A spec gets its number only when its file is created.*

| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | `automation` module: `AutomationService` with `snapshot` and `find` by `AutomationId` | Planned | EPIC-008 spec 7; EPIC-013 spec 1 |
| 2 | `perform(id, action)` through a virtual input device; errors for unknown ids, actions and disabled widgets | Planned | 1; EPIC-013 spec 2 |

## Notes
- `automation` is already in `tools/layering.json` (depends on `core`).
- ADR-0015 (semantic automation tree), ADR-0016 (agent access stays opt-in; this epic adds no
  network access).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Epic proposed (roadmap to Milestone 2) | |
| 2026-11-07 | Epic approved (roadmap to Milestone 2) | Emilio Pomares |
