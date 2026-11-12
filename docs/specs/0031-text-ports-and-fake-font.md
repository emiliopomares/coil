# SPEC-0031: Text ports (`TextShaper`, `FontRasterizer`), a fake fixed-advance font and their contract suite

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-08 |
| Module(s) | `coil::render` (ports, `render_testing`, contract suite) |
| Epic | [EPIC-010](../epics/EPIC-010-text.md) (spec 1) |
| Related ADRs | ADR-0009 (text behind `TextShaper`/`FontRasterizer`), ADR-0004 (ports and adapters), ADR-0003 (fakes and contract suites), ADR-0018 (FreeType + HarfBuzz only in `backends/text_freetype`) |
| Depends on | SPEC-0019 (`render` module) |

> **Scope Gate checklist**
> - [x] One concern: the two ports through which Coil turns UTF-8 into positioned glyphs and glyphs into coverage bitmaps, with a deterministic fake that obeys the same contract
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`render`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
Coil can't show a word yet. ADR-0009 puts shaping and rasterization behind ports, so the core only
sees shaped glyph runs and metrics, and FreeType and HarfBuzz stay in one backend (ADR-0018). This
spec defines those ports, so that the layers above them can be built and tested now: glyph runs in
the `DrawList` (spec 3), text measurement (spec 4) and `Label` (spec 5). It also adds a fake font
whose every number can be worked out by hand, and a contract suite that spec 2's FreeType backend
must also pass.

## 2. Scope
- Value types: `FontId`, `GlyphId`, `FontMetrics`, `ShapedGlyph`, `ShapedRun`, `GlyphBitmap`.
- Port `TextShaper`: load a font from bytes, query its metrics at a size, shape a UTF-8 string
  into a single run of positioned glyphs.
- Port `FontRasterizer`: rasterize one glyph of a font at a pixel size into an 8-bit coverage bitmap.
- `FakeTextEngine` (in `modules/render/testing`, library `coil::render_testing`), which implements
  both ports with fonts whose glyphs all have the same advance (fixed-advance fonts) and solid-box
  glyphs.
- Contract suite (`coil::render_contract`, header-only typed tests) for any implementation of both
  ports, instantiated for the fake.

## 3. Out of Scope
- The FreeType and HarfBuzz backend and the bundled default font (spec 2).
- Glyph runs in the `DrawList`, the glyph atlas, drawing text (spec 3).
- Measurement for layout (spec 4).
- Unloading fonts; font families, styles, weights or lookup by name; fallback chains (EPIC-010
  non-goal).
- Line breaking, bidirectional text, scripts, languages and OpenType features: one run, shaped in
  logical order (left to right).
- Kerning or ligatures in the fake (each code point is one glyph). Real shapers may produce both;
  the contract allows it.
