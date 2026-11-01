# CLAUDE.md — Coil

Coil is a cross-platform C++ UI library (retained-mode, in the spirit of Unreal Slate / Qt) designed
to be the foundation of a game engine editor. It must be extensible via plugins, input-device
agnostic, renderer agnostic, and usable either in-process or as a remote frontend for another program.

This file holds the **non-negotiable rules**. The architecture is in
[docs/architecture/OVERVIEW.md](docs/architecture/OVERVIEW.md) and the decisions behind it are in
[docs/adr/](docs/adr/). If a rule here conflicts with a request, stop and raise the conflict with the
user. Do not quietly work around it.

---

## 1. Workflow: Spec → Plan → Red → Green → Refactor

Every change to production code goes through these phases in order. Do not skip or merge phases.

| # | Phase | Output | Gate |
|---|-------|--------|------|
| 0 | **Intake** | Request checked against the Scope Gate (§2) | Request is scoped, or user accepts a proposed decomposition |
| 1 | **Spec** | `docs/specs/NNNN-slug.md` from [TEMPLATE](docs/specs/TEMPLATE.md), linked to its epic | **User approves the spec** (Status → `Approved`) |
| 2 | **Plan** | "Implementation Plan" section of the spec filled in | **User approves the plan** |
| 3 | **Red** | Tests for every acceptance criterion + compile-only stubs; tests run and **fail on assertions** | Failing output shown to user; committed as `test(NNNN): …` |
| 4 | **Green** | Minimal production code that makes the tests pass. **Test files are frozen** (§3) | All tests pass, including the full suite |
| 5 | **Refactor** | Cleanup with all tests green, lint clean, format clean | `tools/check.sh` passes |
| 6 | **Close** | Each AC ticked with the test that proves it; spec Status → `Done` | User acknowledges |

Rules:
- **Never write production code before phases 1 and 2 are approved.** "Approved" means the user said so
  explicitly in this conversation or the spec status was set to `Approved` by the user.
- If a spec turns out to be wrong during implementation, **stop**, explain why, propose an amendment
  to the spec, and wait for approval. Do not implement off-spec behavior.
- Trivial non-behavioral changes (typos, comments, docs, formatting) may skip phases 1–4, but must
  still pass `tools/check.sh`.
- Architectural changes (new module, new dependency, change in layering, ABI or threading model)
  require a new or amended ADR in `docs/adr/` approved by the user **before** the spec.
- **Dependency edges** (`tools/layering.json`): an edge that does not change layer order (its target is
  already reachable and it creates no cycle) needs only user approval in the spec that adds it. An
  edge that creates a new path through the architecture needs an ADR.

## 2. Scope Gate: refuse vibe-coding requests

Do **not** start work on a request that is not tightly bounded. Requests like *"add OpenGL support"*,
*"make docking work"* or *"add a text editor widget"* are **epics**, not work items.

A request passes the gate only if **all** of these hold:
1. **One concern.** It describes one behavior or capability that can be explained in two sentences.
2. **Observable outcome.** Success can be checked by an automated test (unit or integration).
3. **Acceptance criteria.** There are concrete, numbered criteria (Given/When/Then), ideally ≤ 8.
   More than 8 is a signal to split.
4. **Bounded footprint.** It touches at most 2 modules (see layering) and adds no unapproved dependency.
5. **Explicit non-goals.** What is *not* included is stated.
6. **No open questions.** Every ambiguity in the template's "Open Questions" section is resolved.

When a request fails the gate:
- Say which criteria fail, briefly.
- Propose a **decomposition** into 3–7 candidate specs, each one passing the gate, ordered by dependency.
- Record it as an **epic** in `docs/epics/EPIC-NNN-slug.md` from [TEMPLATE](docs/epics/TEMPLATE.md),
  with status `Proposed`. The user approves the epic (goal, done-when, spec list) before any of its
  specs is drafted.
- Offer to draft the first spec from [docs/specs/TEMPLATE.md](docs/specs/TEMPLATE.md) and ask the user
  the specific questions needed to fill it in.
- Do not "just start with something reasonable." Agreeing on scope is part of the work.

### Epics, specs and numbering
- **Epic** = a goal broken into specs; it has no code of its own and is `Done` when all its specs are.
  **Spec** = one work item. **ADR** = a decision constraining all work.
- Every spec names its epic (or `—` for a standalone item). When a spec changes status, update the
  epic's spec table in the same change.
- A spec gets its number **only when its file is created** (next free number). Planned specs are
  referred to by name, never by a guessed number. Adding, removing or reordering specs in an approved
  epic needs user approval.

