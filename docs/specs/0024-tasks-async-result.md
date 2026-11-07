# SPEC-0024: `tasks` module: `AsyncResult`, `AsyncPromise` and `ManualPromise`

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `coil::tasks` (new; already in `tools/layering.json`); `coil::foundation` (`ErrorCode::Cancelled`); a `tsan` build preset |
| Epic | [EPIC-009](../epics/EPIC-009-commands-and-state.md) (spec 1) |
| Related ADRs | ADR-0014 (async command handling), ADR-0010 (threading), ADR-0013 (errors), ADR-0002 (sanitizers) |
| Depends on | SPEC-0002 (`coil::Error`) |

> **Scope Gate checklist**
> - [x] One concern: a value that arrives later (or fails, or is cancelled), safely completed from any thread
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 modules (`tasks`, `foundation`); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
ADR-0014 makes every slow operation asynchronous: command handlers, remote calls, and frame capture
(ADR-0017), which is the next spec on the road to the button. They all return an `AsyncResult<T>`,
completed by an `AsyncPromise<T>` held by whoever does the work, possibly on another thread. This
spec creates that pair and its test fake. Continuations (`then`, always on the UI thread) need the
`UiDispatcher` and come in the next spec (§9 Q1).

## 2. Scope
- New module `modules/tasks`: `AsyncResult<T>`, `AsyncPromise<T>`, `makeAsync<T>()`, for any
  move-constructible `T` and for `void`.
- Test support `modules/tasks/testing`: `ManualPromise<T>` (§9 Q3).
- `foundation`: `ErrorCode::Cancelled` (§9 Q4).
- Build: a `tsan` configure/build/test preset; the threading test runs under it (§9 Q5).

