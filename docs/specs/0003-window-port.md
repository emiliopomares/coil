# SPEC-0003: Window port with in-memory fake and reusable contract tests

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::platform` (new), including its test-support library `coil::platform_testing` |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0003 (fakes, contract tests), ADR-0004 (ports & adapters), ADR-0006 (logical units), ADR-0013 (errors) |
| Depends on | SPEC-0002 |

> **Scope Gate checklist**
> - [x] One concern: the abstract interface for creating, querying and destroying windows
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules (`platform`, using `foundation`); no new dependency or layering edge
>   (`platform → foundation` is already allowed by OVERVIEW §3)
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
Coil must never talk to an OS windowing API directly from library code (ADR-0004). It needs a
**port**: an interface Coil owns, which real adapters (SDL3 next, native backends later) implement.
This spec defines that interface for the smallest useful behavior (window lifetime, title and size),
plus an in-memory fake for tests and a **contract test suite** that every implementation must pass.
The SDL3 adapter spec will reuse the same suite unchanged.

## 2. Scope
- New module `modules/platform` with the `WindowSystem` port and its value types (`WindowId`,
  `WindowSize`, `WindowDesc`).
- Test-support library `coil::platform_testing` with `FakeWindowSystem`, an in-memory implementation
  (ADR-0003: `modules/<m>/testing/`).
- A reusable GoogleTest **typed contract suite** `WindowSystemContract<T>` and its instantiation for
  `FakeWindowSystem`.

## 3. Out of Scope
- Real OS windows and any third-party library (SDL3 adapter spec).
- Events, including close requests (window events spec).
- Position, visibility, fullscreen, minimize/maximize, icons, resizing *after* creation, DPI or
  pixel scale.
- Native window handles and graphics-API hints for render backends (a rendering epic). The port is
  designed so they can be added later without breaking changes.
- Thread-affinity enforcement (needs `COIL_ASSERT`, a later spec). The rule still applies (§4.4).
- A maximum window size.
- Shared geometry types (`Vec2`, `Size`, `Rect`). `WindowSize` is local to `platform` until a math
  spec introduces common types.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/window_system.hpp
#pragma once

#include <coil/foundation/error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace coil::platform {

/// Identifies a window owned by a WindowSystem. Value 0 never identifies a window.
/// Ids are unique per WindowSystem instance and never reused, even after destruction.
struct WindowId {
    std::uint64_t value = 0;

    friend constexpr bool operator==(WindowId, WindowId) = default;
};

/// Window client-area size in logical units (DIPs, ADR-0006).
struct WindowSize {
    int width = 0;
    int height = 0;

    friend constexpr bool operator==(WindowSize, WindowSize) = default;
};

/// Parameters for creating a window.
struct WindowDesc {
    std::string title; ///< UTF-8. May be empty.
    WindowSize size;   ///< Both dimensions must be > 0.
};

/// Port: creates and manages OS windows. Implemented by adapters in backends/.
/// The WindowSystem owns every window it creates; callers refer to windows by WindowId.
class WindowSystem {
public:
    virtual ~WindowSystem();

    WindowSystem(const WindowSystem&) = delete;
    WindowSystem& operator=(const WindowSystem&) = delete;
    WindowSystem(WindowSystem&&) = delete;
    WindowSystem& operator=(WindowSystem&&) = delete;

    [[nodiscard]] virtual Expected<WindowId> createWindow(const WindowDesc& desc) = 0;
    [[nodiscard]] virtual Expected<void> destroyWindow(WindowId id) = 0;

    /// Live windows, in creation order.
    [[nodiscard]] virtual std::vector<WindowId> windows() const = 0;

    [[nodiscard]] virtual Expected<std::string> title(WindowId id) const = 0;
    [[nodiscard]] virtual Expected<void> setTitle(WindowId id, std::string_view title) = 0;
    [[nodiscard]] virtual Expected<WindowSize> size(WindowId id) const = 0;

protected:
    WindowSystem() = default;
};

} // namespace coil::platform
```
```cpp
// modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp
namespace coil::platform::testing {

/// In-memory WindowSystem for tests. No OS resources; deterministic ids starting at 1.
class FakeWindowSystem final : public WindowSystem { /* overrides of every member above */ };

} // namespace coil::platform::testing
```
```cpp
// modules/platform/tests/contract/coil/platform/window_system_contract.hpp
// Typed GoogleTest suite. An implementation opts in by specializing
//   template <> struct WindowSystemContractTraits<Impl> { static std::unique_ptr<WindowSystem> make(); };
// and instantiating:
//   INSTANTIATE_TYPED_TEST_SUITE_P(Impl, WindowSystemContract, ::testing::Types<Impl>);
```

### 4.2 Behavioral rules
1. `createWindow` with both size dimensions > 0 MUST create a window and return a `WindowId` whose
   `value` is not 0. The window MUST appear at the end of `windows()`.
2. `title(id)` and `size(id)` MUST return exactly the values of the `WindowDesc` used to create the
   window, or the latest `setTitle`. Titles are UTF-8 and MUST round-trip byte for byte. An empty
   title is valid.
