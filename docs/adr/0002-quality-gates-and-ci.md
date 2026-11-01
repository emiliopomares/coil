# ADR-0002: Lint, warnings, sanitizers, CI matrix
Status: Accepted (2026-11-01)

## Decision
- **clang-format** (`.clang-format`) is authoritative. CI fails on any diff.
- **clang-tidy** (`.clang-tidy`) with `WarningsAsErrors: '*'`, run on all `modules/` and `backends/`
  sources through `compile_commands.json`. Test directories may have a narrow override
  file that disables only checks triggered by GoogleTest macros, and the override must list each
  disabled check with a reason.
- **Compiler warnings as errors.** GCC/Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
  -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Werror`.
  MSVC: `/W4 /WX /permissive- /Zc:__cplusplus /utf-8`.
- **Sanitizers.** ASan+UBSan preset on Linux and macOS; TSan preset for any code that crosses threads.
- **Layering check.** `tools/check_layering.py` validates `#include` edges against `tools/layering.json`
  (OVERVIEW §3). It needs **Python ≥ 3.9, standard library only** (amended 2026-11-01, SPEC-0004).
- **CI (GitHub Actions)** matrix:
  windows-x64 (MSVC, clang-cl) · windows-arm64 (MSVC) · macos-arm64 (AppleClang) · macos-x64 ·
  linux-x64 (GCC, Clang) · linux-arm64 (GCC). Debug and Release builds. Sanitizer jobs on Linux x64.
- **Single local gate:** `tools/check.sh` runs format check, tidy, layering, build and tests.

## Consequences
+ Problems surface on every PR, on every platform.
− Slower CI, and clang-tidy cost grows with the code base (mitigate with caching and changed-files mode on PRs).
