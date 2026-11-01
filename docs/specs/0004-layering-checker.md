# SPEC-0004: Layering checker for `#include` dependencies

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | tooling (`tools/`), gate tests (`tests/`). No library module changes |
| Epic | [EPIC-001](../epics/EPIC-001-engineering-foundation.md) |
| Related ADRs | ADR-0002 (quality gates), ADR-0004 (layered hexagonal architecture), ADR-0018 (third-party placement) |
| Depends on | SPEC-0001 (gate test infrastructure), SPEC-0003 (test-support conventions) |

> **Scope Gate checklist**
> - [x] One concern: reject `#include` directives that break the architecture's dependency rules
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 areas (tools, gate tests); no new library dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
The architecture's core promise (ADR-0004) is that library code in `modules/` never touches OS,
windowing or GPU APIs, and that modules depend on each other only along approved edges. Today, nothing
enforces this. The next spec adds the first third-party library (SDL3), so this is the last moment
before such leaks become possible. This spec adds an automated gate that reads every `#include` and
fails on any violation.

## 2. Scope
- `tools/check_layering.py`: a Python ≥ 3.9 script (standard library only) that scans `modules/` and
  `backends/` and reports violations.
- `tools/layering.json`: the **machine-readable dependency rules**, becoming the authoritative source
  for OVERVIEW §3's "may depend on" column.
- `tools/check.sh layering`, also run as part of `tools/check.sh` (after format, before configure).
- Gate tests that run the checker on small fixture trees, each containing exactly one kind of
  violation, or none.

## 3. Out of Scope
- Checking CMake `target_link_libraries` against the rules.
- Includes produced by macros (`#include MACRO`), and preprocessor conditions: every `#include` line is
  checked regardless of `#if`. Lines inside `/* … */` block comments are skipped only when the
  `#include` itself starts inside a comment that began earlier on the same line or before.
- Per-module third-party allowances (ADR-0018 private dependencies such as nlohmann/json in
  `modules/json`). Added, with config support, by the first spec that needs one.
- Rules for `examples/` and the root `tests/` folder.
- A Python linter or formatter for `tools/*.py`.
- Windows `check.ps1` (CI matrix spec).

## 4. Behavior Specification

### 4.1 Public interface (command line + config)
```
tools/check_layering.py [--root DIR] [--config FILE]
    --root    repository root to scan (default: the repository containing the script)
    --config  rules file (default: <root>/tools/layering.json)

Exit codes:  0 = no violations · 1 = violations found · 2 = usage or configuration error
Output:      one line per violation, sorted by path and line:
             <path relative to root>:<line>: error: <message> [<rule-id>]
```
```jsonc
// tools/layering.json
{
  "modules": {                       // module -> modules it may depend on (direct edges)
    "foundation": [],
    "platform":   ["foundation"],
    "render":     ["foundation"],
    "tasks":      ["foundation"],
    "json":       ["foundation"],
    "core":       ["foundation", "platform", "render", "tasks"],
    "widgets":    ["core"],
    "automation": ["core"],
    "docking":    ["core", "widgets", "automation", "json"],
    "mcp":        ["automation", "core", "json"],
    "remote":     ["core"],
    "plugin":     ["core"],
    "app":        ["docking", "mcp", "remote", "plugin"]
  },
  "backends": {}                     // backend -> modules it may include (ports); added per adapter spec
}
```

### 4.2 Rules (each with its rule id)
Terminology: a file is **test code** if it is under a `tests/` or `testing/` folder of a module or
backend. Otherwise it is **production code** (`include/`, `src/`). An angle include `<coil/…>` is
**resolved** by locating the header under the known include roots: `modules/*/include`,
`modules/*/testing/include` (test-support), `modules/*/tests/contract` (contract) and
`backends/*/include`. `foundation` is the dependency-free base layer: every module and backend may
include it without listing it.

1. **`module-edge`.** A file in module *M* may include `<coil/X/…>` only if *X* = *M*, *X* is listed in
   *M*'s **direct** dependencies, or *X* = `foundation`, which every module may include (§9 Q1).
   Headers of indirect dependencies are rejected, so a module cannot bypass the layer below it (e.g.
   `widgets` reaching the `platform` or `render` ports around `core`).
2. **`external-header`.** In `modules/`, an angle include that is not a `coil/` header MUST be a C++
   standard library header (fixed list in the script, e.g. `<vector>`, `<cstdint>`, `<format>`). OS,
   windowing, GPU and other third-party headers (`<windows.h>`, `<SDL3/SDL.h>`, `<vulkan/vulkan.h>`, …)
   are rejected.