## 3. Out of Scope
- `then` and other continuations; `UiDispatcher` (EPIC-009 spec 2, §9 Q1).
- Coroutines (`Task<T>`, `co_await`), `TaskScheduler`, thread pools.
- Progress reporting, timeouts, per-command concurrency policies (with `CommandRouter`).
- Blocking waits (`get()`, `wait()`): the UI thread never blocks (ADR-0010).
- Adding the TSan preset to CI (EPIC-001's CI matrix spec).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/tasks/include/coil/tasks/async_result.hpp
namespace coil::tasks {

template <typename T> class AsyncResult;
template <typename T> class AsyncPromise;

/// A connected promise/result pair. The producer keeps the promise; the consumer keeps the result.
template <typename T> struct AsyncPair { AsyncPromise<T> promise; AsyncResult<T> result; };
template <typename T> [[nodiscard]] AsyncPair<T> makeAsync();

/// The consumer side: a value (or error) that arrives later. Move-only, single consumer.
template <typename T>
class AsyncResult {
public:
    /// Already completed (ADR-0014: synchronous handlers return ready results).
    [[nodiscard]] static AsyncResult ready(T value);      // AsyncResult<void>::ready() takes nothing
    [[nodiscard]] static AsyncResult failed(Error error);

    AsyncResult(AsyncResult&&) noexcept; AsyncResult& operator=(AsyncResult&&) noexcept;
    ~AsyncResult();                                        // rule 7

    /// Whether the outcome is known (completed, failed, cancelled or abandoned).
    [[nodiscard]] bool isReady() const;
    /// Moves the outcome out, once. nullopt while not ready, and after it was taken.
    [[nodiscard]] std::optional<Expected<T>> take();
    /// Asks the producer to stop. If the outcome is not known yet, it becomes Cancelled now.
    void cancel();
};

/// The producer side. Move-only; may be completed from any thread.
template <typename T>
class AsyncPromise {
public:
    AsyncPromise(AsyncPromise&&) noexcept; AsyncPromise& operator=(AsyncPromise&&) noexcept;
    ~AsyncPromise();                                       // rule 6

    /// Sets the outcome if it is not known yet. Returns whether this call set it.
    [[nodiscard]] bool complete(T value);                  // complete() for void
    [[nodiscard]] bool fail(Error error);
    /// Signalled when the consumer cancels or drops the result.
    [[nodiscard]] std::stop_token stopToken() const;
};

} // namespace coil::tasks
```
```cpp
// modules/tasks/testing/include/coil/tasks/testing/manual_promise.hpp
namespace coil::tasks::testing {

/// Test double for a producer (ADR-0014): the test holds it and decides when and how the
/// operation ends. Hands out its result once.
template <typename T>
class ManualPromise {
public:
    ManualPromise();
    [[nodiscard]] AsyncResult<T> takeResult();     // precondition: called once
    [[nodiscard]] bool complete(T value);          // complete() for void
    [[nodiscard]] bool fail(Error error);
    [[nodiscard]] bool cancelRequested() const;    // the consumer cancelled or dropped the result
    [[nodiscard]] bool settled() const;            // the outcome is known
};

} // namespace coil::tasks::testing
```
```cpp
// modules/foundation/include/coil/foundation/error.hpp (addition)
    Cancelled = 5, ///< The operation was cancelled before it finished.
```

### 4.2 Behavioral rules
1. `makeAsync<T>()` returns a connected pair whose result is not ready.
2. `ready(value)` and `failed(error)` return results that are ready, with no promise.
3. **First outcome wins.** The first of `complete`, `fail`, `cancel` (rule 5) and abandonment (rule 6)
   sets the outcome; later `complete`/`fail` calls return `false` and change nothing.
4. `take()` returns the outcome once: `nullopt` while not ready, the outcome on the first call after,
   `nullopt` after that. `isReady()` stays `true` after `take()`.
5. **Cancel:** `cancel()` requests stop on the promise's `stopToken()`. If the outcome was not known,
   it becomes `Error{Cancelled}`. If it was already known, it is kept, and stop is not requested.
6. **Abandoned promise:** destroying (or moving over) a promise that has not set the outcome makes it
   `Error{Internal, "promise abandoned"}` (§9 Q6).
7. **Dropped result:** destroying (or moving over) a result whose outcome is not known requests stop
   on the promise, as `cancel()` does (§9 Q7).
8. **Threads:** `complete`, `fail`, the promise's destructor and `stopToken()` may be called from any
   thread, concurrently with the result's members on another thread. A result is used by one thread
   at a time (single consumer). No call blocks for longer than a short internal lock.
9. A moved-from `AsyncResult` or `AsyncPromise` may only be destroyed or assigned to; any other call
   is a precondition violation (`COIL_EXPECTS`, which this spec introduces in `foundation` if it does
   not exist yet, §9 Q8).
10. `ManualPromise` behaves as an `AsyncPromise` and reports `cancelRequested()` (rule 5 or 7) and
    `settled()` (rule 3).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Second completion | `false`, outcome unchanged |
| `cancel()` after completion | No effect; the value is kept |
| `cancel()` twice | Second call has no effect |
| Promise dropped unfinished | `Internal`, "promise abandoned" |
| Result dropped unfinished | Stop requested |
| `take()` twice | Second returns `nullopt` |
| `T` without a default constructor | Supported (only move construction is required) |

### 4.4 Threading and lifetime
Shared state between the pair, released when both sides are gone. Rule 8 is proven under TSan.

### 4.5 Async behavior
This spec is the async primitive; its states are completed, failed, cancelled, abandoned.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1** — `ready(42)` is ready and `take()` gives 42, then `nullopt`; `failed(Error{NotFound})`
  gives `NotFound`; `AsyncResult<void>::ready()` gives success; a move-only `T`
  (`std::unique_ptr<int>`) and a type without a default constructor work.
- **AC-2** — From `makeAsync<int>()`, the result is not ready and `take()` is `nullopt`;
  `complete(7)` returns `true`; the result is then ready and gives 7. `fail(Error{NotFound})` on a
  fresh pair gives `NotFound`. The same for `void`.
- **AC-3** — After `complete(1)`, `complete(2)` and `fail(…)` return `false` and the result still
  gives 1.
- **AC-4** — `cancel()` on a pending result: the promise's `stopToken()` reports stop requested, the
  result gives `Cancelled`, and a later `complete` returns `false`. `cancel()` after `complete(5)`:
  the result gives 5 and stop is not requested.
- **AC-5** — Destroying an unfinished promise makes the result give `Internal`; destroying an
  unfinished result requests stop; destroying either side after completion does neither. Move
  assignment over a live side behaves like destroying it.
- **AC-6** — `complete` called on another `std::thread` (joined before checking) is seen by the
  result; 1000 pairs completed on a second thread while the first thread polls `isReady()` all give
  their value. Clean under the `tsan` preset.
- **AC-7** — `ManualPromise`: `takeResult()` gives a connected result; `cancelRequested()` becomes
  `true` after `cancel()` or dropping the result; `settled()` becomes `true` after `complete`, `fail`
  or `cancel`.
- **AC-8** — `toString(ErrorCode::Cancelled)` is `"Cancelled"`, and `Error{Cancelled}` formats as
  `"Cancelled"`.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `AsyncResult.AC1_ReadyAndFailedResults` | unit | |
| AC-2 | `AsyncResult.AC2_PromiseCompletesResult` | unit | |
| AC-3 | `AsyncResult.AC3_FirstOutcomeWins` | unit | |
| AC-4 | `AsyncResult.AC4_CancelRequestsStop` | unit | |
| AC-5 | `AsyncResult.AC5_DroppedSides` | unit | |
| AC-6 | `AsyncResult.AC6_CompletesFromAnotherThread` | unit | Real `std::thread`, joined; no sleeps |
| AC-7 | `ManualPromise.AC7_ObservesTheConsumer` | unit | |
| AC-8 | `Error.AC8_CancelledCode` | unit | In `foundation`'s tests |

AC-6 uses a real thread because rule 8 is about threads; it never sleeps or waits on time: the
thread is joined, and the polling loop is bounded by the 1000 completions, not by a clock.

## 7. Non-Functional Requirements
- Header-only templates; one small source file for shared non-template code.
- No allocation besides the shared state (one per pair).

## 8. Architectural Impact
- New module `tasks` (planned in OVERVIEW §3 and `tools/layering.json`: depends on `foundation`). No
  new edge. `render → tasks` for capture is a separate ADR-0009 amendment, before the capture spec.
- New public enumerator `ErrorCode::Cancelled`.
- New `tsan` preset in `CMakePresets.json` (ThreadSanitizer, like `asan`).

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — Where `then` goes.** ADR-0014 says continuations always run on the UI thread through
  `UiDispatcher`, which is EPIC-009 spec 2. *Recommended:* `then` moves into spec 2 (renamed
  "`UiDispatcher`, `ManualUiDispatcher` and `then` continuations"). This spec has polling only.
- **Q2 — How a pair is created.** *Recommended:* `makeAsync<T>()` returning `{promise, result}`, so
  neither side can exist without the other.
- **Q3 — What `ManualPromise` adds.** *Recommended:* a test double that wraps a promise and exposes
  `cancelRequested()` and `settled()`, so tests can check that the consumer cancelled or that work
  ended, without production code exposing those observers.
- **Q4 — How cancellation is reported.** *Recommended:* a new `ErrorCode::Cancelled`. Reusing
  `Unavailable` would make "cancelled by the user" indistinguishable from "service down".
- **Q5 — TSan.** CLAUDE.md §6 requires TSan for threaded code, and no preset exists. *Recommended:*
  add a `tsan` preset now and run the full suite under it in the Refactor step; CI picks it up with
  EPIC-001's CI matrix.
- **Q6 — An abandoned promise.** *Recommended:* `Internal` with the message "promise abandoned": a
  producer that never answers is a bug, and the consumer must not wait forever.
- **Q7 — A dropped result.** *Recommended:* it requests stop, since nobody is waiting for the work.
- **Q8 — `COIL_EXPECTS`.** CLAUDE.md names it, but it has not been needed yet. *Recommended:* if
  `foundation` has no assertion macro, this spec adds a minimal `COIL_EXPECTS(cond)` that aborts with
  file and line in all builds, and uses it for rule 9 (not tested: a death test would fork).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0024): …`):
   - **Stubs:** the module and headers as in §4.1; the shared state exists but `complete`/`fail`
     return `false` without storing, `isReady()` is `false`, `take()` is `nullopt`, `cancel()` and the
     destructors do nothing; `ready`/`failed` return results that are not ready. `ManualPromise`
     forwards; its observers return `false`. `ErrorCode::Cancelled` declared, `toString` returns
     `"Unknown"` for it.
   - **Tests:** AC-1 to AC-8. Lint the test code.
   - **Expected Red:** all eight fail on assertions; the existing 211 pass.
