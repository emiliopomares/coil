# ADR-0016: MCP server for agent control
Status: Accepted (2026-11-01)

## Context
Coil applications must be controllable and inspectable by AI agents from day one, using the Model
Context Protocol (MCP: JSON-RPC 2.0 with tools, resources and notifications).

## Decision
- **Layering.** `modules/mcp` contains the protocol logic as pure, testable code: JSON-RPC message in →
  messages out, mapped onto `AutomationService`, `CommandRouter`, `PropertyStore` and frame capture.
  **Transports** are adapters in `backends/`: Streamable HTTP on loopback (primary, since GUI apps are
  long-running and agents attach to them) and stdio (when the agent host launches the app). The
  supported MCP spec revision is pinned in the module, and the server negotiates during `initialize`.
- **Tools (initial set; each added by its own spec):**
  | Tool | Purpose |
  |------|---------|
  | `ui_snapshot` | Semantic tree (filtered, paginated, depth-limited) with version |
  | `ui_find` | Query nodes by id, role, name or state |
  | `ui_perform` | Perform an action (activate, set_value, focus, scroll, expand, select, drag) on a node |
  | `ui_wait_for` | Wait for a condition with a timeout (async completion) |
  | `ui_screenshot` | PNG of a window, a node's bounds or a region; optional overlay of node ids (“set-of-marks”) |
  | `commands_list` / `command_invoke` | Discover commands (with metadata) and invoke them; returns the async result or an operation id |
  | `operation_status` / `operation_cancel` | Track and cancel async operations (ADR-0014) |
  | `properties_get` / `properties_set` | Read and write bound state |
  | `workspace_list` / `workspace_apply` | Inspect and switch dock layouts |
- **Resources:** `coil://ui/tree`, `coil://windows`, `coil://commands` with subscriptions → change
  notifications driven by automation change events.
- **Threading.** The transport runs on its own I/O thread. Every UI access is marshaled through
  `UiDispatcher`, and responses complete asynchronously. The MCP layer never touches widgets directly.
- **Security (non-negotiable):**
  - **Off by default**; enabled explicitly by the application (and optionally by a CLI flag or env var
    the app chooses to honor).
  - HTTP binds to loopback only, requires a per-session bearer token, and validates `Origin`
    (DNS-rebinding protection).
  - **Capability tiers** configured by the app: `observe` (snapshot, screenshot, read properties),
    `interact` (perform, wait), `command` (invoke commands, set properties). Plus a per-command
    allowlist or denylist via command metadata (`agentExposed`).
  - Sensitive nodes are redacted in snapshots and masked in screenshots. Every agent action is
    audit-logged through the `Logger` port.
- **Implementation.** No mature official C++ MCP SDK is assumed. We implement the JSON-RPC/MCP subset we
  need behind our own interfaces, so protocol revisions only touch `modules/mcp`. Requires a JSON codec
  (nlohmann/json in `modules/json`, ADR-0018).
- **Dogfooding.** Claude Code can attach to example apps through this server to verify features
  end to end.

## Consequences
+ Agents work semantically (ids, roles, actions) with screenshots as visual confirmation.
+ The same surface is a powerful end-to-end test harness.
− A remote-control surface is security-sensitive. Defaults stay locked down, and changes to these
  rules require an ADR.
− We track MCP spec evolution ourselves.