- Hinting, sub-pixel glyph positioning, LCD (coloured) anti-aliasing, colour glyphs (emoji).
- Thread safety beyond "one thread at a time" (§4.4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/render/include/coil/render/text.hpp
namespace coil::render {

/// Identifies a loaded font. Issued by the TextShaper that loaded it; never reused.
struct FontId {
    std::uint64_t value = 0;
    friend constexpr bool operator==(FontId, FontId) = default;
};

/// A glyph index inside one font. Meaningless without its FontId.
struct GlyphId {
    std::uint32_t value = 0;
    friend constexpr bool operator==(GlyphId, GlyphId) = default;
};

/// Vertical metrics of a font at one size, in DIPs. Line height = ascent + descent + lineGap.
struct FontMetrics {
    float ascent = 0.0F;  ///< Baseline to the top of the tallest glyphs (> 0).
    float descent = 0.0F; ///< Baseline down to the bottom of the lowest glyphs (>= 0).
    float lineGap = 0.0F; ///< Extra space the font asks for between lines (>= 0).
    friend constexpr bool operator==(FontMetrics, FontMetrics) = default;
};

/// One glyph of a shaped run.
struct ShapedGlyph {
    GlyphId glyph;
    /// Pen position for this glyph, relative to the run's origin on the baseline, in DIPs
    /// (x to the right, y down). Includes any offset the shaper applies.
    Point position;
    float advance = 0.0F;       ///< How far the pen moves after this glyph, in DIPs.
    std::uint32_t cluster = 0;  ///< Byte offset, in the shaped text, of the cluster's first byte.
    friend constexpr bool operator==(ShapedGlyph, ShapedGlyph) = default;
};

/// A string shaped with one font at one size.
struct ShapedRun {
    FontId font;
    float size = 0.0F;               ///< Font size (em height) in DIPs.
    std::vector<ShapedGlyph> glyphs; ///< In visual order (left to right in this spec).
    float advance = 0.0F;            ///< Total pen advance of the run, in DIPs.
    friend bool operator==(const ShapedRun&, const ShapedRun&) = default;
};

/// 8-bit coverage of one glyph at one pixel size. Row 0 is the top; 0 is empty, 255 is full.
struct GlyphBitmap {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    /// From the pen position to the bitmap's left column, in pixels (may be negative).
    std::int32_t left = 0;
    /// From the baseline up to the bitmap's top row, in pixels (FreeType's bitmap_top).
    std::int32_t top = 0;
    std::vector<std::uint8_t> coverage; ///< width * height values, row-major.
    friend bool operator==(const GlyphBitmap&, const GlyphBitmap&) = default;
};

} // namespace coil::render

// modules/render/include/coil/render/text_shaper.hpp
namespace coil::render {

/// Port: loads fonts and shapes UTF-8 text with them (ADR-0009). Implemented by
/// backends/text_freetype (HarfBuzz) and by FakeTextEngine for tests.
class TextShaper {
public:
    virtual ~TextShaper() = 0; // pure, defined in src/text.cpp
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&&) = delete;
    TextShaper& operator=(TextShaper&&) = delete;

    /// Loads a font file (TrueType or OpenType, face 0) from `data`; the shaper keeps its own
    /// copy. Fails with InvalidArgument for data it cannot read as a font.
    [[nodiscard]] virtual Expected<FontId> loadFont(std::span<const std::byte> data) = 0;
    /// The font's vertical metrics at `size` DIPs.
    [[nodiscard]] virtual Expected<FontMetrics> metrics(FontId font, float size) const = 0;
    /// Shapes `utf8` as one left-to-right run with `font` at `size` DIPs.
    [[nodiscard]] virtual Expected<ShapedRun> shape(FontId font, float size,
                                                    std::string_view utf8) const = 0;

protected:
    TextShaper() = default;
};

} // namespace coil::render

// modules/render/include/coil/render/font_rasterizer.hpp
namespace coil::render {

/// Port: turns glyphs into coverage bitmaps (ADR-0009). Used by renderers (the software
/// renderer's glyph atlas, spec 3). Accepts the FontIds of the TextShaper it is paired with.
class FontRasterizer {
public:
    virtual ~FontRasterizer() = 0;
    // copy and move deleted, as above

    /// Rasterizes `glyph` of `font` with an em height of `pixelSize` pixels.
    [[nodiscard]] virtual Expected<GlyphBitmap> rasterize(FontId font, GlyphId glyph,
                                                          float pixelSize) const = 0;

protected:
    FontRasterizer() = default;
};

} // namespace coil::render

// modules/render/testing/include/coil/render/testing/fake_text_engine.hpp
namespace coil::render::testing {

/// Shape of a fake font, in ems (multiplied by the size to get DIPs or pixels).
struct FakeFontDesc {
    float advance = 0.5F; ///< Every glyph's advance.
    float ascent = 0.8F;
    float descent = 0.2F;
    float lineGap = 0.0F;
};

/// Deterministic fake of both text ports (rules 9-12).
class FakeTextEngine final : public TextShaper, public FontRasterizer {
public:
    /// Adds a fake font. Precondition: every field is finite, advance > 0, ascent > 0,
    /// descent >= 0, lineGap >= 0.
    [[nodiscard]] FontId addFont(const FakeFontDesc& desc);

