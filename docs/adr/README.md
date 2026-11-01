# Architecture Decision Records

Each ADR records one decision: context, decision, consequences. Statuses: `Proposed` → `Accepted`
(by the user) → optionally `Superseded by ADR-XXXX`. Accepted ADRs are binding. To change one, write
a new ADR that supersedes it.

| ADR | Title | Status |
|-----|-------|--------|
| [0001](0001-language-and-toolchain.md) | Language, build system, dependencies | Accepted (incl. Amendment 1) |
| [0002](0002-quality-gates-and-ci.md) | Lint, warnings, sanitizers, CI matrix | Accepted |
| [0003](0003-testing-strategy.md) | Testing framework and strategy | Accepted |
| [0004](0004-layered-hexagonal-architecture.md) | Layered hexagonal architecture | Accepted |
| [0005](0005-retained-widget-tree.md) | Retained widget tree, ownership, handles, registry | Accepted |
| [0006](0006-layout-model.md) | Two-pass layout in logical units | Accepted |
| [0007](0007-device-agnostic-input.md) | Device-agnostic input via intents | Accepted |
| [0008](0008-commands-out-state-in.md) | Commands out, state in (local or remote) | Accepted |
| [0009](0009-rendering-abstraction.md) | DrawList and RenderBackend port; GPU surfaces | Accepted |
| [0010](0010-threading-model.md) | Threading model | Accepted |
| [0011](0011-plugin-abi.md) | Native C++ plugins with a build fingerprint | Accepted |
| [0012](0012-docking-and-workspaces.md) | Docking as model/view; serializable workspaces | Accepted |
| [0013](0013-error-handling.md) | Error handling and contracts | Accepted |
| [0014](0014-async-command-handling.md) | Asynchronous command handling | Accepted |
| [0015](0015-semantic-automation-tree.md) | Semantic automation tree | Accepted |
| [0016](0016-mcp-server.md) | MCP server for agent control | Accepted |
| [0017](0017-frame-capture.md) | Frame capture and software reference renderer | Accepted |
| [0018](0018-third-party-dependencies.md) | Third-party dependencies | Accepted |
