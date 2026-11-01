# ADR-0004: Layered hexagonal architecture (ports & adapters)
Status: Accepted (2026-11-01)

## Context
Coil must support several OSes, windowing libraries, GPU APIs, input devices and IPC transports, and
must stay testable. Each of these external technologies changes at its own pace.

## Decision
- Library code in `modules/` is organized in layers: foundation → ports (platform, render, tasks) →
  core → widgets/docking/remote/plugin → app. Dependencies point only toward foundation (OVERVIEW §3).
- All interaction with the outside world goes through **ports**: abstract interfaces defined by the
  library in the module that consumes them. **Adapters** in `backends/` implement ports and are the
  only code allowed to include OS or third-party API headers.
- The **composition root** (`coil::app` or the user's `main`) creates adapters and injects them. Library
  code has no singletons and no global state.
- Adding an inter-module dependency edge requires an ADR. Enforcement: CMake links plus
  `tools/check_layering.py`.

## Consequences
+ Backends can be swapped (SDL3↔native, Vulkan↔GL) without touching widgets.
+ The core can be tested headlessly.
− More interfaces and indirection. Virtual calls happen at port boundaries only (coarse-grained:
  per frame or per batch, never per pixel).
