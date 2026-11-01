# ADR-0013: Error handling and contracts
Status: Accepted (2026-11-01)

## Decision
- **Recoverable errors** (I/O, parse, plugin load, transport, invalid user data):
  `coil::Expected<T>` (= `std::expected<T, coil::Error>`). `Error` holds an `ErrorCode` and an optional
  human-readable message. Every such function is `[[nodiscard]]`.
- **One project-wide `ErrorCode` enum** (amended 2026-11-01 by SPEC-0002; originally "module-scoped code
  enums"). Codes describe the *kind* of failure, not where it happened. Numeric values are stable and
  never reused (they cross process boundaries); new codes are appended. The underlying type is
  `std::uint16_t` for wire-format headroom. Program logic never branches on the message.
- **Programmer errors** (violated preconditions, broken invariants): `COIL_EXPECTS(cond)`,
  `COIL_ENSURES(cond)`, `COIL_ASSERT(cond)`. Active in Debug and `checked` builds; they call a
  configurable failure handler (default: log + abort). Death tests check them.
- **No exceptions thrown by Coil code.** The library still compiles with exceptions enabled, so
  applications may use them, but none propagate through Coil APIs or callbacks into Coil. Exceptions
  escaping a user callback are a contract violation, caught at the boundary in checked builds.
- **Logging** goes through a `Logger` port (no direct `stdout`/`stderr` in library code).

## Consequences
+ Explicit, testable error paths that stay ABI-friendly.
− Some verbosity; mitigated by helper macros or functions for propagation.
