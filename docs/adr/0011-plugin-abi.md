# ADR-0011: Native C++ plugins with a build fingerprint
Status: Accepted (2026-11-01)

## Context
C++ has no stable binary interface (ABI). Two binaries can only safely exchange C++ objects
(`std::string`, `std::unique_ptr`, virtual classes, exceptions) if they were built with the same
compiler, standard library and settings. Two options were considered:

| | **A. Native C++ plugins (chosen)** | B. C ABI "hourglass" |
|---|---|---|
| Plugin author writes | Ordinary C++ against Coil headers: subclass `Widget`, register it | C++ against an SDK that wraps a C function-table boundary |
| Effort for us | Low: the plugin API *is* the normal API | High: every extensible concept needs a C mirror plus C++ wrappers on both sides |
| Plugins built with a different compiler or settings | **Rejected at load** with a clear error | Work |
| Third-party binary distribution | Must ship builds per platform and toolchain (as Unreal does) | One binary per platform |
| Debugging | Normal | Through the C boundary |

Coil's primary consumer is our own engine and editor, where host and plugins are built together. The
added cost of B buys little until third parties ship closed binaries.

## Decision
- Plugins are shared libraries built **with the same toolchain and configuration as the host**, and they
  use the regular Coil C++ API. Plugins require Coil to be built as shared libraries (one copy of
  registries in the process). On Windows, the dynamic CRT (`/MD`, `/MDd`) is mandatory.
- **Two exported C symbols** (unmangled, so they can be looked up):
  - `const CoilBuildFingerprint* coil_plugin_fingerprint()`: a plain C struct with Coil version, compiler
    id and version, standard library id and ABI version, architecture, build configuration (debug or
    release, iterator debug level), C++ standard, and exceptions/RTTI flags. The host compares it with its
    own fingerprint before touching anything else. **On mismatch, the host refuses to load the plugin and
    returns a descriptive `Error`.** It never "tries anyway."
  - `coil::Plugin* coil_plugin_create()`: returns the plugin object. `Plugin` is an abstract interface:
    `info()`, `onLoad(PluginRegistry&)`, `onUnload()`.
- **`PluginRegistry`** lets a plugin register widget types, panel types, commands (async handlers),
  intents, input bindings and automation roles. Built-in widgets use the same registry.
- **Build integration:** `find_package(Coil)` plus a `coil_add_plugin(name …)` CMake helper that applies
  the host's flags automatically, so matching fingerprints is the default outcome.
- **Boundary hygiene** (keeps option B possible later): no exceptions thrown across `onLoad` or handler
  calls; objects created by a plugin are destroyed through the plugin's own factories or deleters;
  plugin-facing types live in a curated `coil/plugin/` header set.
- **Lifecycle:** discover → fingerprint check → create → `onLoad` → … → `onUnload` → unload at shutdown.
  No hot unloading initially.

## Consequences
+ Plugins are as easy to write as built-in widgets, with minimal extra code for us.
− Binary plugins only work with an identically built host. A C ABI tier for third-party binary
  distribution can be added later in a new ADR without breaking this one.