    [[nodiscard]] Expected<FontId> loadFont(std::span<const std::byte> data) override;
    [[nodiscard]] Expected<FontMetrics> metrics(FontId font, float size) const override;
    [[nodiscard]] Expected<ShapedRun> shape(FontId font, float size,
                                            std::string_view utf8) const override;
    [[nodiscard]] Expected<GlyphBitmap> rasterize(FontId font, GlyphId glyph,
                                                  float pixelSize) const override;
};

} // namespace coil::render::testing

// modules/render/tests/contract/coil/render/text_contract.hpp
namespace coil::render {

/// Specialize for each implementation under contract.
template <typename Impl>
struct TextContractTraits;
//   static std::unique_ptr<Impl> make();       // a fresh engine; Impl is a TextShaper and a FontRasterizer
//   static FontId addFont(Impl& engine);       // a usable font (the fake: addFont; FreeType: the bundled font)

// INSTANTIATE_TYPED_TEST_SUITE_P(Name, TextContract, ::testing::Types<Impl>);

} // namespace coil::render
```

### 4.2 Behavioral rules
**Both ports**
1. A `FontId` is valid from the call that issued it until the engine is destroyed; ids are never
   reused, and `FontId{0}` is never issued. A `FontRasterizer` accepts the ids of the `TextShaper` it
   is paired with. An implementation of both ports (the fake, `text_freetype`) shares one id space.
2. A size (`size`, `pixelSize`) MUST be finite and > 0.
3. Results are deterministic: the same calls on the same engine give equal results.

**`TextShaper`**
4. `metrics` scales linearly with size: `metrics(f, 2s)` is twice `metrics(f, s)`, within 1e-4 relative
   error. `ascent > 0`, `descent >= 0`, `lineGap >= 0`.
5. `shape` returns a run with `font` and `size` as given. Shaping is independent of display scale:
   positions and advances are unhinted DIPs, so layout gives the same answer at every scale.
6. Within a run, glyphs are in left-to-right order: each glyph's `position.x` is ≥ the previous
   glyph's (within 1e-4), `run.advance` is the sum of the glyphs' advances, `cluster` values are
   non-decreasing and each is the offset of a code point boundary in the text.
7. Shaping `""` gives a run with no glyphs and `advance == 0`. Every non-empty valid text gives at
   least one glyph.
8. Shaping is linear in size, like rule 4: every `position` and `advance` at `2s` is twice its value
   at `s`, within 1e-4 relative error, with the same glyphs and clusters.

**`FakeTextEngine`** (each is checked by the fake's own tests; rules 1-8 by the contract suite)
9. `addFont(desc)` issues a new id. `loadFont` fails with InvalidArgument for any data: the fake reads
   no font files.
10. `metrics(f, s)` is `{desc.ascent * s, desc.descent * s, desc.lineGap * s}`.
11. `shape(f, s, text)` gives one glyph per code point, in order: `glyph` is the code point,
    `cluster` its byte offset, `advance` is `desc.advance * s`, and the i-th glyph's position is
    `(i * desc.advance * s, 0)`.
12. `rasterize(f, g, p)`: for a code point that is whitespace (U+0009–U+000D, U+0020, U+00A0), an
    empty bitmap (0×0, `left = 0`, `top = 0`). Otherwise a solid box: `width = ceil(desc.advance * p)`,
    `height = ceil(desc.ascent * p) + ceil(desc.descent * p)`, `left = 0`,
    `top = ceil(desc.ascent * p)`, every coverage value 255. `GlyphId` values above U+10FFFF, or in
    U+D800–U+DFFF, are not glyphs of a fake font.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `FontId` not issued by this engine (including `FontId{0}`) | `metrics`, `shape`, `rasterize` fail with NotFound |
| Size or pixel size ≤ 0, NaN or infinite | InvalidArgument (checked after the font is found) |
| `shape` given text that is not valid UTF-8 (bad bytes, overlong forms, surrogates, truncated sequences) | InvalidArgument |
| `loadFont` given empty data, or data that is not a font | InvalidArgument |
| `rasterize` given a `GlyphId` the font doesn't have | NotFound |
| Empty text | Success: empty run (rule 7) |
| Glyph with no outline (a space) | Success: 0×0 bitmap |
| `addFont` with an invalid `FakeFontDesc` | Precondition violation (`COIL_EXPECTS`) |

### 4.4 Threading and lifetime
- Neither port is thread-safe. Each engine is used by one thread at a time; for now that is the UI
  thread. Where the renderer runs and how it shares the rasterizer is decided with the frame model
  (EPIC-015, ADR A), not here.
- The engine owns its fonts. Results (`ShapedRun`, `GlyphBitmap`) are plain values that outlive the
  call and the engine.

### 4.5 Async behavior
None. Every call is synchronous and does no I/O (fonts arrive as bytes; reading files is the
caller's job).

### 4.6 Automation and agent exposure
Not applicable: these are ports, not widgets. `Label` (spec 5) publishes the text it shows.

## 5. Acceptance Criteria
- **AC-1** — Given a fresh engine (contract), when fonts are added, then each has a distinct
  non-zero id, and `metrics`, `shape` and `rasterize` with an unknown id (`FontId{0}`, and an id one
  past the last issued) fail with NotFound (rule 1).
- **AC-2** — Given a font (contract), when `metrics`, `shape` or `rasterize` get a size of 0, −1, NaN
  or infinity, then each fails with InvalidArgument; and `loadFont` of empty data and of the bytes
  `"not a font"` fails with InvalidArgument (rule 2, §4.3).
- **AC-3** — Given a font (contract), when metrics are queried at sizes 10 and 20, then they obey
  rule 4 (signs, and linear in size within 1e-4).
- **AC-4** — Given a font (contract), when `"Hello, world"` is shaped at size 16, then the run has the
  given font and size, at least one glyph, ordered positions, `advance` = the sum of the glyph
  advances, non-decreasing clusters on code point boundaries; `""` gives an empty run; shaping
  `"Hello"`, `"ñandú"` and `"日本"` at 16 and 32 is linear in size (rule 8); and shaping twice gives
  equal runs (rules 3, 5–8).
- **AC-5** — Given a font (contract), when `shape` gets invalid UTF-8 (a lone `0xFF`, a truncated
  `0xE6 0x97`, an overlong `0xC0 0xAF`, an encoded surrogate `0xED 0xA0 0x80`), then it fails with
  InvalidArgument (§4.3).
- **AC-6** — Given the glyphs of shaping `"Ag "` (contract), when each is rasterized at pixel size 24,
  then every result succeeds with `coverage.size() == width * height`, at least one glyph has
  coverage greater than zero, and rasterizing twice gives equal bitmaps; a `GlyphId` of
  `0xFFFF'FFFF` fails with NotFound (rule 3, §4.3).
