# Coil Architecture Overview

> Status: **Accepted** (2026-11-01). Each section links to the ADR that records the decision. Changes
> require a superseding ADR.

## 1. Goals and constraints

| Goal | Consequence for the architecture |
|------|----------------------------------|
| Foundation for a Unity/Unreal-style editor | Retained widget tree, docking, serializable workspaces, high widget counts |
| UI logic decoupled from app logic; can run as a remote frontend | Commands out / state in, via a router with in-process or remote handlers (ADR-0008) |
| Never assume keyboard or mouse | Devices → input mapping → semantic intents (ADR-0007) |
| Renderer agnostic (Vulkan, OpenGL, Metal, …); embeddable GPU viewports | Backend-agnostic `DrawList` and a `RenderBackend` port (ADR-0009) |
| Extensible with new widgets and panels via DLLs | Type registries plus native C++ plugins guarded by a build fingerprint (ADR-0011) |
| Multi-core and GPU workloads | UI-thread affinity, task-scheduler port, immutable frame snapshots (ADR-0010) |
| Windows/macOS/Linux × x64/arm64 | Portable core; platform code only in adapters; CI matrix (ADR-0001, ADR-0002) |
| Production quality | SDD+TDD, lint, sanitizers, fakes for every port (ADR-0003) |
| Handlers can do slow work (HTTPS, disk, remote) | All command handlers async, cancellable, UI-thread continuations (ADR-0014) |
| Controllable by AI agents from day one | Mandatory semantic automation tree (ADR-0015) + MCP server (ADR-0016) + frame capture (ADR-0017) |

## 2. Architectural pattern: layered hexagonal (ports & adapters)

```
            ┌───────────────────────────────────────────────────────────┐
            │                      Application / Editor                 │
            │   (uses coil::app to compose everything, registers         │
            │    command handlers, binds properties, loads plugins)      │
            └───────────────┬───────────────────────────────▲───────────┘
                            │ composition root               │ Commands
            ┌───────────────▼───────────────────────────────┴───────────┐
            │  mcp · docking · widgets · plugin host · remote             │  ← library modules
            │  automation (semantic tree, actions, waitFor)              │
            │  ───────────────────────────────────────────────────────  │    (portable C++)
            │  core: widget tree, layout, intents, focus, commands,      │
            │        properties, styling, painter                       │
            │  ───────────────────────────────────────────────────────  │
            │  ports: platform (Window, InputSource, Clipboard, Clock)   │
            │         render (DrawList, RenderBackend, GpuSurface)       │
            │         remote (Transport) · tasks (TaskScheduler)         │
            │  ───────────────────────────────────────────────────────  │
            │  foundation: Error/expected, math, ids, handles, Value,    │
            │              logging interface, assertions                │
            └───────────────▲───────────────────────────────▲───────────┘
                            │ implements                     │ implements
            ┌───────────────┴──────────┐      ┌─────────────┴─────────────┐
            │ backends/platform_sdl3    │      │ backends/render_vulkan    │
            │ backends/transport_socket │      │ backends/render_opengl    │
            │ backends/mcp_http, _stdio │      │ backends/render_software  │
            └───────────────────────────┘      └───────────────────────────┘
            Test fakes (FakeWindowSystem, RecordingRenderBackend, …) are not backends:
            they live in modules/<m>/testing/ (see §3).
```

**Core idea:** everything that touches the outside world (OS, windows, input hardware, GPU, sockets,
threads, files, time) is a **port**, an abstract interface owned by the library. **Adapters** in
`backends/` implement ports. The core can always run headless with fake adapters, which is what makes
strict TDD practical for a UI library.

## 3. Modules and allowed dependencies

| Module | Responsibility | May depend on |
|--------|----------------|---------------|
| `foundation` | `Error`, `std::expected` helpers, math (`Vec2`, `Rect`, `Color`), `Handle<T>`, `Value`, logging port, assertions | — |
| `platform` | Ports: `WindowSystem`, `Window`, `InputSource`, `Clipboard`, `Clock`, `CursorService` | foundation |
| `render` | `DrawList`, `Painter` primitives, `RenderBackend` port, `GpuSurface` port, font/image resource ids | foundation |
| `tasks` | `TaskScheduler` port, `UiDispatcher`, `AsyncResult<T>`/`AsyncPromise<T>`, `Task<T>` coroutine type | foundation |
| `core` | Widget base, tree, `WidgetHandle`, layout (measure/arrange), input mapping → intents, focus, hit testing, `CommandRouter`, `PropertyStore`, styling, type registry | foundation, platform, render, tasks |
| `widgets` | Standard widgets: Button, Label, Panel, Box/Grid/Stack layouts, Slider, ScrollView, Viewport (GPU surface), … | core |
| `docking` | Dock model (pure data), dock view, panel factory registry, workspace serialization | core, widgets, automation, json |
| `automation` | `AutomationService`: semantic tree snapshots, change events, `perform` (via virtual input device), `waitFor` | core |
| `mcp` | MCP/JSON-RPC protocol logic mapped onto automation, commands, properties and capture; `McpTransport` port | automation, core, json |
| `remote` | Remote command handler, remote property sync, wire protocol, `Transport` port | core |
| `plugin` | Plugin host: discover, fingerprint check, load, register; `coil_add_plugin` CMake helper | core |
| `app` | Composition root: main loop, wiring adapters to ports, default setup | all modules (not backends) |
| `backends/*` | Adapters (SDL3, Vulkan, OpenGL, sockets, fakes) | their port module(s) + foundation + third-party |
| `json` | `Value` ↔ JSON codec (nlohmann/json kept private) | foundation |

