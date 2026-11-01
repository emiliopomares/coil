# ADR-0014: Asynchronous command handling
Status: Accepted (2026-11-01)

## Context
Command handlers routinely do slow work: an HTTPS request, a disk scan, a GPU bake, a round trip to a
remote process. The UI thread must never block, and completions must update the UI safely, even when
the widget that started the work no longer exists.

## Decision
- **Every command handler is asynchronous by contract.** Signature:
  `AsyncResult<CommandResult> handle(const Command&, CommandContext&)`. A synchronous handler is
  adapted with `AsyncResult<T>::ready(value)`, so there is a single code path for both.
- **`AsyncResult<T>` / `AsyncPromise<T>`** (foundation/tasks):
  - The promise may be completed from **any thread**. Continuations (`.then(...)`) **always run on the
    UI thread** via `UiDispatcher`, never inline on the completing thread.
  - Move-only and single-consumer. Results are `std::expected<T, Error>`, so failures are values.
  - **Awaitable:** C++20 coroutines can `co_await` an `AsyncResult` (`coil::Task<T>` coroutine type),
    so handler code can read top to bottom. Callback style remains available.
  - **Cancellation** through `std::stop_token`. `CommandContext` provides the token, a progress
    reporter (`report(0.0..1.0, message)`) and the originating `WidgetHandle` (possibly stale).
- **Lifetime safety.** Continuations capture `WidgetHandle`s, never `this` or raw widget pointers. When a
  `UiContext` shuts down, it cancels outstanding work and drops pending continuations.
- **Per-command concurrency policy** (command metadata): `Allow` | `IgnoreWhileRunning` (default) |
  `CancelPrevious` | `Queue`. Optional timeout per command.
- **Observable execution state.** The router publishes `commands.<id>.running`, `.progress` and
  `.lastError` into the `PropertyStore`, so buttons can show busy or disabled state and spinners by
  binding, with no widget-specific code. The same state is visible to automation and MCP (ADR-0015, ADR-0016).
- **Widget event handling stays synchronous.** `Widget::onIntent` must be fast and return a `Reply`. Any
  slow work goes through a command. A remote handler (ADR-0008) is just another async handler.
- **Plugin ABI** (ADR-0011): async handlers are expressed as `start(…, completionCallback, userData)`
  returning an operation handle, plus a `cancel(handle)` entry point.
- **Testing.** `ManualUiDispatcher` (drain on demand) and `ManualPromise` let tests complete, fail,
  cancel or time out operations deterministically, with no sleeps and no real threads.

## Consequences
+ Long-running and networked actions are first-class, cancellable, and visible to UI and agents.
− Slightly more ceremony for trivial handlers, mitigated by the `ready()` adaptation.
− Coroutine debugging is harder. The callback form is always available.