3. **`test-framework`.** `<gtest/…>` and `<gmock/…>` are allowed only in test code.
4. **`test-support`.** Headers that resolve to a `testing/include` or `tests/contract` root may be
   included only by test code.
5. **`unresolved-coil-header`.** A `<coil/…>` include that matches no file under the known roots is a
   violation (typo, or a header in the wrong place). **Configured headers** (generated by CMake into the
   build tree) resolve through their template: `<coil/X/name.hpp>` also matches
   `modules/*/include/coil/X/name.hpp.in`. Templates therefore live at the include path they are
   generated to.
6. **`quote-include`.** `"…"` includes MUST resolve, relative to the including file's folder, to a file
   inside the same module or backend.
7. **`backend-edge`.** A file in backend *B* may include `<coil/X/…>` only if *X* is in *B*'s list or
   *X* = `foundation`. External headers are allowed in backends (that is where they belong).
8. **`unknown-module`** / **`unknown-backend`.** A folder under `modules/` or `backends/` without an entry
   in the config is a violation.
9. **Config validation** (exit 2): the config MUST be valid JSON with the two objects above. Every
   dependency MUST name a module defined in `modules`. The module graph MUST be acyclic (a cycle is
   reported as `config-cycle`, listing the cycle).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `--root` is not a directory, or the config is missing or invalid JSON | exit 2, message on stderr |
| Config cycle (`a → b → a`) | exit 2, `error: dependency cycle a -> b -> a [config-cycle]` |
| No `modules/` or `backends/` folder under root | Not an error (nothing to check) |
| `#include` with extra spaces (`#  include  <x>`) | Recognized |
| Non-UTF-8 bytes in a source file | Read with replacement characters; still checked |

### 4.4 Threading and lifetime
Not applicable (a command-line tool).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
- **AC-1** — Given the repository as it is today, when running `tools/check.sh layering`, then it
  exits 0 with no output lines starting with a path.
- **AC-2** — Given a fixture where `modules/foundation/src/a.cpp` includes
  `<coil/platform/window_system.hpp>`, the checker exits 1 and reports
  `modules/foundation/src/a.cpp:<line>: error: … [module-edge]`. An indirect include (module `c`
  depending on `b`, `b` on `a`, `c` including `<coil/a/…>`) is **also rejected** with `[module-edge]`.
  In a separate fixture, `c` including `<coil/b/…>` (direct) and `<coil/foundation/…>` (always allowed)
  is accepted (exit 0).
- **AC-3** — Given a fixture where `modules/platform/src/a.cpp` includes `<SDL3/SDL.h>` and
  `<windows.h>`, the checker exits 1 with two `[external-header]` violations. Standard headers in the
  same file (`<vector>`, `<cstdint>`) are not reported.
- **AC-4** — Given a fixture where production code includes `<gtest/gtest.h>`, a test-support header and
  a contract header, the checker exits 1 with `[test-framework]` and two `[test-support]` violations.
  The same includes inside `tests/` produce no violations (exit 0 in a separate fixture).
- **AC-5** — Given a fixture with `"detail.hpp"` resolving inside the module (accepted),
  `"../../other/src/x.hpp"` escaping it, and `<coil/foundation/missing.hpp>` matching no file, the checker
  exits 1 with exactly one `[quote-include]` and one `[unresolved-coil-header]` violation. In a separate
  fixture, `<coil/x/gen.hpp>` with only `modules/x/include/coil/x/gen.hpp.in` present is accepted.
- **AC-6** — Given a fixture with backend `sdl` allowed `["platform"]`, including
  `<coil/platform/…>`, `<coil/foundation/…>` (always allowed) and `<SDL3/SDL.h>` is accepted, and
  including `<coil/core/…>` gives `[backend-edge]`. An extra folder `modules/ghost/` (no config entry) gives
  `[unknown-module]`.
- **AC-7** — Given a config with a cycle (`a → b → a`), the checker exits 2 and prints the cycle with
  `[config-cycle]`. A config naming an undefined module dependency also exits 2.
- **AC-8** — `tools/check.sh` (all gates) runs the layering gate after format and before configure, and
  fails if the gate fails (proven by `tools/check.sh layering --root <violating fixture>` exiting
  non-zero).

## 6. Test Plan
CTest gate tests in `tests/CMakeLists.txt`, using `tests/gates/expect_failure.cmake`, which requires
both a non-zero exit and a matching diagnostic. Fixtures live in `tests/fixtures/layering/<case>/`,
each with its own `layering.json` and a minimal `modules/`/`backends/` tree.

