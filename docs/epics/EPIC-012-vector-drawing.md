# EPIC-012: Vector drawing

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-02 |
| Origin | "does the system allow … node-based editors (with curves joining the nodes, etc.)?" (2026-11-02) |

## Goal
Widgets can draw curves, stroked shapes, transformed content and images. This is what a node editor
(curved links, pan and zoom) and image-based widgets (thumbnails, computed colour fields) need beyond
EPIC-006's rectangles and gradients.

## Done when
- The `DrawList` has paths (lines, quadratic and cubic Bézier curves, closed shapes), filled and
  stroked with width, caps and joins.
- The `DrawList` has transform push/pop (translate and uniform scale at least), used for pan and zoom.
- Images can be uploaded as textures and drawn with `drawImage(TextureId, Rect)`.
- The software renderer draws all of them, anti-aliased, proven by golden images.
- An example shows two boxes joined by a Bézier curve, panned and zoomed by a transform (manual check
  plus a golden test).

## Out of scope
- Rotation and arbitrary affine transforms beyond what pan and zoom need (later, if a use case needs it).
- Dashed strokes, blur, shadows, shaders (ADR-0009 Amendment 2: shaders need an ADR).
- Image decoding (PNG, JPEG): images come in as raw RGBA.
- Hit testing through transforms and on curves (EPIC-008 spec 5 provides custom hit shapes; mapping
  through transforms is specified with the widget that uses them).
- GPU backends.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | Paths in the `DrawList`: lines, quadratic and cubic Béziers, fill and stroke styles | Planned | EPIC-006 spec 1 |
| 2 | Software rasterization of paths: anti-aliased fill and stroke; golden tests | Planned | 1; EPIC-006 spec 3 |
| 3 | Transform push/pop (translate, uniform scale) in the `DrawList` and the software renderer | Planned | EPIC-006 spec 3 |
| 4 | Textures and `drawImage`: upload raw RGBA, draw scaled into a rect | Planned | EPIC-006 spec 3 |
| 5 | Example: two boxes joined by a curve, with pan and zoom (manual check, golden test) | Planned | 2, 3 |

## Notes
- ADR-0009 already lists paths, images and transforms in the vocabulary. This epic implements them.
- Reference use cases: node editor (curves, pan and zoom), colour wheel (`drawImage` as an alternative
  to sweep gradients).
- Not on the critical path to the button (Milestone 2). It can run any time after EPIC-006.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Epic proposed | |