Rules:
- No cycles. A module may include headers only of itself, of the modules in its "may depend on"
  column (**direct** edges only, so no layer can be bypassed), and of `foundation`, which every module
  and backend may include. **[tools/layering.json](../../tools/layering.json) is the authoritative copy
  of this column**; the table mirrors it and both change together.
- Backends never depend on `core`, `widgets` or `docking`; they see only ports.
- **Backends** (SPEC-0005): `backends/<name>/` is built with `coil_add_backend` as `coil::<name>`. Its
  public header (`include/coil/<name>/`) exposes only a factory returning the port and options, never
  third-party types, and third-party libraries are linked `PRIVATE`. Each backend's tests instantiate
  the contract suites of the ports it implements.
- **Test support** (SPEC-0003): `modules/<m>/testing/` builds `coil::<m>_testing` (fakes of `<m>`'s
  ports; may depend on `coil::<m>`). Reusable **contract suites** live in `modules/<m>/tests/contract/`
  and are exposed as the INTERFACE target `coil::<m>_contract`. Only test executables may link either;
  adapters' tests instantiate the contract suites for their implementation.
- Executables (examples, the editor) choose adapters at composition time. The library never names a
  concrete backend.
- Enforcement: `tools/check_layering.py` (SPEC-0004) checks every `#include` in `modules/` and
  `backends/` and runs in `tools/check.sh`; CMake `target_link_libraries` mirrors the same edges.
  Configured headers keep their `.hpp.in` template at the include path they are generated to.

## 4. Key runtime flows

### 4.1 Input → intent → widget → command
```
InputSource (adapter) ──RawInputEvent──► InputMapper ──Intent──► FocusManager / HitTester
                                         (bindings)               │
                                                                  ▼
                                                    Widget::onIntent(Intent) → Reply{handled?}
                                                                  │
                                                     emits Command{id, args: Value}
                                                                  ▼
                                    CommandRouter ──► in-process handler  ─┐  every handler returns
                                                  └─► RemoteCommandHandler ┘  AsyncResult<CommandResult>
                                                                  │
                         completion (any thread) ──► UiDispatcher ──► continuation on UI thread
                         router publishes commands.<id>.running / .progress / .lastError → PropertyStore
```
- A raw event is a device fact (`device=Gamepad#0, control=SouthButton, pressed`). An intent is the
  UI meaning (`Activate`, `Navigate{Left}`, `PointerMove{pointerId, pos}`, `Scroll{delta}`,
  `TextInput{utf8}`, `Cancel`, `ContextMenu`, …).
- Pointers are generic: mouse, touch, pen, or a gamepad virtual cursor, each with a `PointerId`.
- Bubbling: intents route focused/hit widget → ancestors until handled (Slate-style `Reply`).

### 4.2 State binding (state in)
`PropertyStore` holds typed values keyed by path (`"scene.selection.count"`). Widgets bind to paths
and get change notifications on the UI thread. The application, or a remote process via the
`remote` module, writes to the store. Widgets never poll application objects.

### 4.3 Frame
```
UI thread:  drain UiDispatcher queue → process intents → update animations
            → layout (measure/arrange, only dirty subtrees) → paint → DrawList snapshot
Render:     RenderBackend::submit(windowId, const DrawList&)   (same thread initially; may move)
```

### 4.4 Docking and workspaces
- `DockModel` is pure data: a tree of splits, tab stacks and floating windows that reference
  `PanelInstanceId` + `PanelTypeId`. It is serializable to versioned JSON. That is a **workspace**.
- `DockView` builds widgets from the model through the `PanelFactory` registry, so plugin panels can
  be restored.
- Detaching a tab is a model operation (move node to a floating node). The view asks `WindowSystem`
  for a new OS window. Every docking behavior can be tested on the model without any window.

### 4.5 Agent control (MCP)
```
Agent ──MCP (HTTP loopback / stdio)──► mcp_* adapter (I/O thread) ──► modules/mcp (protocol)
          │                                                   │ marshaled via UiDispatcher
          │                                                   ▼
          │      AutomationService ─ snapshot / find / perform (intents via virtual device) / waitFor
          │      CommandRouter     ─ command_invoke → AsyncResult → operation_status
          │      PropertyStore     ─ properties_get/set
          ◄──────RenderBackend::capture → ImageEncoder → PNG (sensitive nodes masked)
```
Off by default, loopback only, token-authenticated, with capability tiers set by the application (ADR-0016).

## 5. Resolved decisions (formerly open)

| # | Topic | Decision |
|---|-------|----------|
| O-1 | Remote wire encoding | MessagePack |
| O-2 | Text shaping and rasterization | FreeType + HarfBuzz |
| O-3 | First platform adapter | SDL3 |
| O-4 | First render backend | Vulkan (MoltenVK on macOS), then OpenGL; native Metal later |
| O-5 | Plugin ABI | Native C++ with build fingerprint (ADR-0011) |
| O-6 | Accessibility | Semantic automation tree now (ADR-0015); platform bridges later |
| O-7 | License | MIT ([LICENSE](../../LICENSE)) |
| O-8 | JSON codec | nlohmann/json, private to `modules/json` |
| O-9 | PNG encoder | libspng |
| O-10 | MCP HTTP transport | cpp-httplib |

Dependency placement: ADR-0018.