| AC | Test name | Expectation |
|----|-----------|-------------|
| AC-1 | `gate.layering.AC1_RepositoryIsClean` | `tools/check.sh layering` exits 0 |
| AC-2 | `gate.layering.AC2_RejectsDisallowedModuleEdge` | fail, regex `foundation/src/a\.cpp:[0-9]+: error: .*\[module-edge\]` |
| AC-2 | `gate.layering.AC2_RejectsIndirectEdge` | fail, regex `c/src/.*\[module-edge\]` |
| AC-2 | `gate.layering.AC2_AcceptsDirectEdgeAndFoundation` | exit 0 |
| AC-3 | `gate.layering.AC3_RejectsExternalHeadersInModules` | fail, `SDL3/SDL\.h.*\[external-header\]` and `windows\.h.*\[external-header\]` (script checks the count of 2) |
| AC-4 | `gate.layering.AC4_RejectsTestOnlyIncludesInProduction` | fail, `[test-framework]` + 2 × `[test-support]` |
| AC-4 | `gate.layering.AC4_AcceptsTestOnlyIncludesInTests` | exit 0 |
| AC-5 | `gate.layering.AC5_RejectsEscapingAndUnresolvedIncludes` | fail, exactly 1 × `[quote-include]` and 1 × `[unresolved-coil-header]` |
| AC-5 | `gate.layering.AC5_AcceptsConfiguredHeaderTemplate` | exit 0 |
| AC-6 | `gate.layering.AC6_EnforcesBackendEdgesAndKnownFolders` | fail, `[backend-edge]` and `[unknown-module]`, no violation for the accepted lines |
| AC-7 | `gate.layering.AC7_RejectsConfigCycle` | exit 2 (non-zero) with `[config-cycle]` |
| AC-7 | `gate.layering.AC7_RejectsUndefinedDependency` | non-zero, message names the undefined module |
| AC-8 | `gate.layering.AC8_CheckShPropagatesFailure` | `tools/check.sh layering --root <AC-2 fixture> --config …` fails with `[module-edge]` |

**Counting violations.** Where an AC says "exactly", `expect_failure.cmake` cannot count matches. These
tests use a small companion helper, `tests/gates/expect_output.cmake`, which takes an expected exit code
and `EXPECT_COUNT_<n>=<regex>` arguments. It is created in this spec.

## 7. Non-Functional Requirements
- Python ≥ 3.9, standard library only (no `pip install`).
- Runs in < 2 s on the current repository.
- Deterministic output (sorted), so failures diff cleanly.

## 8. Architectural Impact
- `tools/layering.json` becomes the **authoritative** copy of OVERVIEW §3's dependency column. OVERVIEW §3
  says so, and both are changed together through an ADR when edges change.
- Python 3.9+ becomes a development-time requirement (it is already present on every CI image). Noted
  in ADR-0002.
- No library or ABI impact.

## 9. Open Questions
*None.* Q1 decided (option C); Q2 and Q3 confirmed as recommended by approval on 2026-11-01. Rationale kept below.
- **Q1 — May a module include headers of its *indirect* dependencies?** **Decided 2026-11-01: no
  (option C).** Only direct dependencies, plus `foundation`, which every module and backend may
  include. Transitive inclusion was rejected because it lets a module bypass the layer below it (e.g.
  `widgets` calling the `WindowSystem` or `RenderBackend` ports directly instead of going through
  `core`). It also makes dependencies shift when a middle layer is refactored. Listing `foundation`
  everywhere would add noise without protecting anything. Process consequence (CLAUDE.md §1): an edge
  that does not change layer order (its target is already reachable and it creates no cycle) needs
  only user approval in the spec adding it; an edge creating a new path needs an ADR.
- **Q2 — Where do the rules live?** *Recommended: `tools/layering.json`* as the authoritative copy,
  mirrored by the OVERVIEW table. The alternative, parsing the Markdown table in OVERVIEW, is fragile
  and couples documentation layout to tooling.
