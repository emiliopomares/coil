# EPIC-001: Engineering foundation

| Field | Value |
|-------|-------|
| Status | In Progress |
| Date | 2026-11-01 |
| Origin | Project kickoff: "clean, production quality code … must use a linter … SDD + TDD … cross-platform" |

## Goal
The project builds, tests and enforces its quality gates (format, lint, warnings, sanitizers, layering)
on every supported platform, locally and in CI.

## Done when
- A developer can configure, build, test and run all gates locally with one command.
- CI builds and tests on Windows (x64, arm64), macOS (arm64, x64) and Linux (x64, arm64) on every PR.
- Include edges that violate OVERVIEW §3 fail CI.
- Every C++ feature Coil relies on is compiled on every CI target, so an unsupported toolchain fails fast.

## Out of scope
- Any library functionality (`coil::Error` belongs to EPIC-002).
- Release packaging, `find_package(Coil)` export, shared-library builds.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0001 Build skeleton](../specs/0001-build-skeleton.md) | Done (pending acknowledgement) | — |
| 2 | CI matrix | Planned | SPEC-0001 |
| 3 | Layering checker (`tools/check_layering.py`) | Planned | SPEC-0001 |
| 4 | Compiler feature check | Planned | CI matrix; ADR-0001 amendment |

## Notes
- ADR-0001 Amendment 1 (accepted) defines the required C++23 feature set and pins LLVM 23; the compiler
  feature check spec verifies the provisional toolchain minimums in CI.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic created; SPEC-0001 attached retroactively | Emilio Pomares |
