# EPIC-006: First pixels

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01): "software rendering first" (ADR-0009 Amendment 1) |

## Goal
Coil draws inside its windows. Layout results are painted as rectangles through a backend-agnostic
`DrawList`, rasterized in software, and shown sharp on screen at the display's pixel scale. The same
frames can be captured as images for tests and agents.

## Done when
- The `render` module has a `DrawList` with filled rects, rounded rects, borders and clip push/pop,
  a `Painter` that records into it, and a `Color` type.
- A `RenderBackend` port, a `render_recording` backend for tests, and a contract suite exist.
- `render_software` rasterizes the `DrawList` into an RGBA image, deterministically, proven by golden
  images.
- Software frames are presented in SDL windows at the correct pixel scale (EPIC-005 spec 3).
- A frame can be captured as an image (ADR-0017, without PNG encoding).
- **Milestone 1:** an example draws coloured boxes arranged by rows and columns (EPIC-004) that
  re-layout and redraw as the window is resized. Recorded manual check plus a golden-image test.

## Out of scope
- Text and glyph runs (EPIC-010), images and textures, paths and lines, transforms, gradients.
- Vulkan, OpenGL and Metal backends (later epics).
- Frame scheduling beyond "redraw when something changed" (EPIC-003).
- PNG encoding (`ImageEncoder`, with the MCP work).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | `render` module: `Color`, `DrawList`, `Painter` (rects, rounded rects, borders, clips) | Planned | EPIC-004 spec 1 (geometry) |
| 2 | `RenderBackend` port, `render_recording` backend, contract suite | Planned | 1 |
| 3 | `render_software`: rasterize rects, rounded rects, borders and clips into an image; golden tests | Planned | 2 |
| 4 | Presenting software frames in SDL windows at the display's pixel scale | Planned | 3; EPIC-005 spec 3 |
| 5 | Frame capture to an image (`capture`) | Planned | 3 |
| 6 | Example: coloured boxes laid out by rows and columns, redrawn on resize | Planned | 4; EPIC-004 |

## Notes
- ADR-0009 Amendment 1 (software first). ADR-0017 (capture, golden images). ADR-0006 and ADR-0019
  (DIPs; pixel conversion only in backends).
- **Decided in spec 4:** the presentation mechanism (SDL window surface or a streaming texture) and
  where it sits in the layering. It may need an ADR amendment for a backend-to-backend edge.
- **Decided in spec 3:** anti-aliasing of rounded corners, the colour space (sRGB storage, blending
  rules), and the golden-image tolerance.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
