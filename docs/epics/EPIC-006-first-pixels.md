# EPIC-006: First pixels

| Field | Value |
|-------|-------|
| Status | In Progress |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01): "software rendering first" (ADR-0009 Amendment 1) |

## Goal
Coil draws inside its windows. Layout results are painted as rectangles through a backend-agnostic
`DrawList`, rasterized in software, and shown sharp on screen at the display's pixel scale. The same
frames can be captured as images for tests and agents.

## Done when
- The `render` module has a `DrawList` with filled rects, rounded rects, borders, linear, radial and
  sweep gradient fills (ADR-0009 Amendment 2) and clip push/pop, a `Painter` that records into it, and a
  `Color` type.
- A `RenderBackend` port, a `render_recording` backend for tests, and a contract suite exist.
- `render_software` rasterizes the `DrawList` into an RGBA image, deterministically, proven by golden
  images.
- Software frames are presented in SDL windows at the correct pixel scale (EPIC-005 spec 3).
- A frame can be captured as an image (ADR-0017, without PNG encoding).
- **Milestone 1:** an example draws coloured boxes arranged by rows and columns (EPIC-004) that
  re-layout and redraw as the window is resized. Recorded manual check plus a golden-image test.

## Out of scope
- Text and glyph runs (EPIC-010), images and textures, paths and lines, transforms.
- Vulkan, OpenGL and Metal backends (later epics).
- Frame scheduling beyond "redraw when something changed" (EPIC-003).
- PNG encoding (`ImageEncoder`, with the MCP work).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | [SPEC-0019 `render` module: `Color`, `DrawList`, `Painter` (rects, rounded rects, borders, gradients, clips)](../specs/0019-render-drawlist-and-painter.md) | Done | EPIC-004 spec 1 (geometry) |
| 2 | [SPEC-0020 `RenderBackend` port, recording backend, contract suite](../specs/0020-render-backend-port.md) | Done | 1 |
| 3a | [SPEC-0021 `render_software`: rects, rounded rects, borders and clips into an image](../specs/0021-software-rasterizer-shapes.md) | Done | 2 |
| 3b | `render_software`: linear, radial and sweep gradients | Planned | 3a |
| 4 | [SPEC-0022 Presenting images in windows](../specs/0022-present-images-in-windows.md) | Done | 3a; EPIC-005 spec 3 |
| 5 | Frame capture to an image (`capture`) | Planned | 3 |
| 6 | [SPEC-0023 `boxes` example: coloured boxes laid out and redrawn on resize (Milestone 1)](../specs/0023-boxes-example.md) | In Progress | 4; EPIC-004 |

## Notes
- ADR-0009 Amendment 1 (software first). ADR-0017 (capture, golden images). ADR-0006 and ADR-0019
  (DIPs; pixel conversion only in backends).
- **Decided in spec 4 (SPEC-0022):** `WindowSystem::presentImage` on the window port, SDL window
  surface; `Image` moves to `foundation`. No new layering edge.
- **Order after spec 4:** spec 6 (Milestone 1), then spec 5 (needed by EPIC-013). Spec 3b waits until
  after Milestone 2 (roadmap, 2026-11-07).
- **Golden images:** spec 6 checks exact pixel probes; the stored golden image comes with spec 5
  (SPEC-0023 Q5).
- **Decided in spec 3:** anti-aliasing of rounded corners, the colour space (sRGB storage, blending
  rules), gradient interpolation (sRGB or linear light), and the golden-image tolerance.
- **Decided in spec 1:** the gradient model: number of colour stops, and whether a gradient is a
  fill style for every shape or its own command. If spec 1 or 3 grows too large, gradients split into
  their own spec.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
| 2026-11-02 | Linear and radial gradient fills added to specs 1 and 3 (ADR-0009 Amendment 2) | Emilio Pomares |
| 2026-11-02 | Sweep (conic) gradients added, for colour wheels (reference use case) | Emilio Pomares |
| 2026-11-02 | Epic approved; started after EPIC-004 spec 4 (rows and columns are enough for Milestone 1) | Emilio Pomares |
| 2026-11-02 | Spec 3 split into 3a (shapes, borders, clips) and 3b (gradients) (SPEC-0021 Q1); spec 4 needs only 3a | Emilio Pomares |
| 2026-11-05 | Spec 4 is SPEC-0022; spec 6 to be done before specs 5 and 3b (SPEC-0022 Q6) | Emilio Pomares |
| 2026-11-07 | Spec 6 is SPEC-0023; golden image file deferred to spec 5; spec 3b after Milestone 2 | Emilio Pomares |
