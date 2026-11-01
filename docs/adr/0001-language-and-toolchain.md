# ADR-0001: Language, build system, dependencies
Status: Accepted (2026-11-01)

## Context
Coil must build cleanly on Windows, macOS and Linux for x64 and arm64, with one build description.

## Decision
- **C++23**, no compiler extensions. Minimum: MSVC 19.40 (VS 17.10), Clang 18, AppleClang 16, GCC 14.
  C++23 gives us `std::expected`, `std::move_only_function`, deducing `this`, and `std::print`.
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
