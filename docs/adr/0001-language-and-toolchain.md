# ADR-0001: Language, build system, dependencies
Status: Accepted (2026-11-01), including Amendment 1

## Context
Coil must build cleanly on Windows, macOS and Linux for x64 and arm64, with one build description.

## Decision
- **C++23**, no compiler extensions. Minimum toolchains: see Amendment 1, which replaces the original
  list (MSVC 19.40, Clang 18, AppleClang 16, GCC 14).
- **C++20 modules are not used** yet: support across CMake, MSVC, Clang and GCC and in clang-tidy is
  still uneven. Revisit later in a new ADR.
- **CMake ≥ 3.28 with `CMakePresets.json`**, Ninja generator (Ninja Multi-Config allowed). Presets:
  `dev`, `release`, `asan`, `tsan`, `ci-<os>-<arch>`.
- **vcpkg in manifest mode** (`vcpkg.json`, pinned baseline) for third-party dependencies. It works the
  same on all 6 targets and integrates with CMake via the toolchain file.
- **Dependency policy:** `modules/` may depend only on the C++ standard library (and, if needed,
  small header-only libraries approved by ADR). Heavy dependencies (SDL3, Vulkan, FreeType, HarfBuzz,
  MessagePack) live only in `backends/`. Each new third-party dependency requires an ADR.
- Libraries build as static or shared (`COIL_BUILD_SHARED`). Public symbols are exported with
  `COIL_<MODULE>_API` macros. Default symbol visibility is hidden.

## Consequences
+ Modern, expressive standard library features; portable.
− Users must have recent toolchains (acceptable for a new engine project).

## Amendment 1 (2026-11-01): minimum toolchains defined by tested features
Status: Accepted (2026-11-01)

### Context
The original minimums were chosen by version number and listed features that turned out not to exist on
them. Probes on the development Mac showed AppleClang 15 lacking `std::stop_token`, `std::jthread`,
`std::move_only_function` and deducing `this`. After upgrading to AppleClang 21.0.0 (Command Line
Tools for Xcode 27.2), every probed feature compiled and ran **except** `std::move_only_function`.

### Decision
- **Required C++23 feature set** (the contract; versions follow from it): `std::expected`,
  `std::format`, `std::print`, coroutines (`<coroutine>`), `std::stop_token` / `std::stop_source`,
  `std::jthread`, deducing `this`.
- **Banned until available on every target:** `std::move_only_function`. Use `std::function`; if a
  move-only callable is ever required, a small Coil type is added through its own spec.
- **Minimum toolchains:**
  | Toolchain | Minimum | Verified |
  |-----------|---------|----------|
  | AppleClang | 21 (Command Line Tools / Xcode 27.2) | Yes, dev Mac, macOS 27.2 |
  | Clang | 18 | Provisional |
  | GCC | 14 | Provisional |
  | MSVC | 19.40 (VS 17.10) | Provisional |
  Provisional entries become verified (or are raised) by the "Compiler feature check" spec (EPIC-001),
  which compiles the required feature set on every CI target.
- **clang-format / clang-tidy are pinned to LLVM major version 23**, both from the same release, so
  formatting and lint results do not drift between machines. `.clang-tidy` enables check groups by
  wildcard, so a new LLVM major can introduce new checks: moving to it is a deliberate change made
  through its own spec. `tools/check.sh` locates the tools in `PATH` or in Homebrew's LLVM directory.
- **Lint policy adjustments for LLVM 23** (approved 2026-11-01): `portability-avoid-pragma-once` is
  disabled (it contradicts the `#pragma once` rule); `readability-trailing-comma` is disabled (style
  owned by clang-format). `modernize-use-designated-initializers` stays enabled.

### Consequences
+ Toolchain requirements are backed by evidence and enforced by CI instead of assumed.
+ ADR-0014 keeps `std::stop_token` for cancellation (no custom token needed).
− Contributors on macOS need Xcode 27.2-era tools.
