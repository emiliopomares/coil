# SPEC-0005: SDL3 window adapter passing the window contract headlessly

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `backends/platform_sdl3` (new); build infrastructure (`vcpkg.json`, CMake helper, `tools/layering.json`) |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0004 (ports & adapters), ADR-0010 (UI thread), ADR-0013 (errors), ADR-0018 (SDL3 confined to `backends/platform_sdl3`) |
| Depends on | SPEC-0003 (window port + contract), SPEC-0004 (layering checker) |

> **Scope Gate checklist**
> - [x] One concern: a real `WindowSystem` implementation on SDL3
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (new backend; build files). SDL3 already approved by ADR-0018; the
>   `platform_sdl3 → platform` edge is the one ADR-0018 designed
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
The window port exists and has a contract, but only a fake implements it. This spec adds the first real
adapter: `SdlWindowSystem`, backed by SDL3. It proves that the port, the contract suite and the layering
rules work with a real third-party library, and it is the step before an app can open a visible window.
The adapter is tested **headlessly** with SDL's `offscreen` video driver, so tests need no display.

## 2. Scope
- `sdl3` added to `vcpkg.json` (version from the pinned baseline: 3.4.18).
- `cmake/CoilModule.cmake`: `coil_add_backend(<name> …)`, building `coil_<name>` / `coil::<name>` from
  `backends/<name>/`.
- `backends/platform_sdl3`:
  - Public header with a factory and an options struct. **No SDL types in the public header.**
  - Private implementation `SdlWindowSystem : WindowSystem`. SDL is linked `PRIVATE`.
- `tools/layering.json`: `"backends": { "platform_sdl3": ["platform"] }`.
- Tests: the `WindowSystemContract` suite instantiated for SDL (`Sdl3/…`), plus adapter-specific tests.

## 3. Out of Scope
- Events, including close requests (window events spec).
- Showing or hiding windows, position, high-DPI flags, fullscreen, icons, native handles for render
  backends, graphics-API flags (`SDL_WINDOW_VULKAN` etc.).
- Running tests against a real display or on Linux/Windows. Local verification is macOS only; other
  platforms come with the CI matrix spec.
- Making SDL3 optional (vcpkg features or a CMake option). SDL3 is always built for now. If library
  users need a build without it, a later spec handles that.
