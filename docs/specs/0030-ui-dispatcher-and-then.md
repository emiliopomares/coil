# SPEC-0030: `UiDispatcher`, `ManualUiDispatcher` and `then` continuations

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-08 |
| Module(s) | `coil::tasks` (and its test-support library) |
| Epic | [EPIC-009](../epics/EPIC-009-commands-and-state.md) (spec 2) |
| Related ADRs | ADR-0010 (threading model), ADR-0014 (async command handling), ADR-0013 (errors) |
| Depends on | SPEC-0024 (`AsyncResult`, `AsyncPromise`, `ManualPromise`) |

> **Scope Gate checklist**
> - [x] One concern: work reaches the UI thread only through a `UiDispatcher`, and an `AsyncResult`'s continuation runs there once its outcome is known, whichever thread produced it
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`tasks`); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
ADR-0014 requires that an operation's continuation always runs on the UI thread, never inline on the
thread that completed it, and ADR-0010 makes `UiDispatcher::post` the only thread-safe way into UI
state. SPEC-0024 gave `AsyncResult` polling only. This spec adds the dispatcher port, the
`ManualUiDispatcher` fake that tests drain on demand, and `then`, so a command handler finishing on a
worker thread can safely update the UI. EPIC-009 specs 3–4 (the `CommandRouter` and widget-safe
continuations) build on it.

## 2. Scope
- `UiTask`: a move-only, type-erased `void()` callable.
- `UiDispatcher` port with `post(UiTask)`.
- `testing::ManualUiDispatcher`: a thread-safe fake with `pending()` and `runPending()`.
- `AsyncResult<T>::then(UiDispatcher&, F) &&`, returning a chained `AsyncResult`, with cancellation
  propagated back to the source.
- Tests for every rule, including completion from another thread (run under TSan).

## 3. Out of Scope
- **The production dispatcher** that wakes a blocked event loop (`waitEvents`) when another thread
  posts. That needs a platform wake-up and belongs with the frame scheduler and UI host (EPIC-003
  spec 2, EPIC-013) (Q2).
- `WidgetHandle` capture and dropping continuations whose widget is gone (EPIC-009 spec 4).
- `UiContext` shutdown and debug thread-affinity assertions (EPIC-013).
- Coroutines (`Task<T>`), timeouts, `TaskScheduler` and thread pools.
- Continuations on a result held by reference; `then` without a dispatcher.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/tasks/include/coil/tasks/ui_dispatcher.hpp
namespace coil::tasks {

/// A move-only, type-erased void() callable (std::move_only_function is not allowed, CLAUDE.md §5).
class UiTask {
public:
    UiTask() noexcept = default; ///< Empty.

    /// Takes ownership of `callable` (which may be move-only).
    template <typename F>
        requires(!std::same_as<std::remove_cvref_t<F>, UiTask> && std::invocable<std::decay_t<F>&>)
    explicit UiTask(F&& callable); // callers write post(UiTask{[] { ... }}) (Amendment A)

    UiTask(UiTask&&) noexcept;
    UiTask& operator=(UiTask&&) noexcept;
    UiTask(const UiTask&) = delete;
    UiTask& operator=(const UiTask&) = delete;
    ~UiTask();

    [[nodiscard]] explicit operator bool() const noexcept; ///< Whether it holds a callable.
    void operator()();                                     ///< Precondition: not empty.
};

/// Port: runs tasks on the UI thread (ADR-0010).
class UiDispatcher {
public:
    virtual ~UiDispatcher() = 0;
    UiDispatcher(const UiDispatcher&) = delete;
    UiDispatcher& operator=(const UiDispatcher&) = delete;
    UiDispatcher(UiDispatcher&&) = delete;
    UiDispatcher& operator=(UiDispatcher&&) = delete;

    /// Queues `task` to run later on the UI thread. Callable from any thread; never runs `task`
    /// before returning. Precondition: `task` is not empty.
    virtual void post(UiTask task) = 0;

protected:
    UiDispatcher() = default;
};

} // namespace coil::tasks

// modules/tasks/include/coil/tasks/async_result.hpp (added to AsyncResult<T>)
/// Runs `continuation(Expected<T>)` on `ui` once this result's outcome is known, and returns a
/// result for what it returns (rules 4–7). Consumes this result.
template <typename F>
    requires std::invocable<F&, Expected<T>>
[[nodiscard]] AsyncResult<ThenValue<F, T>> then(UiDispatcher& ui, F continuation) &&;
// ThenValue<F, T>: U if the continuation returns Expected<U>; otherwise its return type R
// (R may be void).

