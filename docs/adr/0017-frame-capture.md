# ADR-0017: Frame capture (screenshots) and the software reference renderer
Status: Accepted (2026-11-01)

## Decision
- **`RenderBackend` gains a capture capability:** `capture(WindowTargetId, optional<Rect> region) →
  AsyncResult<Image>` (RGBA8, sRGB, with its pixel scale). GPU backends implement it via asynchronous
  readback after present. They never stall the frame.
- **`GpuSurface` content is included** in captures. User renderers must render into Coil-provided
  targets (already required by ADR-0009) and must not draw directly to the swapchain.
- **Sensitive masking** happens in capture, not by the caller: nodes flagged `sensitive` (ADR-0015) are
  filled over before encoding.
- **Optional overlay** of automation ids and bounds ("set-of-marks") is drawn on the captured image, never
  into the live UI.
- **`render_software` backend**: a deterministic CPU rasterizer of the `DrawList`. It lets headless CI and
  unit or integration tests produce real screenshots, and it is the reference for golden-image tests.
  Not optimized for production use.
- **Image encoding** (PNG) lives in an adapter behind an `ImageEncoder` port. Library: libspng (ADR-0018).

## Consequences
+ Screenshots are available to agents and tests on every platform, including CI without a GPU.
− A second rasterizer must stay consistent with GPU backends (golden tests with a tolerance).
