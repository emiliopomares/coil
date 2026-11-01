# SPEC-0002: `coil::Error` and `Expected<T>` for recoverable errors

| Field | Value |
|-------|-------|
| Status | In Progress |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-01 |
| Module(s) | `coil::foundation` |
| Epic | [EPIC-002](../epics/EPIC-002-hello-window.md) |
| Related ADRs | ADR-0013 (error handling), ADR-0008 (remote protocol: stable codes) |
| Depends on | SPEC-0001 |

> **Scope Gate checklist**
> - [x] One concern: the value type Coil uses to report recoverable errors
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches ≤ 2 modules (foundation only); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is empty

---

## 1. Problem / Motivation
ADR-0013 says recoverable errors are returned as `std::expected<T, coil::Error>`. Nothing in Coil can
fail yet, but the window port (next in EPIC-002) needs to report `InvalidArgument`, `NotFound` and
`Unavailable`. This spec defines the error value, its codes and the helpers every later spec will use.

## 2. Scope
- `ErrorCode`: a single, project-wide enumeration with **stable numeric values**, starting with
  `InvalidArgument`, `NotFound`, `Unavailable` and `Internal`.
- `Error`: an immutable value holding an `ErrorCode` and an optional human-readable message.
- `Expected<T>`: an alias for `std::expected<T, Error>` (including `Expected<void>`).
- `makeUnexpected(code, message)`: a helper that builds the error side of an `Expected`.
- `toString(ErrorCode)` and `std::format` support for `ErrorCode` and `Error`, used by logs and test
  output.

## 3. Out of Scope
- Assertion and contract macros (`COIL_EXPECTS`, `COIL_ASSERT`), which get their own spec.
- Logging (`Logger` port).
- Error codes beyond the four listed. Each later spec adds the codes it needs, appended with new
  numbers (§4.2 rule 2).
- Per-module error categories, error chaining or causes, stack traces, source locations.
- Helpers for propagating errors (`COIL_TRY`-style macros), a later spec if repetition appears.
- GoogleTest pretty-printing of `Error`. It is test convenience, not library behavior; it can be
  added inside a test file when needed.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/foundation/include/coil/foundation/error.hpp
#pragma once

#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace coil::foundation {

/// Project-wide error codes. Numeric values are stable and never reused: they may cross process
/// boundaries (remote protocol, MCP) and be persisted.
enum class ErrorCode : std::uint16_t {
    InvalidArgument = 1, ///< A caller-supplied value is outside the accepted domain.
    NotFound = 2,        ///< A referenced entity (id, handle, name) does not exist.
    Unavailable = 3,     ///< A required resource or service cannot be used right now.
    Internal = 4,        ///< An unexpected failure inside Coil or a backend.
};

/// Returns the enumerator name ("InvalidArgument", …), or "Unknown" for any other value.
[[nodiscard]] std::string_view toString(ErrorCode code) noexcept;

/// A recoverable error: a code for programs to branch on, plus an optional message for humans.
/// Program logic must never branch on the message.
class Error {
public:
    explicit Error(ErrorCode code, std::string message = {});

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;

private:
    ErrorCode m_code;
    std::string m_message;
};

/// Result of an operation that can fail recoverably.
template <typename T>
using Expected = std::expected<T, Error>;

/// Builds the error side of an Expected: `return makeUnexpected(ErrorCode::NotFound, "…");`
[[nodiscard]] std::unexpected<Error> makeUnexpected(ErrorCode code, std::string message = {});

} // namespace coil::foundation

namespace coil {
using foundation::Error;
using foundation::ErrorCode;
using foundation::Expected;
using foundation::makeUnexpected;
} // namespace coil

/// Formats as the enumerator name, same as toString().
template <>
struct std::formatter<coil::foundation::ErrorCode> : std::formatter<std::string_view> { /* … */ };