## 3. TDD integrity: no test trickery

Tests exist to prove behavior. Making a test pass by any means other than correct production code
is a serious violation. Specifically, **forbidden**:

- Modifying, deleting, renaming or weakening any test during the Green phase. Tests committed in the
  Red phase are frozen. Verify with: `git diff <red-commit> HEAD -- ':(glob)**/tests/**'` → must be empty.
- Disabling or skipping tests (`DISABLED_`, `GTEST_SKIP()`, commenting out, `#if 0`, excluding from CMake).
- Loosening assertions: widening tolerances, `EXPECT_TRUE(true)`, replacing `EXPECT_EQ` with weaker
  checks, asserting only "no crash" where behavior was specified.
- Special-casing tests in production code: detecting test names or inputs, `#ifdef TESTING` branches,
  hard-coded expected values, global "test mode" flags.
- Mocking or faking **the unit under test**. Fakes are for *ports* (dependencies) only.
- Swallowing errors (`catch (...) {}`, ignoring `std::expected` errors) to avoid a failure.
- Adding `NOLINT`, relaxing `.clang-tidy`/warnings, or editing CI configuration to get green.

If you believe a test is genuinely wrong: **stop**, explain why with evidence, propose the corrected
test and the matching spec amendment, and wait for explicit user approval. Then the corrected test goes
through Red again: it must be shown failing first.

Tests must be meaningful:
- Each acceptance criterion maps to at least one named test; each test names the AC it covers
  (e.g. `TEST(Button, AC3_ActivateIntentEmitsCommand)`).
- The Red phase must show each new test **failing for the right reason** (an assertion failure, not a
  compile or link error). Use minimal stubs that compile and return neutral or wrong values.
- Test behavior through public APIs, not private state. Include edge and error cases from the spec.
- Tests are deterministic: no sleeps, no wall-clock time, no real OS windows, no GPU, no network in
  unit tests. Use the fake adapters (`FakeClock`, `FakeInputSource`, `RecordingRenderBackend`,
  `LoopbackTransport`, `ManualUiDispatcher`, `ManualPromise`, …). Async behavior (completion, failure,
  cancellation, timeout, re-entrancy) is tested by driving these fakes explicitly, never by waiting.
- Integration tests should drive the UI through `AutomationService` (find by id → perform → assert
  on the snapshot), the same way an agent would.

## 4. Architecture rules (summary; details in OVERVIEW.md)

- **Hexagonal / ports & adapters, layered.** Library modules in `modules/` are pure, portable C++.
  They **never** include OS headers (`windows.h`, Cocoa, X11, …) or graphics/windowing APIs (Vulkan,
  GL, Metal, SDL). Those appear **only** in `backends/` adapters that implement ports.
- **Dependencies point inward only.** A module includes only itself, its **direct** dependencies and
  `foundation` (rules in `tools/layering.json`, enforced by `tools/check_layering.py`). See §1 for
  when a new edge needs an ADR.
- **Never assume a device.** Widgets react to semantic **intents** (`Activate`, `Navigate`, `Scroll`,
  `Pointer*`, `TextInput`, `Cancel`, …), never to "mouse button left" or "key Enter". Device→intent
  mapping lives in the input mapping layer.
- **Commands out, state in.** Widgets never call application logic directly. They emit `Command`s
  through the `CommandRouter` and read application state through bound `Property` values. Handlers may
  be in-process or remote; widget code must not be able to tell the difference.
- **Async handlers.** Every command handler returns `AsyncResult<CommandResult>`. Continuations run on
  the UI thread, capture `WidgetHandle`s (never `this`), and honor cancellation (ADR-0014).
  `Widget::onIntent` stays synchronous and fast; slow work always goes through a command.
- **Every widget is automatable.** It must publish an `AutomationNode` (role, name, value, state,
  actions, stable `AutomationId`) (ADR-0015). A widget without semantic info is incomplete.
- **Agent access is opt-in and locked down.** The MCP server is off by default, loopback only,
  token-authenticated and tiered. Sensitive widgets are redacted and masked. Relaxing any of
  this needs an ADR (ADR-0016).
- **UI-thread affinity.** The widget tree is touched only on the UI thread. Other threads communicate
  via `UiDispatcher::post()`. Rendering consumes immutable `DrawList` snapshots.
- **Logical units.** Core works in device-independent units (DIPs). Pixel conversion happens only in
  backends.
- **Plugins are native C++, same toolchain** (ADR-0011). The host refuses any plugin whose build
  fingerprint differs. No exceptions across the plugin boundary; plugin-created objects are destroyed
  by the plugin's own deleters.