- **AC-7** — Given a fake font `{advance 0.5, ascent 0.8, descent 0.2, lineGap 0.1}`, when its metrics
  are queried and `"aé日"` is shaped at size 10, then the metrics are `{8, 2, 1}` and the glyphs are
  exactly `{U+0061 at (0,0), cluster 0}`, `{U+00E9 at (5,0), cluster 1}`, `{U+65E5 at (10,0),
  cluster 3}`, each with advance 5, and run advance 15; `loadFont` of any bytes fails with
  InvalidArgument (rules 9–11).
- **AC-8** — Given the same fake font, when `'A'` is rasterized at pixel size 10 and 15, then the
  bitmaps are 5×10 with `top = 8`, and 8×15 with `top = 12` (`ceil(12) + ceil(3)`), `left = 0`, all
  coverage 255; space and U+00A0 give 0×0 bitmaps; GlyphIds `0x110000` and `0xD800` fail with
  NotFound (rule 12).

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used | Notes |
|----|-----------|------|---------------------|-------|
| AC-1 | `TextContract.AC1_FontIdsAreDistinctAndUnknownIdsFail` | unit (contract) | `FakeTextEngine` | Typed suite; spec 2 reruns it for FreeType |
| AC-2 | `TextContract.AC2_InvalidSizesAndFontDataFail` | unit (contract) | `FakeTextEngine` | |
| AC-3 | `TextContract.AC3_MetricsScaleLinearly` | unit (contract) | `FakeTextEngine` | |
| AC-4 | `TextContract.AC4_ShapedRunsAreOrderedAndLinear` | unit (contract) | `FakeTextEngine` | Non-ASCII text checks cluster offsets |
| AC-5 | `TextContract.AC5_InvalidUtf8Fails` | unit (contract) | `FakeTextEngine` | |
| AC-6 | `TextContract.AC6_ShapedGlyphsRasterize` | unit (contract) | `FakeTextEngine` | |
| AC-7 | `FakeTextEngine.AC7_FixedAdvanceMetricsAndShaping` | unit | — | Exact values |
| AC-8 | `FakeTextEngine.AC8_SolidBoxGlyphs` | unit | — | Exact values, including rounding up |