// modules/tasks/testing/include/coil/tasks/testing/manual_ui_dispatcher.hpp
namespace coil::tasks::testing {

/// A UiDispatcher that tests drain on demand. post() and pending() are thread-safe.
class ManualUiDispatcher final : public UiDispatcher {
public:
    void post(UiTask task) override;
    [[nodiscard]] std::size_t pending() const;  ///< Tasks queued and not yet run.
    /// Runs, in order and on the calling thread, the tasks queued when the call starts; tasks they
    /// post wait for the next call. Returns how many ran.
    [[nodiscard]] std::size_t runPending();
    ~ManualUiDispatcher() override; ///< Destroys pending tasks without running them.
};

} // namespace coil::tasks::testing
```

### 4.2 Behavioral rules
1. **Post.** `post` MUST NOT run the task before returning. Tasks MUST run in the order their `post`
   calls completed, each exactly once.
2. **Draining (fake).** `runPending` MUST run only the tasks queued when it starts; tasks posted while
   it runs (by those tasks or by other threads) MUST wait for the next call (Q6). `post` and
   `pending` MAY be called from any thread concurrently with each other and with `runPending`.
3. **Dropping (fake).** Destroying a `ManualUiDispatcher` MUST destroy its pending tasks, and what they
   captured, without running them (ADR-0014: pending continuations are dropped).
4. **`then`.** `std::move(result).then(ui, f)` MUST leave `result` moved-from and return a chained
   result that is not ready. Nothing is posted while the source's outcome is unknown.
5. **Delivery.** When the source's outcome becomes known (completed, failed or abandoned, on any
   thread), or at the `then` call if it is already known, exactly one task MUST be posted to `ui`,
   from the thread that made it known (or the calling thread). Running that task MUST call
   `f` exactly once with the outcome, on the thread running the task. `f` is never called inline.
6. **Chained outcome.** After `f` returns, the chained result MUST hold: `f`'s value if it returns
   `R` (non-void); success if it returns `void`; or exactly `f`'s `Expected<U>` (value or error).
7. **Cancellation.** If the chained result is cancelled (`cancel()`) or destroyed before `f` runs, the
   source MUST be asked to stop (its promise's `stopToken()` is signalled), and `f` MUST NOT be called,
   even if the source later completes and the task runs. A cancelled chained result holds
   `Cancelled` (SPEC-0024 rule 5). After `f` has run, cancelling changes nothing.
8. **Dropped task.** If the posted task is destroyed without running (rule 3), `f` MUST NOT be called
   and the chained result MUST become the abandoned error (`Internal`, "promise abandoned",
   SPEC-0024 rule 6).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `then` on a moved-from result | Precondition violation (`COIL_EXPECTS`) |
| `post` of an empty `UiTask`; calling an empty `UiTask` | Precondition violation |
| `runPending` called from inside a task it is running | Precondition violation (Q6) |
| Source failed, cancelled by its producer, or abandoned | `f` receives the error; it decides (rule 5) |
| `f` returns `std::unexpected(e)` | Chained result fails with `e` (rule 6) |
| Dispatcher destroyed before the source settles | Not allowed: `ui` must outlive every `then` attached to it until its task has run or been dropped (documented precondition, not checked) |

### 4.4 Threading and lifetime
- `post`, and the delivery post of rule 5, run on whatever thread calls them or settles the source.
  The task, and therefore `f`, runs on the thread that drains the dispatcher (the UI thread).
- The posted task owns `f` and the chained promise. `f` and its captures are destroyed on the thread
  that runs or drops the task.
- **Allocation in `noexcept` paths:** an abandoned promise's destructor (rule 5) posts, which may
  allocate. An allocation failure there terminates, as for SPEC-0024's prebuilt error (§10.3).

### 4.5 Async behavior
- **Completion / failure:** rules 5–6. **Cancellation:** rule 7. **Abandonment:** rule 5 (source),
  rule 8 (task). **Re-entrancy:** a continuation may call `post` and `then`; their tasks run in the
  next `runPending` (rule 2). **Timeouts:** out of scope.
- No widgets yet, so no originating-widget handling (EPIC-009 spec 4).

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1** — A `UiTask` built from a move-only callable (capturing a `std::unique_ptr`) can be moved
  and run, and runs that callable; a default `UiTask` is empty (`false`), a non-empty one `true`;
  destroying an unrun `UiTask` destroys its captures.
- **AC-2** — On a `ManualUiDispatcher`, `post` does not run the task; `pending()` counts queued tasks;
  `runPending()` runs them in posting order and returns how many ran; a task that posts another
  makes that one wait for the next `runPending()`.
- **AC-3** — Destroying a `ManualUiDispatcher` with pending tasks destroys them (their captures are
  released) without running them.
- **AC-4** — For a pending result, `then` posts nothing; after the promise completes, exactly one task
  is pending; `runPending()` calls the continuation once with the value, and the chained result holds
  what the continuation returned. For an already ready result, the task is pending right after
  `then`, and the continuation has not run yet.
- **AC-5** — A failed, abandoned or producer-cancelled source reaches the continuation as its error;
  a continuation returning `std::unexpected(e)` fails the chained result with `e`; one returning
  `Expected<U>` with a value, or `void`, gives that value, or success of `AsyncResult<void>`.
- **AC-6** — Cancelling the chained result before the continuation runs signals the source promise's
  stop token (`ManualPromise::cancelRequested()`), leaves the chained result `Cancelled`, and the
  continuation is never called, even after the source completes and the dispatcher is drained.
  Destroying the chained result instead also signals the stop token. Cancelling after the
  continuation ran leaves the chained result unchanged.
- **AC-7** — Destroying the dispatcher while a continuation's task is pending never calls the
  continuation, destroys its captures, and fails the chained result with `Internal`.
- **AC-8** — A promise completed on a worker thread (joined before checking) leaves exactly one task
  pending; `runPending()` on the test thread calls the continuation on the test thread with the value.
  Tasks posted concurrently from 4 threads (100 each) all run exactly once. Both pass under TSan.

## 6. Test Plan
| AC | Test name | Type | Fakes used | Notes |
|----|-----------|------|------------|-------|
| AC-1 | `UiTask.AC1_HoldsMoveOnlyCallables` | unit | — | Capture-destruction counter via `std::shared_ptr` use count |
| AC-2 | `ManualUiDispatcher.AC2_RunsInOrderOnDemand` | unit | — | Includes a task that posts |
| AC-3 | `ManualUiDispatcher.AC3_DestructionDropsTasks` | unit | — | |
| AC-4 | `Then.AC4_ContinuationRunsOnDrain` | unit | `ManualPromise`, `ManualUiDispatcher` | Pending and ready sources |
| AC-5 | `Then.AC5_ErrorsAndReturnShapes` | unit | same | Typed over `int` and `void` sources |
| AC-6 | `Then.AC6_CancellationReachesTheSource` | unit | same | cancel, drop, cancel-after-run |
| AC-7 | `Then.AC7_DroppedTaskAbandonsTheChain` | unit | same | |
| AC-8 | `Then.AC8_CompletesFromAnotherThread` | unit | same + `std::thread` (joined) | As SPEC-0024 AC6; TSan preset |

Deterministic: worker threads are joined before any assertion; no sleeps or waits.

## 7. Non-Functional Requirements
- No locks held while running a task or a continuation, or while posting from `settle` (avoids
  deadlocks when a continuation re-enters).
- TSan clean (CLAUDE.md §6).

## 8. Architectural Impact
- Module: `tasks` only, plus `coil::tasks_testing`. No layering change.
- New public types: `UiTask`, `UiDispatcher`, `AsyncResult::then`; test type `ManualUiDispatcher`.
- `detail::SharedState<T>` gains a one-shot "outcome known" hook (internal; no public API change).
  Existing behavior from SPEC-0024 is unchanged and its tests stay green.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — The task type.** `std::function` needs copyable callables, but a continuation owns a move-only
  promise; `std::move_only_function` is banned. *Recommended:* our own small `UiTask` (move-only,
  `void()` only) in `tasks`. A general-purpose `UniqueFunction<Sig>` can come later if another
  module needs one.
- **Q2 — A production dispatcher now?** A real one must wake the UI thread out of `waitEvents`, which
  needs a platform wake-up (an SDL user event). *Recommended:* the port and the fake only; the real
  dispatcher arrives with the frame scheduler and UI host (EPIC-003 spec 2, EPIC-013), whose specs
  add the wake-up.
- **Q3 — Continuation signature.** *Recommended:* one callable taking `Expected<T>`, so success and
  failure can't be handled in two places that drift apart; `then` consumes the result (`&&`) and
  returns a chained `AsyncResult`.
- **Q4 — Flattening.** *Recommended:* a continuation returning `Expected<U>` gives `AsyncResult<U>`, not
  `AsyncResult<Expected<U>>`, so continuations can fail without nesting.
- **Q5 — Cancellation upstream.** *Recommended:* cancelling or dropping the chained result asks the
  source to stop (rule 7); nobody is left waiting for the source otherwise.
- **Q6 — Draining.** *Recommended:* `runPending` runs a snapshot (rule 2), so a task that reposts
  itself cannot loop forever inside one drain; calling it re-entrantly is a precondition violation.
- **Q7 — Dropped tasks.** *Recommended:* dropped without running, and the chained result abandoned
  (rules 3, 8), as ADR-0014 says pending continuations are dropped at shutdown.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0030): …`):
   - **Stubs:** `ui_dispatcher.hpp` with `UiTask` that stores its callable but whose `operator()`
     does nothing; `UiDispatcher`; `ManualUiDispatcher` whose `post` drops the task, `pending()`
     returns 0 and `runPending()` returns 0; `then` returns a fresh unready result and drops `f`.
   - **Tests:** `modules/tasks/tests/ui_dispatcher_test.cpp` (AC-1 to AC-3) and `then_test.cpp`
     (AC-4 to AC-8).
   - **Expected Red:** every AC fails on assertions. AC-3's and AC-7's "not run" parts hold
     against the stub; each has a positive control (a task that does run when drained, a
     continuation that does run when not dropped), so the tests only pass with a working
     implementation.