- Multiple simultaneous `SdlWindowSystem` instances (rejected, §4.2 rule 4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// backends/platform_sdl3/include/coil/platform_sdl3/sdl_window_system.hpp
#pragma once

#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>

#include <memory>
#include <string>

namespace coil::platform_sdl3 {

/// Options for creating the SDL3-backed window system.
struct SdlWindowSystemOptions {
    /// SDL video driver to use (e.g. "cocoa", "x11", "wayland", "windows", "offscreen").
    /// Empty: let SDL choose the platform default.
    std::string videoDriver;
};

/// Creates a WindowSystem backed by SDL3. Initializes SDL's video subsystem; the returned object
/// shuts it down when destroyed. Must be called, and the result used, on the main thread.
/// Fails with Unavailable if SDL video cannot be initialized or is already initialized.
[[nodiscard]] Expected<std::unique_ptr<platform::WindowSystem>>
createSdlWindowSystem(const SdlWindowSystemOptions& options = {});

} // namespace coil::platform_sdl3
```

### 4.2 Behavioral rules
1. `SdlWindowSystem` MUST satisfy every rule of SPEC-0003 §4.2. Its window ids come from its own counter
   (starting at 1), not from SDL's window ids (§9 Q3).
2. `createSdlWindowSystem` MUST apply `options.videoDriver` (if not empty) as SDL's video-driver hint,
   then initialize the SDL video subsystem. On failure it MUST return `Unavailable` with SDL's error
   text in the message, and leave SDL video **not** initialized.
3. Windows are created **visible**, with the requested title and size. `size()` reports the size SDL
   reports for the window (SDL may differ from the request on real displays, e.g. tiling window
   managers. Under `offscreen` it matches).
4. If SDL's video subsystem is already initialized when `createSdlWindowSystem` is called (by another
   `SdlWindowSystem` or by the host application), it MUST return `Unavailable` and change nothing
   (§9 Q2).
5. Destroying the `SdlWindowSystem` MUST destroy all its remaining windows and shut down SDL's video
   subsystem, so a new `SdlWindowSystem` can be created afterwards.
6. Any SDL failure while creating a window MUST return `Internal` with SDL's error text. No partial
   window is left registered.
7. The public header MUST compile without SDL's include path. No SDL type or header leaks to users.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Unknown video driver name | `Unavailable`; SDL video not initialized afterwards |
| SDL video already initialized | `Unavailable`; existing state untouched |
| `SDL_CreateWindow` fails | `Internal` with SDL's message; no window registered |
| Non-positive size | `InvalidArgument` (checked before calling SDL, per contract) |
| Windows still open when the system is destroyed | Destroyed by the system; video subsystem shut down |

### 4.4 Threading and lifetime
- SDL requires video calls on the **main thread** (mandatory on macOS). The `SdlWindowSystem` must be
  created, used and destroyed on the main thread, which is Coil's UI thread (ADR-0010). Enforcement is
  out of scope (no `COIL_ASSERT` yet).
- The `SdlWindowSystem` owns all SDL windows it creates (SPEC-0003 Q1) and SDL's video subsystem for its
  lifetime.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets).

## 5. Acceptance Criteria
- **AC-1** — Given `videoDriver = "offscreen"`, `createSdlWindowSystem` succeeds and returns a usable
  system with no windows.
- **AC-2** — The complete `WindowSystemContract` suite (SPEC-0003, AC-1…AC-7) passes for the SDL
  adapter, instantiated as `Sdl3` and run under the `offscreen` driver. The contract header is reused
  unchanged.
- **AC-3** — Given `videoDriver = "no-such-driver"`, `createSdlWindowSystem` returns `Unavailable` with
  a non-empty message, and afterwards `SDL_WasInit(SDL_INIT_VIDEO)` is 0.
- **AC-4** — Given a live `SdlWindowSystem`, a second `createSdlWindowSystem` returns `Unavailable`.
  After the first is destroyed, creating one succeeds again.
- **AC-5** — Given a `SdlWindowSystem` with two open windows, destroying it leaves
  `SDL_WasInit(SDL_INIT_VIDEO) == 0`. Under the `asan` preset, no leaks are reported.
- **AC-6** — A translation unit that includes only `<coil/platform_sdl3/sdl_window_system.hpp>` and links
  only `coil::platform_sdl3` (no SDL include path) compiles. This is proven by a gate test building that
  target.
- **AC-7** — `tools/layering.json` registers `platform_sdl3` with `["platform"]`, and
  `tools/check.sh layering` passes with the new backend, which includes SDL headers. SDL headers
  anywhere in `modules/` would still be rejected (already proven by SPEC-0004 AC-3).

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `SdlWindowSystem.AC1_CreatesWithOffscreenDriver` | backend unit | |
| AC-2 | `Sdl3/WindowSystemContract.AC1…AC7` | contract | traits `make()` uses `offscreen` |
| AC-3 | `SdlWindowSystem.AC3_UnknownDriverIsUnavailable` | backend unit | checks `SDL_WasInit` |
| AC-4 | `SdlWindowSystem.AC4_SecondInstanceIsUnavailableUntilFirstIsDestroyed` | backend unit | |
| AC-5 | `SdlWindowSystem.AC5_DestructionClosesWindowsAndShutsDownVideo` | backend unit + `asan` run | |
| AC-6 | `gate.platform_sdl3.AC6_PublicHeaderNeedsNoSdl` | CTest builds an `EXCLUDE_FROM_ALL` target | passes if the build succeeds |
| AC-7 | `gate.layering.AC1_RepositoryIsClean` (existing) | gate | new backend present |

All backend tests run in one executable on the main thread, sequentially. Each test creates and destroys
its own `SdlWindowSystem`.

## 7. Non-Functional Requirements
- SDL3 appears only in `backends/platform_sdl3` (ADR-0018). The layering checker and AC-6 enforce it.
- First configure builds SDL3 through vcpkg (a few minutes, then cached).

## 8. Architectural Impact
- First backend: establishes `backends/<name>/{include,src,tests}` and `coil_add_backend`.
- New dependency SDL3 3.4.18, as already approved and confined by ADR-0018.
- New edge `platform_sdl3 → platform`, the edge ADR-0018 defines. Per CLAUDE.md §1 it needs only
  approval of this spec.

## 9. Open Questions
*None.* Q1–Q4 confirmed as recommended by approval on 2026-11-01 (Q2 clarified: one `SdlWindowSystem` manages any number of windows; only a second *system* is rejected). Rationale kept below.
- **Q1 — How do tests select the headless driver?** *Recommended: an options struct
  (`videoDriver`).* It's a real feature (apps may force `wayland` or `x11`), and tests can pick
  `offscreen` or a broken driver per test. The alternative, the `SDL_VIDEODRIVER` environment variable,
  works per process only, so AC-3 would need its own executable, and it hides an important setting.
- **Q2 — More than one live `SdlWindowSystem`?** *Recommended: no; the second gets `Unavailable`.*
  SDL's video state is process-global (one driver, one event queue). Two owners would silently share
  it. Detection uses `SDL_WasInit`, so no global variable is added in Coil.
- **Q3 — Window ids: SDL's or our own?** *Recommended: our own counter.* The contract requires ids that
  are never reused per instance. SDL's ids are process-wide and not specified as never reused across
  re-initialization.
- **Q4 — Windows visible on creation?** *Recommended: yes.* That's what `hello_window` needs. A
  `hidden` option comes later if needed. Under `offscreen` nothing is shown anyway.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0005): …`):
   - **Build:** `vcpkg.json` gets `sdl3`; `coil_add_backend`; `backends/platform_sdl3` with the public
     header; `tools/layering.json` entry; `add_subdirectory(backends/platform_sdl3)`.
   - **Stub:** `createSdlWindowSystem` returns a stub `SdlWindowSystem` without touching SDL. The stub
     behaves like the SPEC-0003 stub fake (ids 0, empty values, success everywhere).
   - **Tests:** contract instantiation, adapter tests, the AC-6 header target and gate test, and a test
     `.clang-tidy` (same override as `modules/platform/tests`). Pre-freeze: lint the tests and confirm
     the `offscreen` driver exists in the vcpkg SDL build (§10.3).
   - **Expected Red:**
     - Fail: contract AC1–AC7, AC3 (no error), AC4 (second instance succeeds).
     - Pass as guards: AC1 (creation "succeeds", no windows), AC5 (`SDL_WasInit` is 0 because the stub
       never initialized SDL), AC6 (the header is already SDL-free) and AC-7 (layering).
