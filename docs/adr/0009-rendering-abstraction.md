# ADR-0009: DrawList and RenderBackend port; GPU surfaces
Status: Accepted (2026-11-01), including Amendments 1–3

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
  *Resolved by SPEC-0022:* `WindowSystem::presentImage` on the window port (SDL window surface), with
  `Image` in `foundation`; the application connects renderer and window. No new edge.
- **Vulkan, then OpenGL,** follow as their own epics, unchanged otherwise. Metal is still later.

### Consequences
+ Visible results early, fully deterministic tests and screenshots from the start, and no GPU in CI.
− CPU rendering is slow for large windows. That's acceptable for development and examples, and the
  production path remains Vulkan.
− The `DrawList` must not drift towards what is easy on a CPU. Batching-friendly design (see above)
  still applies.

## Amendment 2 (2026-11-02): closed drawing vocabulary; gradients
Status: Accepted (2026-11-02)

### Context
Custom widgets (ADR-0005 Amendment 1) need to draw. Letting them add their own `DrawList` command
types would require code for each renderer (software, Vulkan, OpenGL), break the recording backend,
frame capture and golden tests, and leave remote frontends unable to draw them.

### Decision
- **The `DrawList` vocabulary is closed to plugins.** Widgets, built-in or custom, draw with `Painter`
  primitives only. Pixels a widget computes itself are drawn as an image (`TextureId`). Rendering that
  needs the GPU directly uses a `GpuSurface`.
- **The vocabulary grows for everyone.** A missing primitive is added to the `DrawList` through a
  spec, implemented by every backend, and available to every widget. Letting plugins define their own
  commands (for example, custom shaders) needs a new ADR.
- **Gradients join the vocabulary:** linear, radial and sweep (conic) gradient fills, with at least
  two colour stops. Sweep gradients were added on 2026-11-02, for colour wheels.
  They first ship in the software renderer (EPIC-006).

### Consequences
+ Every widget works on every backend, in tests, in captures and in remote frontends.
− Effects outside the vocabulary must wait until it is extended, or use an image or a `GpuSurface`.

## Amendment 3 (2026-11-07): `render` may depend on `tasks`
Status: Accepted (2026-11-07)

### Context
ADR-0017 defines `RenderBackend::capture` as returning `AsyncResult<Image>`, so that GPU backends can
read pixels back after present without stalling a frame. `AsyncResult` lives in `tasks` (ADR-0014,
SPEC-0024), and `tools/layering.json` lets `render` depend only on `foundation`. The edge
`render → tasks` creates no cycle, but it is a new path through the layers (CLAUDE.md §1).

### Decision
- **`render` may depend on `tasks`** (and through it on `foundation` only). `tools/layering.json`
  gains `"render": ["foundation", "tasks"]`.
- `render` uses `tasks` only for asynchronous results in its ports (`capture` first). It does not
  schedule work, own threads or call `UiDispatcher`: completing a result is the backend's business,
  and continuations stay on the UI thread as ADR-0014 requires.
- Backends of `render` may include `tasks` headers for the types that appear in `render`'s ports
  (include what you use). Their edge to `tasks` changes no layer order, so it is added, with user
  approval, by the spec that needs it (CLAUDE.md §1).

### Consequences
+ `capture` has the shape ADR-0017 promised from the first backend, so GPU backends add readback
  without changing the port or the contract suite.
− `render` is no longer a leaf over `foundation`. `tasks` must stay small and free of UI concepts,
  or this edge would pull them into rendering.