2. **Green:** `UiTask` (type erasure through a `std::unique_ptr` to an abstract holder);
   `ManualUiDispatcher` (mutex-guarded `std::deque`, snapshot swap in `runPending`, re-entrancy flag);
   a one-shot hook in `SharedState` invoked outside the lock by `settle` and `abandon`; `then` built
   on `makeAsync`, with the chained promise's stop token cancelling the source.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; `ctest --preset tsan`.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/tasks/include/coil/tasks/ui_dispatcher.hpp` | create | `UiTask`, `UiDispatcher` |
| `modules/tasks/src/ui_dispatcher.cpp` | create | Out-of-line destructor of the port |
| `modules/tasks/include/coil/tasks/async_result.hpp` | modify | `then`; the hook in `SharedState` |
| `modules/tasks/testing/include/coil/tasks/testing/manual_ui_dispatcher.hpp` | create | The fake |
| `modules/tasks/tests/{ui_dispatcher_test.cpp, then_test.cpp, CMakeLists.txt}` | create / modify | AC-1 to AC-8 |
| `modules/tasks/CMakeLists.txt` | modify | New source |
| `docs/epics/EPIC-009-commands-and-state.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Posting from `noexcept` paths.** `abandon()` runs in the promise's destructor and now posts. A
  throwing allocation there terminates. Alternative: pre-allocate the queue node at `then` time,
  which needs an intrusive queue in every dispatcher. Rejected for now; if clang-tidy's
  `bugprone-exception-escape` flags it, I stop and propose a fix rather than adding a `NOLINT`.
- **A lock-ordering bug** (the source's mutex held while posting, with a continuation re-entering)
  would deadlock. Rule: the hook runs after the lock is released. AC-8 under TSan checks it.
- **Alternative:** the continuation's dispatcher stored in the result at creation. Rejected:
  results are created by producers, which should not know the consumer's thread.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer presets pass (`asan`, `tsan`)
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)
- [ ] (Async) Completion, failure and cancellation each covered by a test

### AC verification
- [ ] AC-1 → `UiTask.AC1_HoldsMoveOnlyCallables`
- [ ] AC-2 → `ManualUiDispatcher.AC2_RunsInOrderOnDemand`
- [ ] AC-3 → `ManualUiDispatcher.AC3_DestructionDropsTasks`
- [ ] AC-4 → `Then.AC4_ContinuationRunsOnDrain`
- [ ] AC-5 → `Then.AC5_ErrorsAndReturnShapes`
- [ ] AC-6 → `Then.AC6_CancellationReachesTheSource`
- [ ] AC-7 → `Then.AC7_DroppedTaskAbandonsTheChain`
- [ ] AC-8 → `Then.AC8_CompletesFromAnotherThread`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-08 | Spec and implementation plan created (Draft) | |
| 2026-11-08 | Spec, plan and Q1–Q7 approved as recommended | Emilio Pomares |
| 2026-11-08 | Amendment A (before Red): `UiTask`'s constructor is `explicit`, not implicit, because clang-tidy (`cppcoreguidelines-explicit-constructor`) rejects implicit single-argument constructors; callers write `post(UiTask{…})` | Emilio Pomares |
| 2026-11-08 | Red: AC-1 to AC-8 fail on assertions (task never runs, nothing queued, chain never settles, source not cancelled); the "not run" and "cancel" checks of AC-3, AC-6 and AC-7 hold against the stub, but each test's positive control fails; the other 268 pass | — |
