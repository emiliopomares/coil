# ADR-0013: Error handling and contracts
Status: Accepted (2026-11-01)

## Decision
- **Recoverable errors** (I/O, parse, plugin load, transport, invalid user data):
  `std::expected<T, coil::Error>`. `Error` = `{ErrorCode code; std::string message;}` with module-scoped
  code enums. Every such function is `[[nodiscard]]`.
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
