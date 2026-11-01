# SPEC-0001: Build skeleton with one tested module and local quality gate

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | build infrastructure (`CMakeLists.txt`, `cmake/`, `tools/`), `coil::foundation` |
| Epic | [EPIC-001](../epics/EPIC-001-engineering-foundation.md) |
| Related ADRs | ADR-0001, ADR-0002, ADR-0003, ADR-0018 |
| Depends on | — |

> **Scope Gate checklist**
> - [x] One concern: a buildable, testable, lint-gated project skeleton on a developer machine
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules; dependencies limited to GoogleTest (ADR-0018)
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
No code can follow the Red → Green workflow until a project exists that configures, builds, runs tests,
and enforces formatting, lint and warnings. This spec establishes that foundation with the smallest
real module (`foundation`) and a single local entry point for every quality gate.

## 2. Scope
- Root `CMakeLists.txt` (CMake ≥ 3.28, C++23, extensions off, project version `0.1.0`).
- `CMakePresets.json` with configure, build and test presets `dev` (Debug), `release` and `asan`
  (ASan + UBSan; Clang/GCC only). Generator: Ninja.
- `vcpkg.json` (manifest mode, pinned `builtin-baseline`) with GoogleTest as the only dependency.
- `cmake/CoilWarnings.cmake`: warning flags from ADR-0002 as errors, applied through a
  `coil_set_warnings(<target>)` function.
- `cmake/CoilModule.cmake`: a `coil_add_module(<name> …)` helper that creates `coil_<name>`, alias
  `coil::<name>`, include dirs and warnings, plus a `coil_add_tests(<name> …)` helper using
  `gtest_discover_tests`.
- `modules/foundation` with one public header `coil/foundation/version.hpp`.
- `tools/check.sh`: format check → configure → build → clang-tidy → tests. Fails fast with a non-zero
  exit code. Sub-commands `tools/check.sh format [files…]` and `tools/check.sh tidy [files…]` run a
  single gate on given files (used by the gate tests).
- vcpkg is user-installed and located via `VCPKG_ROOT` (no submodule).
- Test fixtures proving the gates actually reject bad code (§5).

