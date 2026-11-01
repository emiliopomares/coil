# ADR-0018: Third-party dependencies
Status: Accepted (2026-11-01)

## Decision
All dependencies come from vcpkg (manifest mode, pinned baseline). Each is **confined** to the listed
location. Except where noted, none appears in a Coil public header, so each can be replaced by
changing one place.

| Dependency | Purpose | Allowed in | Notes |
|------------|---------|------------|-------|
| GoogleTest / GoogleMock | Tests | `*/tests/` | ADR-0003 |
| SDL3 | Windows, input devices, clipboard, IME | `backends/platform_sdl3` | O-3 |
| Vulkan headers, volk, Vulkan Memory Allocator | Vulkan renderer | `backends/render_vulkan` | MoltenVK on macOS (O-4) |
| OpenGL loader (glad) | OpenGL renderer (second backend) | `backends/render_opengl` | |
| FreeType + HarfBuzz | Font rasterization and text shaping | `backends/text_freetype` | O-2 |
| msgpack-cxx | Remote wire encoding | `modules/remote` (private) | O-1 |
| nlohmann/json | JSON (MCP, workspaces) | `modules/json` (private) | O-8. Public API is `Value` ↔ JSON only |
| libspng (+ zlib) | PNG encoding for captures | `backends/image_spng` | O-9 |
| cpp-httplib | MCP Streamable HTTP transport (loopback) | `backends/mcp_http` | O-10. No TLS needed |

Adding, removing or relocating a dependency requires amending this ADR.

## Consequences
+ The core stays dependency-free. Heavy libraries are isolated and swappable.
− `modules/json` is a new small module (depends on foundation), so OVERVIEW §3 is amended.