2. **Green:** shared state with a mutex, `std::optional<Expected<T>>` and a `std::stop_source`;
   rules 1–10; `toString`.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; `ctest --preset tsan` (full suite).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/tasks/{CMakeLists.txt, include/coil/tasks/async_result.hpp, src/async_result.cpp}` | create | Module |
| `modules/tasks/testing/include/coil/tasks/testing/manual_promise.hpp` | create | Fake |
| `modules/tasks/tests/{CMakeLists.txt, .clang-tidy, async_result_test.cpp, manual_promise_test.cpp}` | create | AC-1–AC-7 |
| `modules/foundation/{include/coil/foundation/error.hpp, src/error.cpp, tests/error_test.cpp}` | modify | `Cancelled`, AC-8 |
| `modules/foundation/include/coil/foundation/assert.hpp` | create (if needed) | `COIL_EXPECTS` (Q8) |
| `CMakeLists.txt`, `CMakePresets.json` | modify | Module; `tsan` preset |
| `docs/epics/EPIC-009-commands-and-state.md` | modify | Spec table; spec 2 renamed (Q1) |

### 10.3 Risks / alternatives considered
- **`std::future`/`std::promise`:** no cancellation, `get()` blocks, and continuations can't be routed
  to the UI thread. Rejected by ADR-0014's requirements.
- **Lock-free state:** not needed at UI rates; a mutex is simpler to prove under TSan.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes on `dev`, `asan` and `tsan`
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `AsyncResult.AC1_ReadyAndFailedResults`
- [ ] AC-2 → `AsyncResult.AC2_PromiseCompletesResult`
- [ ] AC-3 → `AsyncResult.AC3_FirstOutcomeWins`
- [ ] AC-4 → `AsyncResult.AC4_CancelRequestsStop`
- [ ] AC-5 → `AsyncResult.AC5_DroppedSides`
- [ ] AC-6 → `AsyncResult.AC6_CompletesFromAnotherThread`
- [ ] AC-7 → `ManualPromise.AC7_ObservesTheConsumer`
- [ ] AC-8 → `Error.AC8_CancelledCode`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q8 approved as recommended | Emilio Pomares |