3. `setTitle` MUST change only the given window.
4. `createWindow` with `width <= 0` or `height <= 0` MUST return `ErrorCode::InvalidArgument` and MUST
   NOT create a window (`windows()` unchanged).
5. After `destroyWindow(id)` succeeds, `id` MUST NOT appear in `windows()`, and every operation taking
   `id` (`destroyWindow`, `title`, `setTitle`, `size`) MUST return `ErrorCode::NotFound`.
6. Any id never returned by this `WindowSystem` (including `WindowId{}`, value 0) MUST give
   `ErrorCode::NotFound` for every operation taking an id.
7. A `WindowId` value MUST NOT be returned twice by the same `WindowSystem` instance, including after
   destruction.
8. Error messages are informative but not part of the contract. Tests check codes only.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `width <= 0` or `height <= 0` | `InvalidArgument`; no window created |
| Unknown, destroyed or zero `WindowId` | `NotFound` for `destroyWindow`, `title`, `setTitle`, `size` |
| Destroying the same window twice | First call succeeds; second returns `NotFound` |
| Empty title (create or `setTitle`) | Valid; `title()` returns `""` |

### 4.4 Threading and lifetime
- A `WindowSystem` and its windows are used from a single thread, the UI thread (ADR-0010). Calls
  from other threads are a contract violation. Enforcement is out of scope.
- Destroying the `WindowSystem` destroys all its windows. Ids from a destroyed `WindowSystem` must not
  be used.
- The `WindowSystem` owns windows. Callers hold only `WindowId`s, which are never dangling: misuse
  yields `NotFound`, not undefined behavior.

### 4.5 Async behavior
Not applicable. All operations are synchronous and fast.

### 4.6 Automation and agent exposure
Not applicable yet: windows become automation nodes (role `window`) when the automation module
exists (ADR-0015). Stable, never-reused `WindowId`s are a prerequisite for that.

## 5. Acceptance Criteria
Each criterion is a test in the **contract suite**, so it holds for every implementation. In this spec
it is proven against `FakeWindowSystem`.

- **AC-1** — Given an empty window system, when `createWindow({"Coil", {800, 600}})` is called, then it
  returns a `WindowId` with a non-zero value and `windows()` equals `{that id}`.
- **AC-2** — Given that window, `title()` is `"Coil"` and `size()` is `{800, 600}`. A window created
  with `"Coïl — 窓"` returns exactly those UTF-8 bytes.
- **AC-3** — Given two windows, when `setTitle(first, "Renamed")` is called, then the first title is
  `"Renamed"` and the second is unchanged. `setTitle(first, "")` then gives `""`.
- **AC-4** — Given sizes `{0, 600}`, `{800, 0}`, `{-1, 600}` and `{800, -1}`, `createWindow` returns
  `InvalidArgument` each time and `windows()` stays empty.
- **AC-5** — Given windows A and B, when A is destroyed, then `windows()` equals `{B}`, and
  `destroyWindow(A)`, `title(A)`, `setTitle(A, "x")` and `size(A)` all return `NotFound`. B still works.
- **AC-6** — For `WindowId{}` and an id never issued (`WindowId{999}`), all four id-taking operations
  return `NotFound`.
- **AC-7** — Given window A created then destroyed, when window B is created, then B's id differs
  from A's. Over 100 create/destroy cycles, no id value repeats.
- **AC-8** — The suite is a typed contract suite instantiated for `FakeWindowSystem`. Its test names
  carry the AC numbers, and `ctest -N` lists them under the `Fake` instantiation.

## 6. Test Plan
| AC | Test name (`WindowSystemContract`) | Type | Implementation under test |
|----|------------------------------------|------|---------------------------|
| AC-1 | `AC1_CreateReturnsValidIdAndListsWindow` | unit (contract) | `FakeWindowSystem` |
| AC-2 | `AC2_ReportsTitleAndSizeFromDesc` | unit (contract) | 〃 |
| AC-3 | `AC3_SetTitleChangesOnlyThatWindow` | unit (contract) | 〃 |
| AC-4 | `AC4_RejectsNonPositiveSize` | unit (contract) | 〃 |
| AC-5 | `AC5_DestroyedWindowIsNotFound` | unit (contract) | 〃 |
| AC-6 | `AC6_UnknownIdsAreNotFound` | unit (contract) | 〃 |
| AC-7 | `AC7_IdsAreNeverReused` | unit (contract) | 〃 |
| AC-8 | (structure) `ctest -N` listing | — | — |

The contract header lives under `modules/platform/tests/`, so it is **frozen** with the other tests
after Red. Future adapters reuse it but cannot weaken it.

## 7. Non-Functional Requirements
- `platform` (port) and `platform_testing` (fake) use only the standard library and `foundation`.
- `coil::platform_testing` and the contract target are linked only by test executables, never by
  library modules.

## 8. Architectural Impact
- New module `platform` (edge `platform → foundation`, already in OVERVIEW §3).
- Introduces the **test-support library convention**: `modules/<m>/testing/` builds
  `coil::<m>_testing`, which may depend on `coil::<m>` and is linked only by tests. ADR-0003 already
  describes fakes living there. I will add one line to OVERVIEW §3 to make the convention explicit.
