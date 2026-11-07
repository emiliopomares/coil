# EPIC-009: Commands and state

| Field | Value |
|-------|-------|
| Status | Approved |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01) |

## Goal
Widgets talk to the application only through **commands out, state in** (ADR-0008). A widget emits a
`Command` through the `CommandRouter`, and an in-process handler runs and completes asynchronously
(ADR-0014). Its continuation runs on the UI thread and honors cancellation. Widgets read application
state through a `PropertyStore` and are told when it changes.

## Done when
- The `tasks` module provides `AsyncResult<T>` / `AsyncPromise<T>`, a `ManualPromise` fake, and a
  `UiDispatcher` with a `ManualUiDispatcher` fake.
- A `CommandRouter` routes commands to registered in-process handlers. Unknown commands fail cleanly.
- Continuations capture `WidgetHandle`s (never `this`), run on the UI thread, and are dropped if the
  widget is gone or the command was cancelled.
- A minimal `PropertyStore` holds typed values by path and notifies bound widgets on change.
- All async behavior (completion, failure, cancellation, re-entrancy) is tested by driving the fakes,
  never by waiting.

## Out of scope
- Remote handlers and the wire protocol (`remote`), coroutines (`Task<T>`), thread pools, undo/redo,
  MCP.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0024 `tasks` module: `AsyncResult`, `AsyncPromise` and `ManualPromise`](../specs/0024-tasks-async-result.md) | Done | — |
| 2 | `UiDispatcher`, `ManualUiDispatcher` and `then` continuations on the UI thread | Planned | 1 |
| 3 | `Command` and `CommandRouter` with in-process handlers | Planned | 1; EPIC-008 spec 1 |
| 4 | UI-thread continuations with `WidgetHandle` capture and cancellation | Planned | 2, 3 |
| 5 | Minimal `PropertyStore` with change notification | Planned | EPIC-008 spec 1 |

## Notes
- ADR-0008, ADR-0010, ADR-0014. `tasks` is already in `tools/layering.json`.
- Specs 1 and 2 don't depend on the widget tree and could be done earlier, in parallel.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
| 2026-11-07 | Epic approved (roadmap to Milestone 2) | Emilio Pomares |
| 2026-11-07 | Spec 1 is SPEC-0024; `then` continuations move to spec 2, next to `UiDispatcher` (SPEC-0024 Q1) | Emilio Pomares |
