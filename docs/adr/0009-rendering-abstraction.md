# ADR-0009: DrawList and RenderBackend port; GPU surfaces
Status: Accepted (2026-11-01), including Amendment 1

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

## Amendment 1 (2026-11-01): software backend first
Status: Accepted (2026-11-01)

### Context
The original order put Vulkan first and treated `render_software` only as a test reference
(ADR-0017). Getting the first pixels on screen through Vulkan needs a GPU stack (MoltenVK on macOS),
swapchain handling and shader tooling before anything can be seen. None of that is needed to prove
the `DrawList` vocabulary, layout, widgets and text.

### Decision
- **`render_software` is the first real backend.** It is a deterministic CPU rasterizer of the
  `DrawList`, and it is also the reference renderer and screenshot source (ADR-0017).
- Its frames are **shown in OS windows** through a presentation adapter. The mechanism (for example,
  SDL window surfaces or a streaming texture) and its place in the layering are decided by the first
  spec that needs it (EPIC-006), with an ADR amendment if it adds a dependency edge.
- **Vulkan, then OpenGL,** follow as their own epics, unchanged otherwise. Metal is still later.

### Consequences
+ Visible results early, fully deterministic tests and screenshots from the start, and no GPU in CI.
− CPU rendering is slow for large windows. That's acceptable for development and examples, and the
  production path remains Vulkan.
− The `DrawList` must not drift towards what is easy on a CPU. Batching-friendly design (see above)
  still applies.