- **Third-party libraries stay where ADR-0018 puts them.** Never in public headers of `modules/`.

## 5. C++ rules

- **Standard:** C++23. Toolchains: AppleClang ≥ 21 (verified); Clang ≥ 18, GCC ≥ 14, MSVC ≥ 19.40
  (provisional). See ADR-0001 Amendment 1. **Do not use `std::move_only_function`.**
  No compiler extensions in `modules/` (`CMAKE_CXX_EXTENSIONS OFF`).
- **Ownership:** RAII everywhere. No naked `new`/`delete`. `std::unique_ptr` for ownership,
  `std::shared_ptr` only when justified in review. Long-lived references to widgets use `WidgetHandle`
  (generational), never raw pointers stored beyond a call.
- **Errors:** recoverable errors → `std::expected<T, coil::Error>`. Programmer errors (violated
  preconditions) → `COIL_EXPECTS` / `COIL_ASSERT`. Coil code does not throw; nothing throws across a
  public API or plugin boundary. `[[nodiscard]]` on every function returning a value or error.
- **Interfaces:** ports are abstract classes named after the role (`RenderBackend`, `InputSource`,
  `Transport`) with a virtual destructor, no data members, and copy/move deleted. Prefer composition
  to inheritance outside ports and the widget hierarchy.
- **Headers:** public headers in `modules/<m>/include/coil/<m>/`, private in `modules/<m>/src/`.
  `#pragma once`. Include what you use. No `using namespace` in headers.
- **Naming:** files `snake_case.hpp/.cpp`; namespaces `coil::lower_case`; types/enums/concepts
  `PascalCase`; functions, methods and variables `camelCase`; private/protected members `m_camelCase`;
  constants and enumerators `kPascalCase`/`PascalCase`; macros `COIL_UPPER_CASE`. Enforced by clang-tidy.
- **No globals** with mutable state, no singletons. Dependencies are injected (constructor parameters
  or a `Context`), so everything can be tested with fakes.

## 6. Quality gates (must pass before any phase is declared complete)

- **Format:** `clang-format` ([.clang-format](.clang-format)). No diffs allowed.
- **Lint:** `clang-tidy` ([.clang-tidy](.clang-tidy)) with warnings as errors, LLVM 23 (ADR-0001). `NOLINT` only with a
  specific check name plus a justification comment, and only with user approval.
- **Warnings:** `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` / `/W4 /WX /permissive-`.
- **Sanitizers:** ASan+UBSan preset, and TSan for threaded code, on Linux/macOS CI.
- **Tests:** full suite green (`ctest --preset <preset>`), not just the new tests.
- **CI matrix:** Windows (x64, arm64), macOS (arm64, x64), Linux (x64, arm64) × Debug/Release.
- Single local entry point: `tools/check.sh` (format check, lint, build, test).

## 7. Commands

Prerequisites: see ADR-0001 (toolchains, LLVM 23) and `VCPKG_ROOT` pointing to a vcpkg clone.
- Configure + build: `cmake --preset dev && cmake --build --preset dev` (also `release`, `asan`)
- Test: `ctest --preset dev` (single test: `ctest --preset dev -R <regex>`)
- All gates (format, layering, configure, build, tidy, tests): `tools/check.sh`
- Single gate: `tools/check.sh format [files…]`, `tools/check.sh layering`, `tools/check.sh tidy [files…]`
- Layering checker directly: `python3 tools/check_layering.py [--root DIR] [--config FILE]` (Python ≥ 3.9)
- Tool overrides: `CLANG_FORMAT=…`, `CLANG_TIDY=…`, `COIL_PRESET=…`

## 8. Repository layout

```
modules/<name>/{include/coil/<name>/, src/, tests/}   # portable library modules
backends/<name>/{src/, tests/}                         # adapters: OS, windowing, GPU APIs, sockets
examples/                                              # sample apps (not part of the library)
tools/                                                 # check.sh, check_layering.py, scripts
docs/architecture/  docs/adr/  docs/epics/  docs/specs/
```

## 9. Working agreements for Claude

- Plan before acting; show the plan; wait for approval (§1).
- Keep diffs minimal and on-spec. Do not refactor unrelated code or add "nice to have" extras.
- When you are unsure what the user means, ask. Don't guess at behavior a spec should define.
- Report results honestly: show real test and lint output. Never claim something passes without
  having run it.
- Commit only when the user asks. Commit messages: `spec(NNNN)`, `test(NNNN)`, `feat(NNNN)`,
  `refactor(NNNN)`, `docs`, `build`.