/// Formats as "<Code>: <message>", or "<Code>" when the message is empty.
template <>
struct std::formatter<coil::foundation::Error> : std::formatter<std::string_view> { /* … */ };
```

### 4.2 Behavioral rules
1. `Error` MUST store exactly the code and message it was constructed with. It is immutable after
   construction (no setters).
2. `ErrorCode` numeric values MUST NOT change once released. New codes are appended with new
   numbers. Removed codes leave their number permanently unused. Value `0` is reserved and never
   assigned.
3. `toString` MUST return the exact enumerator name for each defined code, and `"Unknown"` for any
   other value (e.g. `static_cast<ErrorCode>(999)`).
4. `std::format("{}", error)` MUST produce `"<Code>: <message>"`, or `"<Code>"` if the message is
   empty. `<Code>` is `toString(error.code())`.
5. `makeUnexpected(code, message)` MUST produce an `std::unexpected<Error>` whose error has that code
   and message, and MUST be convertible to `Expected<T>` for any `T`, including `void`.
6. `coil::Error`, `coil::ErrorCode`, `coil::Expected` and `coil::makeUnexpected` MUST name the same
   entities as their `coil::foundation::` counterparts.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Out-of-range `ErrorCode` (cast from integer) | `toString` → `"Unknown"`; formatting → `"Unknown"` / `"Unknown: <message>"` |
| Empty message | Valid; `message()` is empty; formatted without `": "` |
| Message with non-ASCII UTF-8 | Stored and formatted byte for byte |

### 4.4 Threading and lifetime
`Error` is a regular value type (copyable, movable) with no shared state. It is safe to use from any
thread. `message()` returns a reference valid for the lifetime of the `Error`.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widgets). The stable numeric codes (rule 2) are what will later let MCP and remote
clients branch on errors.

## 5. Acceptance Criteria
- **AC-1** — Given `Error{ErrorCode::NotFound, "window 3"}`, then `code()` is `NotFound` and
  `message()` is `"window 3"`.
- **AC-2** — Given `Error{ErrorCode::Internal}`, then `message()` is empty.
- **AC-3** — Given a function returning `Expected<int>` that executes
  `return makeUnexpected(ErrorCode::InvalidArgument, "size must be positive");`, then the result has
  no value, and its error has code `InvalidArgument` and that message.
- **AC-4** — Given a function returning `Expected<void>` that returns
  `makeUnexpected(ErrorCode::Unavailable)`, then the result has no value and its error code is
  `Unavailable`.
- **AC-5** — `toString` returns `"InvalidArgument"`, `"NotFound"`, `"Unavailable"`, `"Internal"` for
  the four codes, and `"Unknown"` for `static_cast<ErrorCode>(999)`.
- **AC-6** — `std::format("{}", Error{ErrorCode::NotFound, "window 3"})` is `"NotFound: window 3"`.
  A UTF-8 message (`"ventana «Coïl»"`) is reproduced byte for byte.
- **AC-7** — `std::format("{}", Error{ErrorCode::Internal})` is `"Internal"`, and
  `std::format("{}", ErrorCode::Unavailable)` is `"Unavailable"`.
- **AC-8** — The numeric values are `InvalidArgument = 1`, `NotFound = 2`, `Unavailable = 3`,
  `Internal = 4` (stability guard for rule 2).

## 6. Test Plan
All tests in `modules/foundation/tests/error_test.cpp` (unit, GoogleTest, no fakes needed).

| AC | Test name | Notes |
|----|-----------|-------|
| AC-1 | `Error.AC1_StoresCodeAndMessage` | |
| AC-2 | `Error.AC2_MessageIsOptional` | |
| AC-3 | `Error.AC3_MakeUnexpectedFillsExpectedOfValue` | through a helper function returning `Expected<int>` |
| AC-4 | `Error.AC4_MakeUnexpectedFillsExpectedOfVoid` | through a helper function returning `Expected<void>` |
| AC-5 | `Error.AC5_ToStringNamesEveryCode` | includes the out-of-range case (§4.3) |
| AC-6 | `Error.AC6_FormatsCodeAndMessage` | includes the UTF-8 case (§4.3) |
| AC-7 | `Error.AC7_FormatsWithoutMessageAndFormatsCode` | |
| AC-8 | `Error.AC8_CodesHaveStableNumericValues` | **passes in Red** (see §10.3) |
| Rule 6 | `Error.CoilNamespaceAliasesFoundation` | `std::is_same_v` checks; compile-time, also passes in Red |

## 7. Non-Functional Requirements
- No dependencies beyond the standard library.
- `toString` and `code()` do not allocate.

## 8. Architectural Impact
- Adds a public header to `foundation`. Every later module will depend on it.
- New public types: `coil::foundation::{ErrorCode, Error, Expected}`, `makeUnexpected`, `toString`, and
  two `std::formatter` specializations. The `coil::` aliases are defined in the same header.
- ADR-0013 says "module-scoped code enums". This spec uses **one project-wide enum** instead (§9 Q1).
  If confirmed, I will amend ADR-0013 in the same change to match.

## 9. Open Questions
*None.* Resolved 2026-11-01:
- **Q1 — One project-wide `ErrorCode` enum.** Confirmed by approval.
  Rationale: It's simpler, every client (including remote and MCP) sees one
  list, and codes describe the kind of failure, not where it happened. Detail goes in the message.
  Per-module categories can be added later if a module needs codes no other module shares.
  ADR-0013 is amended to match.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0002): …`):
   - Stubs that compile and return wrong or neutral values:
     - `Error` ignores its arguments, so `code()` returns `ErrorCode::Internal` and `message()` returns
       an empty string.
     - `toString` returns `""`.
     - Both formatters output `""`.
     - `makeUnexpected` returns `std::unexpected(Error{ErrorCode::Internal})`.
     - The enum is declared with its final values.
   - Tests from §6, registered in `modules/foundation/tests/CMakeLists.txt`.
   - **Expected Red:** AC-1, AC-3, AC-4, AC-5, AC-6 and AC-7 fail on assertions. AC-2, AC-8 and the
     alias test pass already (see 10.3).