- **Q3 — Python as the implementation language?** *Recommended: yes, ≥ 3.9 with the standard library
  only.* It's available on all CI images and dev machines and needs no build step. A C++ tool would be
  more code for no gain, and shell scripts are not portable to Windows.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0004): …`):
   - **Stub** `tools/check_layering.py`: parses its arguments, then exits 0 without checking anything.
   - **Real files:** `tools/layering.json` (§4.1), and the `layering` sub-command in `tools/check.sh`
     wired into the all-gates run. The wiring is part of AC-8 and is exercised with the stub.
   - **Tests:** fixtures, the `expect_output.cmake` helper, and gate tests in `tests/CMakeLists.txt`.
   - **Expected Red:**
     - Fail: AC2-reject (×2: disallowed and indirect), AC3, AC4-reject, AC5, AC6, AC7 (×2), AC8. The stub exits 0 where a failure
       is required.
     - Already pass: AC1, AC2-accept (direct edge + foundation), AC4-accept, AC5-accept (configured header). These are over-rejection guards, like
       `gate.format_accepts_good`.
2. **Green:** implement the checker (include scanning, header resolution, closure computation, config
   validation, sorted output).
3. **Refactor:** `tools/check.sh` on the repo, `ctest --preset asan`, docs: OVERVIEW §3 (config is
   authoritative), ADR-0002 (Python requirement), CLAUDE.md §7 (`tools/check.sh layering`), EPIC-001.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `tools/check_layering.py` | create | The checker |
| `tools/layering.json` | create | Dependency rules |
| `tools/check.sh` | modify | `layering` sub-command; step in the all-gates run |
| `modules/foundation/src/version_config.hpp.in` → `modules/foundation/include/coil/foundation/version_config.hpp.in` | move | Template at its generated include path (rule 5); `configure_file` source updated |
| `tests/CMakeLists.txt` | modify | `gate.layering.*` tests; `find_package(Python3 3.9 COMPONENTS Interpreter REQUIRED)` |
| `tests/gates/expect_output.cmake` | create | Exit-code + counted-regex assertions |
| `tests/fixtures/layering/<case>/…` | create | One tree per case (§6) |
| `docs/architecture/OVERVIEW.md`, `docs/adr/0002-…`, EPIC-001 | modify | §8 notes; OVERVIEW §3 states the direct-edges + foundation rule |
| `CLAUDE.md` | modify | §1 edge rule (Q1 consequence); §7 `tools/check.sh layering` |

### 10.3 Risks / alternatives considered
- **Header resolution:** a `<coil/…>` path could in principle exist under two roots (for example the
  same name in `include/` and `testing/include/`). The checker treats that as a violation
  (`unresolved-coil-header`, "ambiguous") rather than guessing.
- **Standard library list:** a fixed list can lag a new standard header. Adding one is a one-line change
  in the script, reviewed like any other. A header missing from the list fails loudly, never silently.
- **Fixture sources are not compiled** (they include headers that do not exist on purpose). They live
  under `tests/fixtures/`, which the format and tidy gates already exclude.
- **Alternative considered:** include-what-you-use or clang-based dependency tools. These are heavier,
  need a configured build, and are not designed to enforce architectural edges.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the over-rejection guards listed in §10.1
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] `tools/check.sh` passes, now including the layering gate
- [ ] Sanitizer preset passes
- [ ] Every AC ticked below with its proving test
- [ ] Usage documented (script `--help`, CLAUDE.md §7)

### AC verification
- [ ] AC-1 → `gate.layering.AC1_RepositoryIsClean`
- [ ] AC-2 → `gate.layering.AC2_RejectsDisallowedModuleEdge`, `gate.layering.AC2_RejectsIndirectEdge`, `gate.layering.AC2_AcceptsDirectEdgeAndFoundation`
- [ ] AC-3 → `gate.layering.AC3_RejectsExternalHeadersInModules`
- [ ] AC-4 → `gate.layering.AC4_RejectsTestOnlyIncludesInProduction`, `gate.layering.AC4_AcceptsTestOnlyIncludesInTests`
- [ ] AC-5 → `gate.layering.AC5_RejectsEscapingAndUnresolvedIncludes`, `gate.layering.AC5_AcceptsConfiguredHeaderTemplate`
- [ ] AC-6 → `gate.layering.AC6_EnforcesBackendEdgesAndKnownFolders`
- [ ] AC-7 → `gate.layering.AC7_RejectsConfigCycle`, `gate.layering.AC7_RejectsUndefinedDependency`
- [ ] AC-8 → `gate.layering.AC8_CheckShPropagatesFailure`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Q1 decided: direct edges only + `foundation` always allowed (option C); rules 1 and 7, AC-2, AC-6 and test plan updated | Emilio Pomares |
| 2026-11-01 | Spec and plan approved (Q2: `tools/layering.json`; Q3: Python ≥ 3.9 stdlib). Docs step also fixes the OVERVIEW §2 diagram (fakes in `modules/<m>/testing/`) and narrows ADR-0003 to that location. | Emilio Pomares |
| 2026-11-01 | Amendment (found while preparing Red): generated headers. `version.hpp` includes `<coil/foundation/version_config.hpp>`, which only exists in the build tree. Rule 5 now resolves configured headers through their `.hpp.in` template under a module's `include/` root; the template moves there; new acceptance test `AC5_AcceptsConfiguredHeaderTemplate`. | Emilio Pomares |