## 7. Non-Functional Requirements
- No allocation or performance budget yet (ADR E, EPIC-015). Shaping is not cached.
- ABI: new ports in `render`, which backends implement. Changes to them after spec 2 touch
  `text_freetype`.

## 8. Architectural Impact
- Layers/modules touched: `render` only (public headers, `src/` for the out-of-line destructors,
  `testing/`, `tests/contract/`).
- New public types: those in §4.1.
- New dependencies: none. UTF-8 decoding for the fake is written in `render_testing`; real shaping
  arrives with `text_freetype` (spec 2, ADR-0018 O-2).
- Existing public API: unchanged.

## 9. Open Questions
Resolved 2026-11-12: all as recommended.
- **Q1 — Where the ports live.** *Recommended:* `render`, next to `RenderBackend`: OVERVIEW.md already
  lists font resource ids there, renderers need the rasterizer, and `core` (measurement, widgets)
  already depends on `render`. A separate `text` module would need a new layering edge and an ADR.
- **Q2 — Two ports sharing ids.** *Recommended:* fonts are loaded through `TextShaper`, and a
  `FontRasterizer` accepts the ids of its paired shaper (rule 1). Both backends implement both
  ports in one object. The alternative, a third `FontLibrary` port that owns fonts, adds a type with
  no second implementation in sight.
- **Q3 — Units.** *Recommended:* the shaper works in DIPs and does not hint (rule 5), so text measures
  the same at every display scale; the rasterizer works in pixels (`pixelSize` = size × scale), so
  glyphs stay sharp on Retina.
- **Q4 — Invalid UTF-8.** *Recommended:* fail with InvalidArgument rather than substitute U+FFFD, so
  bad input is caught where it enters. `Label` can choose to substitute later.
- **Q5 — Fake font loading.** *Recommended:* the fake rejects all font data and fonts are added with
  `addFont(FakeFontDesc)`; the contract gets a usable font from its traits, so it doesn't depend on
  a font file.
- **Q6 — Fake glyphs.** *Recommended:* glyph id = code point, and solid boxes (empty for whitespace),
  sized with `ceil` (rule 12), so spec 3's golden tests can work out every pixel by hand.

---

## 10. Implementation Plan
### 10.1 Steps
1. **Red** (commit `test(0031): …`):
   - **Stubs:** the three public headers; out-of-line port destructors; `FakeTextEngine` whose
     `addFont` returns `FontId{0}`, `loadFont` returns `FontId{0}`, `metrics` returns `{}`, `shape`
     returns an empty `ShapedRun{}`, and `rasterize` returns an empty `GlyphBitmap{}`, all as
     successes.
   - **Tests:** the contract suite `text_contract.hpp` (AC-1 to AC-6), and
     `fake_text_engine_test.cpp`, which instantiates the suite for the fake and holds AC-7 and AC-8.
   - **Expected Red:** all 8 tests fail on assertions. AC-1 fails because the ids are zero and the
     same; AC-2 and AC-5 because invalid input succeeds; AC-3 because ascent is 0; AC-4 because the
     run has the wrong size and no glyphs; AC-6 because nothing has coverage and the bad glyph
     succeeds; AC-7 and AC-8 on exact values.