2. **Green:** implement `Error`, `toString`, the formatters and `makeUnexpected`. No test file
   changes.
3. **Refactor:** `tools/check.sh` and `ctest --preset asan`. Record the output in the Change Log.
   Update EPIC-002 and ADR-0013 (Q1).

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/foundation/include/coil/foundation/error.hpp` | create | Public API (§4.1) |
| `modules/foundation/src/error.cpp` | create | `Error`, `toString`, `makeUnexpected` |
| `modules/foundation/CMakeLists.txt` | modify | Add `src/error.cpp` |
| `modules/foundation/tests/error_test.cpp` | create | AC-1 … AC-8, rule 6 |
| `modules/foundation/tests/CMakeLists.txt` | modify | Add `error_test.cpp` |
| `docs/adr/0013-error-handling.md` | modify | Q1: one project-wide code enum |
| `docs/epics/EPIC-002-hello-window.md` | modify | Spec status |

### 10.3 Risks / alternatives considered
- **Tests that pass in Red.**
  - AC-2: the stub's empty message coincides with the expected result. It still guards that the
    message is not filled in with something else.
  - AC-8 and the alias test check declarations, which the stub must already contain to compile.
  These are stability and regression guards, not proofs of new behavior. This is noted honestly, as
  `gate.format_accepts_good` was in SPEC-0001.
- **clang-tidy and `std::formatter` specializations.** `cert-dcl58-cpp` (no additions to namespace
  `std`) may flag the specializations, although the standard explicitly allows them. If it does, I
  will stop and ask about a targeted, justified suppression rather than add `NOLINT` on my own.
- **Alternative considered:** `operator==` on `Error`. Rejected for now: comparing messages is rarely
  meaningful, and comparing only codes would make `==` surprising. Tests and callers compare
  `code()` directly.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log), except
      the guards listed in §10.3
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally; other targets once the CI matrix exists)
- [ ] Sanitizer preset passes
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `Error.AC1_StoresCodeAndMessage`
- [ ] AC-2 → `Error.AC2_MessageIsOptional`
- [ ] AC-3 → `Error.AC3_MakeUnexpectedFillsExpectedOfValue`
- [ ] AC-4 → `Error.AC4_MakeUnexpectedFillsExpectedOfVoid`
- [ ] AC-5 → `Error.AC5_ToStringNamesEveryCode`
- [ ] AC-6 → `Error.AC6_FormatsCodeAndMessage`
- [ ] AC-7 → `Error.AC7_FormatsWithoutMessageAndFormatsCode`
- [ ] AC-8 → `Error.AC8_CodesHaveStableNumericValues`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Spec and implementation plan created (Draft) | |
| 2026-11-01 | Spec and plan approved; Q1 resolved (one project-wide enum) | Emilio Pomares |