2. **Green:** implement with SDL3: `SDL_SetHint(SDL_HINT_VIDEO_DRIVER)`, `SDL_InitSubSystem` /
   `SDL_QuitSubSystem(SDL_INIT_VIDEO)`, `SDL_CreateWindow`, `SDL_SetWindowTitle`,
   `SDL_GetWindowTitle`, `SDL_GetWindowSize`, `SDL_DestroyWindow`. Map of our ids to `SDL_Window*`
   (owned with a custom deleter). No test changes.
3. **Refactor:** `tools/check.sh`, `ctest --preset asan`, docs (OVERVIEW §3 backend table, EPIC-002,
   Change Log).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `vcpkg.json` | modify | add `sdl3` |
| `CMakeLists.txt` | modify | `add_subdirectory(backends/platform_sdl3)` |
| `cmake/CoilModule.cmake` | modify | `coil_add_backend` |
| `tools/layering.json` | modify | backend entry |
| `backends/platform_sdl3/CMakeLists.txt` | create | target, SDL3 `PRIVATE`, tests |
| `backends/platform_sdl3/include/coil/platform_sdl3/sdl_window_system.hpp` | create | Public API (§4.1) |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | create | Implementation |
| `backends/platform_sdl3/tests/CMakeLists.txt` | create | tests, AC-6 target + gate test |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | create | AC-1, AC-3, AC-4, AC-5 + contract instantiation |
| `backends/platform_sdl3/tests/public_header_only.cpp` | create | AC-6 translation unit |
| `backends/platform_sdl3/tests/.clang-tidy` | create | test override (copy) |
| `docs/architecture/OVERVIEW.md`, EPIC-002 | modify | docs |

### 10.3 Risks / alternatives considered
- **`offscreen` driver availability:** SDL enables it by default and the vcpkg port does not disable
  it, but this will be verified in Red before freezing. If it's missing, SDL's `dummy` driver is the
  fallback. Switching would be a spec amendment brought to you.
- **macOS main thread:** GoogleTest runs tests on the main thread. Tests must never create SDL objects
  on other threads.
- **Build time:** building SDL3 the first time takes minutes; vcpkg caches it afterwards.
- **Real-display size differences** (rule 3) don't affect tests (offscreen), but they mean the contract
  suite should not be expected to pass against every real window manager. This is noted for the CI
  matrix spec.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally; other targets once the CI matrix exists)
- [ ] Sanitizer preset passes (including AC-5 leak check)
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `SdlWindowSystem.AC1_CreatesWithOffscreenDriver`
- [ ] AC-2 → `Sdl3/WindowSystemContract.AC1…AC7`
- [ ] AC-3 → `SdlWindowSystem.AC3_UnknownDriverIsUnavailable`
- [ ] AC-4 → `SdlWindowSystem.AC4_SecondInstanceIsUnavailableUntilFirstIsDestroyed`
- [ ] AC-5 → `SdlWindowSystem.AC5_DestructionClosesWindowsAndShutsDownVideo` (+ `asan`)
- [ ] AC-6 → `gate.platform_sdl3.AC6_PublicHeaderNeedsNoSdl`
- [ ] AC-7 → `gate.layering.AC1_RepositoryIsClean`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Spec and plan approved | Emilio Pomares |