## 3. Out of Scope
- CI workflows and the platform matrix (EPIC-001 "CI matrix" spec).
- `tools/check_layering.py` (separate spec).
- Shared-library builds, symbol export macros and `COIL_BUILD_SHARED` (comes with the plugin host).
- TSan preset (added with the first threaded code, `UiDispatcher`).
- `coil::Error`, assertions, or any foundation content beyond the version header.
- Windows-specific local tooling (`check.ps1`). CI will cover Windows ("CI matrix" spec).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/foundation/include/coil/foundation/version.hpp
namespace coil::foundation {

struct Version {
    int major;
    int minor;
    int patch;
    friend constexpr bool operator==(const Version&, const Version&) = default;
};

/// Version of the Coil library, generated from the CMake project version.
inline constexpr Version kVersion{/* generated */};

/// "MAJOR.MINOR.PATCH"
[[nodiscard]] std::string_view versionString() noexcept;

} // namespace coil::foundation
```

### 4.2 Behavioral rules
1. `kVersion` MUST equal `project(VERSION …)` in the root `CMakeLists.txt`, via a configured header.
   It MUST NOT be hand-written in two places.
2. `versionString()` MUST return `"<major>.<minor>.<patch>"` matching `kVersion`.
3. Every Coil target MUST compile with ADR-0002's warning set as errors.
4. `tools/check.sh` MUST exit non-zero if any gate fails, and zero only when all pass.
5. Gate tests MUST check both the non-zero exit **and** the expected diagnostic in the output (e.g.
   `-Wunused-variable`, `readability-identifier-naming`), so a gate that fails for an unrelated
   reason does not count as a pass.
6. Format and tidy checks MUST cover all `*.hpp`/`*.cpp` under `modules/` and `backends/`, and MUST
   exclude `tests/fixtures/`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `VCPKG_ROOT` not set | Configure fails with a message explaining how to set it |
| `clang-format` / `clang-tidy` not found | `check.sh` fails naming the missing tool (no silent skip); gate tests fail, not skip |
| `asan` preset with MSVC | Preset hidden or unavailable on Windows; not an error elsewhere |

### 4.4 Threading and lifetime
Not applicable.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets).

## 5. Acceptance Criteria
- **AC-1** — Given a clean checkout and `VCPKG_ROOT` set, when running `cmake --preset dev` and
  `cmake --build --preset dev`, then both succeed and produce the target `coil::foundation`.
- **AC-2** — Given the build, when reading `coil::foundation::kVersion`, then it equals `{0, 1, 0}`,
  the CMake project version.
- **AC-3** — Given the build, when calling `versionString()`, then it returns `"0.1.0"`.
- **AC-4** — Given the build, when running `ctest --preset dev`, then all foundation tests are
  discovered individually and pass.
- **AC-5** — Given a fixture source containing a warning (unused variable), when compiled with
  `coil_set_warnings`, then compilation fails.
- **AC-6** — Given a fixture file violating `.clang-format`, when running the format gate on it, then
  the gate fails. Given a correctly formatted fixture, the gate passes.
- **AC-7** — Given a fixture violating a clang-tidy naming rule (`int BadName;` member), when running
  the tidy gate on it, then the gate fails.
- **AC-8** — Given the repository as delivered by this spec, when running `tools/check.sh`, then it
  exits 0. The `asan` preset also builds and passes tests on macOS.

## 6. Test Plan
| AC | Test name | Type | Mechanism |
|----|-----------|------|-----------|
| AC-1 | (implicit: every other test needs the build) + `check.sh` run | integration | configure + build |
| AC-2 | `Version.AC2_MatchesProjectVersion` | unit (gtest) | compares with values passed by CMake as compile definitions to the **test** target only |
| AC-3 | `Version.AC3_StringMatchesComponents` | unit (gtest) | |
| AC-4 | `ctest -N` lists both tests above by name | integration | ctest |
| AC-5 | `gate.warnings_are_errors` | CTest, `WILL_FAIL` | `try_compile` / build of `tests/fixtures/warning.cpp` |
| AC-6 | `gate.format_rejects_bad`, `gate.format_accepts_good` | CTest script tests | fixtures in `tests/fixtures/format/` |
| AC-7 | `gate.tidy_rejects_bad_naming` | CTest script test (`WILL_FAIL`) | fixture in `tests/fixtures/tidy/` |
| AC-8 | `tools/check.sh` exit code, plus `ctest --preset asan` | manual + recorded output | |

## 7. Non-Functional Requirements
- Fresh configure + build of the skeleton (excluding the first vcpkg dependency build) < 60 s on the dev Mac.
- No dependency other than GoogleTest.

## 8. Architectural Impact
- Creates the module layout described in OVERVIEW §3 and CLAUDE.md §8.
- New public type: `coil::foundation::Version`.
- No new dependencies beyond ADR-0018.

## 9. Open Questions
*None.* Resolved 2026-11-01:
- Q1 vcpkg → user-installed, located via `VCPKG_ROOT`.
- Q2 local verification on macOS only; other platforms via the "CI matrix" spec.

---

## 10. Implementation Plan

### 10.0 Prerequisites (developer machine, macOS)
| Need (ADR-0001) | Found on dev Mac | Action |
|-----------------|------------------|--------|
| CMake ≥ 3.28 | 3.21.4 (`/opt/local/bin`, MacPorts) | `brew install cmake` and put `/opt/homebrew/bin` before `/opt/local/bin`, or `sudo port selfupdate && sudo port upgrade cmake` |
| AppleClang ≥ 16 | AppleClang 15.0 | Install Command Line Tools for Xcode 16 (or Xcode 16) |
| vcpkg + `VCPKG_ROOT` | not installed | `git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh -disableMetrics`; add `export VCPKG_ROOT=$HOME/vcpkg` to `~/.zshrc` |
| clang-tidy | broken (z3 dylib) | `brew reinstall llvm` (in progress) |
| Ninja, clang-format | OK | — |

### 10.1 Steps
1. **Red — stubs + tests** (commit `test(0001): …`):
   - Build skeleton needed to run tests at all: root `CMakeLists.txt` (with a `VCPKG_ROOT` check before
     `project()`), `CMakePresets.json`, `vcpkg.json`, `cmake/CoilModule.cmake`, foundation target.
   - **Stubs:** `kVersion{0, 0, 0}`, `versionString()` returns `""`, `coil_set_warnings()` is an empty
     function, `tools/check.sh` sub-commands exit 0 without checking.
   - Tests: `Version.*` (gtest), gate tests (`tests/gates/*.cmake` CTest scripts) + fixtures.
   - **Expected Red result:** `AC2`, `AC3`, `gate.warnings_are_errors`, `gate.format_rejects_bad`,
     `gate.tidy_rejects_bad_naming` FAIL on their checks. `gate.format_accepts_good` passes already
     (it guards against over-rejection, and it is not a proof of new behavior; this is noted honestly).
2. **Green:** generate the version header from the project version, implement `versionString()`,
   real warning flags in `CoilWarnings.cmake`, real `check.sh` gates, `asan` preset + sanitizer flags.
   No file under `*/tests/*` changes.
3. **Refactor:** tidy CMake helpers, run `tools/check.sh` and `ctest --preset asan`, record output.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | create | Project, C++23, `VCPKG_ROOT` check, subdirectories, `enable_testing()` |
| `CMakePresets.json` | create | `dev`, `release`, `asan` configure/build/test presets (Ninja, `build/<preset>`, vcpkg toolchain, compile commands) |
| `vcpkg.json` | create | Manifest: `gtest`; `builtin-baseline` pinned to the vcpkg commit at implementation time |
| `cmake/CoilModule.cmake` | create | `coil_add_module()`, `coil_add_tests()` |
| `cmake/CoilWarnings.cmake` | create | `coil_set_warnings()` (ADR-0002 flags, Clang/GCC/MSVC) |
| `cmake/CoilSanitizers.cmake` | create | `COIL_SANITIZE` cache var → ASan/UBSan flags |
| `modules/foundation/CMakeLists.txt` | create | Target `coil_foundation` / `coil::foundation` |
| `modules/foundation/include/coil/foundation/version.hpp` | create | Public API (§4.1) |
| `modules/foundation/src/version_config.hpp.in` | create | Generated per-build values from `PROJECT_VERSION_*` |
| `modules/foundation/src/version.cpp` | create | `versionString()` |
| `modules/foundation/tests/version_test.cpp` | create | AC-2, AC-3 |
| `tests/CMakeLists.txt` | create | Gate tests, fixture targets (`EXCLUDE_FROM_ALL`) |
| `tests/gates/expect_failure.cmake` | create | Runs a command; passes only on non-zero exit **and** a matching diagnostic |
| `tests/fixtures/warning/unused_variable.cpp` | create | AC-5 |
| `tests/fixtures/format/{bad,good}.cpp` | create | AC-6 |
| `tests/fixtures/tidy/bad_naming.cpp` | create | AC-7 |
| `tools/check.sh` | create | Local quality gate |
| `CLAUDE.md` §7 | modify | Fill in real commands |

### 10.3 Risks / alternatives considered
- Homebrew clang-tidy + AppleClang compile database: the SDK sysroot must come from
  `compile_commands.json` (CMake adds `-isysroot`). If not, `check.sh` passes
  `--extra-arg=-isysroot$(xcrun --show-sdk-path)` on macOS.
- `version.hpp` reads constants from a generated private header so `kVersion` stays `constexpr` while
  the project version remains the single source of truth.
- AC-2's test receives the expected version from CMake as compile definitions on the **test** target,
  so the test does not merely re-read the header under test.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally; other targets in the "CI matrix" spec)
- [x] Sanitizer preset passes
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → configure/build
- [x] AC-2 → `Version.AC2_MatchesProjectVersion`
- [x] AC-3 → `Version.AC3_StringMatchesComponents`
- [x] AC-4 → `ctest -N` listing
- [x] AC-5 → `gate.warnings_are_errors`
- [x] AC-6 → `gate.format_rejects_bad`, `gate.format_accepts_good`
- [x] AC-7 → `gate.tidy_rejects_bad_naming`
- [x] AC-8 → `tools/check.sh` output, `ctest --preset asan` output

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec created (Draft) | |
| 2026-11-01 | Q1 (vcpkg via `VCPKG_ROOT`) and Q2 (macOS-only local) resolved; plan added | Emilio Pomares (answers) |
| 2026-11-01 | Spec and implementation plan approved | Emilio Pomares |
| 2026-11-01 | Attached to EPIC-001; forward references to unnumbered specs replaced by names (docs only) | Emilio Pomares |
| 2026-11-01 | **Red** on AppleClang 15.0.0, CMake 4.4.5, gtest 1.18.0 (vcpkg). `ctest -N` lists 6 tests. 5 fail for the planned reason: `AC2` (`kVersion.minor` 0 ≠ 1), `AC3` (`versionString()` `""` ≠ `"0.1.0"`), `gate.warnings_are_errors` / `gate.format_rejects_bad` / `gate.tidy_rejects_bad_naming` ("Expected the command to fail, but it succeeded"). `gate.format_accepts_good` passes (over-rejection guard, as planned). | Emilio Pomares (Red started on request) |
| 2026-11-01 | Red re-verified on AppleClang 21.0.0 (macOS 27.2, Xcode 27.2 CLT): identical 5 failures. | Emilio Pomares |
| 2026-11-01 | Plan deviation: the generated `version_config.hpp` is emitted to the build tree and exposed on the **public** include path (a public header includes it); behavior unchanged. | — (implementation detail) |
| 2026-11-01 | clang-tidy 23 findings: `portability-avoid-pragma-once` and `readability-trailing-comma` disabled in `.clang-tidy`; `modernize-use-designated-initializers` kept; test amended (AC2 expected value uses designated initializers) and re-Red shown failing (`kVersion.minor` 0 ≠ 1), commit `8eefb3b`. LLVM pinned to 23 (ADR-0001 Amendment 1). | Emilio Pomares |
| 2026-11-01 | **Green + Refactor**: `tools/check.sh` exit 0 (format 3 files, tidy 2 TUs, 6/6 tests); `ctest --preset asan` 6/6; `git diff 8eefb3b -- ':(glob)**/tests/**'` empty. All ACs verified. | Emilio Pomares |
| 2026-11-01 | Closed (Done) | Emilio Pomares |
