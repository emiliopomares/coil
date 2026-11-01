# ADR-0008: Commands out, state in (local or remote)
Status: Accepted (2026-11-01)

## Context
A button press might be handled by code compiled with the UI, or by another process (Coil as a
frontend). Widget code must not depend on which.

## Decision
- **Commands out.** Widgets emit `Command{CommandId id; Value args;}`, where `CommandId` is a stable
  string such as `"scene.save"` and `Value` is foundation's serializable variant (null, bool, int64,
  double, string, bytes, array, map, plus vectors and colors). The **`CommandRouter`** dispatches to
  registered `CommandHandler`s:
  - `LocalCommandHandler`: in-process callable;
  - `RemoteCommandHandler` (remote module): serializes over a `Transport` port.
  All handlers are asynchronous (ADR-0014): they return `AsyncResult<CommandResult>`, completion is
  delivered on the UI thread, and the UI never blocks on a handler.
- **Commands carry metadata** (label, enabled-state property path, default bindings), so menus,
  toolbars and shortcuts are built from the same command.
- **State in.** `PropertyStore` holds typed values keyed by path. Widgets bind to paths. Writers are
  local application code or the remote module applying `PropertyChanged` messages. Change
  notifications are delivered on the UI thread.
- **Remote protocol** (remote module): versioned handshake, then `InvokeCommand`, `CommandResult`,
  `SetProperty`, `PropertyChanged`, and later `CreateWidget`/`DestroyWidget` for remotely built UIs.
  Transports are adapters: TCP, Unix domain socket or named pipe, and in-memory `LoopbackTransport`
  for tests. Encoding: MessagePack (ADR-0018).

## Consequences
+ The same UI can serve as an in-process editor or a frontend for an external engine or tool.
+ Undo/redo, macro recording and scripting can be added later as router middleware.
− Stringly-typed ids need validation. A command/property registry rejects unknown ids in debug builds.
