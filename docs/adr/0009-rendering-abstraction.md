# ADR-0009: DrawList and RenderBackend port; GPU surfaces
Status: Accepted (2026-11-01)

## Decision
- Widgets paint through a **`Painter`** that records into a backend-agnostic **`DrawList`**: filled and
  rounded rects, borders, lines/paths, images (by `TextureId`), glyph runs (by `FontId` + positioned
  glyphs), clip push/pop, transforms, and `GpuSurface` placeholders. The DrawList is plain data,
  immutable once submitted, and comparable in tests.
- **`RenderBackend` port**: `createWindowTarget`, `uploadTexture`, `submit(WindowTargetId, const
  DrawList&)`, `present`, `capture` (ADR-0017).
  Backends: `render_recording` (tests), `render_software` (reference), then Vulkan and OpenGL; Metal later
  (Vulkan first, ADR-0018).
- **`GpuSurface` / `Viewport` widget**: embeds user GPU rendering (scene views, compute output). The
  backend allocates an offscreen render target; user code receives backend-native handles via a typed
  query (`surface.nativeHandles<VulkanHandles>()`) and renders on its own schedule. The UI composites
  the result. **One GPU API per window**; cross-API interop is out of scope until an ADR covers it.
- Text: shaping and rasterization sit behind a `TextShaper`/`FontRasterizer` port (FreeType + HarfBuzz, ADR-0018).
  The core only sees shaped glyph runs and metrics.

## Consequences
+ Adding a backend touches no widget code. Paint output is unit-testable via the DrawList.
− The DrawList vocabulary must be designed with batching in mind. Changes to it are ABI-relevant for
  backends.
