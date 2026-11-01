# ADR-0010: Threading model
Status: Accepted (2026-11-01)

## Decision
- **UI-thread affinity.** The widget tree, `PropertyStore` notifications, `CommandRouter` dispatch and
  layout run on the single UI thread of a `UiContext`. Debug builds assert thread affinity on every
  public mutating call.
- **`UiDispatcher::post(task)`** is the only thread-safe way into UI state from other threads (MPSC
  queue, drained once per frame).
- **`TaskScheduler` port** for parallel CPU work (the app plugs in its job system; Coil supplies a
  default thread-pool adapter and an `InlineTaskScheduler` for tests). Results return through
  `UiDispatcher`.
- **Rendering.** Painting produces an immutable `DrawList` snapshot. Initially, submission happens on the
  UI thread. The snapshot design allows moving submission to a render thread later without API changes.
- I/O threads owned by adapters (remote transport, MCP server) never touch UI state. They marshal
  through `UiDispatcher` and receive answers via `AsyncResult` (ADR-0014).
- GPU and multi-core workloads integrate via `GpuSurface` (ADR-0009) and asynchronous commands
  (ADR-0008). The UI never waits on them.

## Consequences
+ Simple and race-free widget code. TSan only has to cover a small surface.
− Applications must marshal results back to the UI thread (made easy by `UiDispatcher`).