- Introduces the **contract-suite convention**: `modules/<m>/tests/contract/` exposes typed suites through
  an INTERFACE target (`coil::<m>_contract`) for adapters' tests.
- No ABI or plugin impact yet.

## 9. Open Questions
*None.* Resolved 2026-11-01 by approval: Q1 system-owned windows with `WindowId`; Q2 empty titles allowed; Q3 reject only `<= 0`. Rationale kept below.
- **Q1 — Who owns windows?** *Recommended:* the `WindowSystem` owns them, and callers hold
  `WindowId`s. Ids can't dangle (misuse gives `NotFound`), they serialize naturally for workspaces,
  remote and MCP, and they suit multi-window docking. The alternative, caller-owned
  `std::unique_ptr<Window>`, makes lifetime the caller's problem across plugin and remote boundaries.
- **Q2 — Empty titles?** *Recommended:* allowed. OSes accept them, and rejecting them adds an error
  path with no benefit.
- **Q3 — Size limits?** *Recommended:* reject only `<= 0`. A maximum depends on the OS and display, so
  each adapter reports what the OS does.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0003): …`):
   - **Production skeleton:** the `platform` module with the port header and an out-of-line virtual
     destructor (`src/window_system.cpp`, which anchors the vtable).
   - **Fake stub:** `FakeWindowSystem` compiles, but `createWindow` returns `WindowId{}` (value 0)
     without storing anything; `windows()` is empty; `title()`/`size()` return empty values;
     `setTitle`/`destroyWindow` return success without effect.
   - **Tests:** the contract header, its instantiation for the fake, and a test `.clang-tidy`
     override. Lint-checked **before** the Red commit.
   - **Expected Red:** AC-1, AC-2, AC-3, AC-4, AC-5, AC-6 and AC-7 fail on assertions. AC-8 is
     structural.
2. **Green:** implement `FakeWindowSystem` (map from id to {title, size}, creation-order list,
   monotonically increasing counter starting at 1). No test file changes.
3. **Refactor:** `tools/check.sh`, `ctest --preset asan`, docs (OVERVIEW §3 convention line, EPIC-002
   status, Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | modify | `add_subdirectory(modules/platform)` |
| `modules/platform/CMakeLists.txt` | create | `coil::platform`, `coil::platform_testing`, tests |
| `modules/platform/include/coil/platform/window_system.hpp` | create | Port (§4.1) |
| `modules/platform/src/window_system.cpp` | create | Out-of-line destructor |
| `modules/platform/testing/include/coil/platform/testing/fake_window_system.hpp` | create | Fake declaration |
| `modules/platform/testing/src/fake_window_system.cpp` | create | Fake implementation |
| `modules/platform/tests/CMakeLists.txt` | create | Contract INTERFACE target + fake test executable |
| `modules/platform/tests/contract/coil/platform/window_system_contract.hpp` | create | Typed contract suite (AC-1…AC-7) |
| `modules/platform/tests/fake_window_system_test.cpp` | create | Traits specialization + instantiation for the fake |
| `modules/platform/tests/.clang-tidy` | create | Same GoogleTest-macro override as foundation's |
| `docs/architecture/OVERVIEW.md` | modify | Test-support and contract conventions |
| `docs/epics/EPIC-002-hello-window.md` | modify | Spec status |

### 10.3 Risks / alternatives considered
- **Typed-test macros and clang-tidy:** `TYPED_TEST_SUITE_P` expands to code that may trigger checks not
  covered by the existing test override. This is caught by linting before the Red commit. Any new
  suppression in the test override is brought to you first.
- **Alternative:** value-parameterized tests with a factory function instead of typed tests. Rejected:
  typed suites give each adapter its own named instantiation (`Fake/…`, `Sdl3/…`) and compile-time
  traits for setup.
- **`WindowSize` uses `int`,** matching OS window APIs. A later math spec may introduce float DIP
  geometry for layout; window sizes can convert at that boundary.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally; other targets once the CI matrix exists)
- [ ] Sanitizer preset passes
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `WindowSystemContract.AC1_CreateReturnsValidIdAndListsWindow`
- [ ] AC-2 → `WindowSystemContract.AC2_ReportsTitleAndSizeFromDesc`
- [ ] AC-3 → `WindowSystemContract.AC3_SetTitleChangesOnlyThatWindow`
- [ ] AC-4 → `WindowSystemContract.AC4_RejectsNonPositiveSize`
- [ ] AC-5 → `WindowSystemContract.AC5_DestroyedWindowIsNotFound`
- [ ] AC-6 → `WindowSystemContract.AC6_UnknownIdsAreNotFound`
- [ ] AC-7 → `WindowSystemContract.AC7_IdsAreNeverReused`
- [ ] AC-8 → `ctest -N` listing under `Fake/WindowSystemContract/…`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Spec and plan approved; Q1–Q3 confirmed as recommended | Emilio Pomares |
