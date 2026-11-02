# EPIC-010: Text

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01): "text first" (before the button) |

## Goal
Coil shows text. Strings are shaped and rasterized behind ports (ADR-0009), with FreeType + HarfBuzz
in a backend (ADR-0018). Glyph runs flow through the `DrawList` to the software renderer, text has
measurable sizes for layout, and a `Label` widget displays it sharply at any display scale.

## Done when
- `TextShaper` and `FontRasterizer` ports exist, with a deterministic fake font (fixed advances) for
  tests.
- `backends/text_freetype` shapes UTF-8 text with HarfBuzz and rasterizes glyphs with FreeType, and
  passes the contract suite.
- The `DrawList` carries glyph runs, and the software renderer draws them through a glyph atlas
  (golden tests).
- Text measurement gives the layout engine intrinsic sizes.
- A `Label` widget (`widgets` module), a built-in primitive that uses only the public API
  (ADR-0005 Amendment 1), displays a single line of text and publishes its automation node (role
  text, name = its text). Its text can be set directly; binding it to a `Property` follows EPIC-009.
  Text that doesn't fit is clipped. Horizontal alignment is start, centre or end. Font, size and
  colour are plain parameters.
- An example shows labels on screen, crisp on Retina (manual check plus a golden test).

## Out of scope
- Text editing, selection, carets, IME, rich text, bidirectional and right-to-left layout, line
  breaking beyond a single line, font fallback chains, emoji colour fonts, ellipsis on overflow,
  styling and theming.

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | Text ports (`TextShaper`, `FontRasterizer`) and a fake fixed-advance font | Planned | EPIC-006 spec 1 |
| 2 | `text_freetype` backend: HarfBuzz shaping, FreeType rasterization, bundled default font | Planned | 1 |
| 3 | Glyph runs in the `DrawList`; glyph atlas in the software renderer | Planned | 1; EPIC-006 spec 3 |
| 4 | Text measurement for layout (intrinsic sizes) | Planned | 1; EPIC-004 |
| 5 | `Label` widget (`widgets` module): single line, clipped, aligned; its automation node | Planned | 3, 4; EPIC-008 |
| 6 | Example: labels on screen (manual check, golden test) | Planned | 5 |

## Notes
- New vcpkg dependencies: `freetype` and `harfbuzz`, already allowed by ADR-0018 (O-2) in
  `backends/text_freetype`.
- **Open question for spec 2:** which default font to bundle, and under what license (for example,
  an OFL-licensed font such as Inter or Noto Sans).
- Specs 1–4 don't need the widget tree, so they can be done before or alongside EPIC-008.

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
| 2026-11-02 | `Label` refined: built-in primitive on the public API only; clip on overflow; start/centre/end alignment; plain style parameters | Emilio Pomares |