2. **Green:**
   - The fake keeps a `std::vector<FakeFontDesc>` indexed by `id - 1`, with
     `COIL_EXPECTS` on the descriptor.
   - A private UTF-8 decoder in `render_testing` rejects bad lead bytes, truncated sequences,
     overlong forms, surrogates and values above U+10FFFF, and gives code points with their byte
     offsets.
   - Size checks come after the font lookup (§4.3 order).
   - Rasterizing fills a `width * height` buffer with 255, using `std::ceil` per rule 12.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; `ctest --preset tsan`.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/render/include/coil/render/text.hpp` | create | Value types |
| `modules/render/include/coil/render/text_shaper.hpp` | create | `TextShaper` port |
| `modules/render/include/coil/render/font_rasterizer.hpp` | create | `FontRasterizer` port |
| `modules/render/src/text.cpp` | create | Out-of-line port destructors |
| `modules/render/testing/include/coil/render/testing/fake_text_engine.hpp` | create | The fake |
| `modules/render/testing/src/fake_text_engine.cpp` | create | The fake, with its UTF-8 decoder |
| `modules/render/tests/contract/coil/render/text_contract.hpp` | create | Contract suite (AC-1 to AC-6) |
| `modules/render/tests/fake_text_engine_test.cpp` | create | Instantiation; AC-7, AC-8 |
| `modules/render/CMakeLists.txt`, `modules/render/tests/CMakeLists.txt` | modify | New sources |
| `docs/epics/EPIC-010-text.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Float rounding in `ceil`.** `0.8F * 15` is not exactly 12. In `float` it rounds to 12.0, so
  AC-8's values hold, but a different descriptor could give a pixel more than a hand calculation.
  The fake computes in `float`, as written in rule 12, and AC-8 uses values that are checked.
  Alternative: round to a tolerance before `ceil`. Rejected as surprising.
- **UTF-8 validation lives in the fake for now.** Spec 2 needs the same check before handing text
  to HarfBuzz, which substitutes rather than rejects. If spec 2 wants a shared helper, its spec
  proposes one (in `foundation`, say). It is not added here, to keep this spec to its footprint.
- **The contract's 1e-4 linearity tolerance** assumes the FreeType backend reads unhinted metrics in
  font units. If spec 2 finds that it can't, the tolerance is a contract change and goes through an
  amendment here.

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors on all CI targets
- [ ] Sanitizer presets pass
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `TextContract.AC1_FontIdsAreDistinctAndUnknownIdsFail`
- [ ] AC-2 → `TextContract.AC2_InvalidSizesAndFontDataFail`
- [ ] AC-3 → `TextContract.AC3_MetricsScaleLinearly`
- [ ] AC-4 → `TextContract.AC4_ShapedRunsAreOrderedAndLinear`
- [ ] AC-5 → `TextContract.AC5_InvalidUtf8Fails`
- [ ] AC-6 → `TextContract.AC6_ShapedGlyphsRasterize`
- [ ] AC-7 → `FakeTextEngine.AC7_FixedAdvanceMetricsAndShaping`
- [ ] AC-8 → `FakeTextEngine.AC8_SolidBoxGlyphs`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-08 | Spec created (Draft) | |
| 2026-11-12 | Spec and Q1–Q6 approved as recommended | Emilio Pomares |
| 2026-11-12 | Implementation plan approved | Emilio Pomares |
| 2026-11-12 | §4.1: the port destructors are pure virtual (still defined), as `UiDispatcher`'s are, so that `misc-multiple-inheritance` accepts a class implementing both ports. No behavioural change | |
| 2026-11-12 | Red: 8 of 8 new tests fail on assertions (284 tests, 276 pass). AC-1: ids 0 and equal; AC-2, AC-5: invalid input succeeds; AC-3: ascent 0; AC-4, AC-6: no glyphs; AC-7, AC-8: exact values differ | |
