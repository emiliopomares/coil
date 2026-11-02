# SPEC-0017: Size and alignment modifiers

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::layout` only |
| Epic | [EPIC-004](../epics/EPIC-004-layout-engine.md) (spec 3) |
| Related ADRs | ADR-0019 (constraints, modifier chains) |
| Depends on | SPEC-0015 (`BoxConstraints`), SPEC-0016 (elements, chains) |

> **Scope Gate checklist**
> - [x] One concern: modifiers that ask for a size and position content inside the space they get
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`layout`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
With only `padding`, an element is as big as its content plus padding. Real layouts need to ask for an
exact size ("this button is 120×32"), set lower or upper limits ("at least 200 wide", "at most 400"),
and place content inside a larger space ("centre the label in the panel"). This spec adds those
modifiers, following Jetpack Compose's semantics (ADR-0019): `size`, `minSize`, `maxSize`, `align`
and `center`. It also adds the constraint operation they share, `BoxConstraints::enforce`, which
SPEC-0015 left to "the spec that needs it".

## 2. Scope
- `BoxConstraints::enforce(other)`: `other`, clamped into these constraints (Flutter's `enforce`).
- `Alignment`: a position inside a box, as horizontal and vertical fractions from −1 to 1, with the
  nine named positions.
- Modifiers: `size(Size)`, `minSize(Size)`, `maxSize(Size)`, `align(Alignment)`, `center()`.

## 3. Out of Scope
- Single-axis variants (`width`, `height`, `minWidth`, …): added when needed (§9 Q4). One-axis limits
  can already be written with `minSize`/`maxSize` using 0 or `kUnbounded` on the other axis.
- Sizes that ignore incoming constraints (Compose's `requiredSize`).
- Aspect ratio, offset, fill-max-size, weight (spec 4).
- Right-to-left layout: `start`/`end` mean left/right until RTL is specified.
- Pixel snapping (backends only, ADR-0006).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/layout/include/coil/layout/box_constraints.hpp (addition)
class BoxConstraints {
public:
    // … existing members …

    /// `other`, clamped into these constraints: each of other's minimum and maximum is clamped into
    /// [min, max] of this one, per axis. The result always satisfies these constraints.
    [[nodiscard]] BoxConstraints enforce(const BoxConstraints& other) const noexcept;
};
```
```cpp
// modules/layout/include/coil/layout/alignment.hpp
namespace coil::layout {

/// A position inside a box: -1 is the start (left) or top edge, 0 the centre, 1 the end (right) or
/// bottom edge. Values in between are proportional.
struct Alignment {
    float horizontal = 0.0F;
    float vertical = 0.0F;

    [[nodiscard]] static constexpr Alignment topStart() noexcept { return {-1.0F, -1.0F}; }
    [[nodiscard]] static constexpr Alignment topCenter() noexcept { return {0.0F, -1.0F}; }
    [[nodiscard]] static constexpr Alignment topEnd() noexcept { return {1.0F, -1.0F}; }
    [[nodiscard]] static constexpr Alignment centerStart() noexcept { return {-1.0F, 0.0F}; }
    [[nodiscard]] static constexpr Alignment center() noexcept { return {0.0F, 0.0F}; }
    [[nodiscard]] static constexpr Alignment centerEnd() noexcept { return {1.0F, 0.0F}; }
    [[nodiscard]] static constexpr Alignment bottomStart() noexcept { return {-1.0F, 1.0F}; }
    [[nodiscard]] static constexpr Alignment bottomCenter() noexcept { return {0.0F, 1.0F}; }
    [[nodiscard]] static constexpr Alignment bottomEnd() noexcept { return {1.0F, 1.0F}; }

    friend constexpr bool operator==(Alignment, Alignment) = default;
};

} // namespace coil::layout
```
```cpp
// modules/layout/include/coil/layout/modifier.hpp (additions)
namespace coil::layout {

/// Asks for exactly `size`; incoming constraints win where they disagree. "coil.size".
/// Fails with InvalidArgument unless both sides are finite and >= 0.
[[nodiscard]] ModifierResult size(Size size);
/// Asks for at least `size`; incoming constraints win. "coil.minSize".
/// Fails with InvalidArgument unless both sides are finite and >= 0.
[[nodiscard]] ModifierResult minSize(Size size);
/// Asks for at most `size` (kUnbounded = no limit on that axis); incoming constraints win.
/// "coil.maxSize". Fails with InvalidArgument if a side is NaN or negative.
[[nodiscard]] ModifierResult maxSize(Size size);
/// Lets the content take any size up to the available space, and positions it by `alignment`.
/// "coil.align". Fails with InvalidArgument unless both fractions are in [-1, 1].
[[nodiscard]] ModifierResult align(Alignment alignment);
/// align(Alignment::center()).
[[nodiscard]] ModifierResult center();

} // namespace coil::layout
```

### 4.2 Behavioral rules
1. **`enforce(o)`**, per axis: `min = clamp(o.min, this.min, this.max)`,
   `max = clamp(o.max, this.min, this.max)`. The result keeps SPEC-0015 rule 3's invariants.
2. **`size(s)`:** inner constraints `c.enforce(tight(s))`; outer size = inner size; inner bounds = the
   stage's rectangle.
3. **`minSize(m)`:** inner constraints `c.enforce(create(m.width, ∞, m.height, ∞))` (the maximums stay
   `c`'s); outer size = inner size; inner bounds = the stage's rectangle.
4. **`maxSize(m)`:** inner constraints `c.enforce(create(0, m.width, 0, m.height))` (the minimums stay
   `c`'s); outer size = inner size; inner bounds = the stage's rectangle.
5. **`align(a)`:** inner constraints `c.loosen()`; outer size `c.constrain(inner)` (the content's size,
   grown to `c`'s minimums); inner bounds: the inner measured size, placed at
   `x = outer.x + (outer.width − inner.width) × (a.horizontal + 1) / 2`, and likewise for `y`.
   The content keeps its measured size even when the rectangle is smaller (it then extends beyond the
   rectangle, centred according to `a`; clipping is a rendering concern).
6. Type ids: `"coil.size"`, `"coil.minSize"`, `"coil.maxSize"`, `"coil.align"` (also for `center()`).

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `size`/`minSize` with a negative, NaN or infinite side | `InvalidArgument` |
| `maxSize` with a negative or NaN side | `InvalidArgument` (`kUnbounded` allowed) |
| `align` with a NaN or infinite fraction, or one outside [−1, 1] | `InvalidArgument` |
| `size` larger than the incoming maximum | Clamped to the maximum (constraints win) |
| `minSize` larger than the incoming maximum | Tight at the maximum |
| `maxSize` smaller than the incoming minimum | Tight at the minimum |
| `align` content arranged into a smaller rectangle | Content keeps its size, offset may be negative |

### 4.4 Threading and lifetime
As SPEC-0016.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
Tests reuse SPEC-0016's approach: a test `Leaf` that prefers a size and records its constraints and
content rectangle.

- **AC-1** — Given `c` = 10..100 × 20..200, then `c.enforce(tight(50×300))` is 50..50 × 200..200,
  `c.enforce(create(0, 50, 250, ∞))` is 10..50 × 200..200, and `c.enforce(unbounded())` is
  10..100 × 20..200.
- **AC-2** — Given a 50×30 leaf with `size(80×40)`, under `loose(100×100)` the leaf sees tight 80×40
  and the element measures 80×40; under `tight(60×60)` the leaf sees tight 60×60 and it measures 60×60.
- **AC-3** — Given a 50×30 leaf with `minSize(70×20)`, under `loose(100×100)` the leaf sees
  70..100 × 20..100 and the element measures 70×30; under `loose(60×60)` the leaf sees
  60..60 × 20..60 and it measures 60×30.
- **AC-4** — Given a 50×30 leaf with `maxSize(40×kUnbounded)`, under `loose(100×100)` the leaf sees
  0..40 × 0..100 and the element measures 40×30; under `tight(80×80)` the leaf sees tight 80×80 and it
  measures 80×80.
- **AC-5** — Given a 50×30 leaf under `tight(200×100)`, arranged at `(0, 0) 200×100`: with
  `align(bottomEnd())` the leaf sees `loose(200×100)`, the element measures 200×100 and the content is
  at `(150, 70) 50×30`; with `center()` at `(75, 35) 50×30`; with `align(topStart())` at
  `(0, 0) 50×30`; with `align({-0.5, 0.5})` at `(37.5, 52.5) 50×30`.
- **AC-6** — Given a 50×30 leaf with `center()` under `loose(200×100)`, it measures 50×30; arranged at
  `(10, 10) 50×30` the content is `(10, 10) 50×30`; arranged at `(0, 0) 20×20` the content is
  `(-15, -5) 50×30`.
- **AC-7** — Invalid parameters fail with `InvalidArgument`: `size` and `minSize` with a negative,
  NaN or infinite side; `maxSize` with a negative or NaN side (but `maxSize(kUnbounded×kUnbounded)`
  succeeds); `align` with a NaN or infinite fraction, or −1.5 or 1.5.
- **AC-8** — Type ids: `size` → `"coil.size"`, `minSize` → `"coil.minSize"`, `maxSize` →
  `"coil.maxSize"`, `align` and `center` → `"coil.align"`.

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used | Notes |
|----|-----------|------|---------------------|-------|
| AC-1 | `BoxConstraints.AC1_EnforceClampsIntoConstraints` | unit | — | in `box_constraints_test.cpp` (new test, existing tests untouched) |
| AC-2 | `SizeModifiers.AC2_SizeAsksForExactSize` | unit | test `Leaf` | `size_and_alignment_test.cpp` |
| AC-3 | `SizeModifiers.AC3_MinSizeRaisesMinimums` | unit | test `Leaf` | |
| AC-4 | `SizeModifiers.AC4_MaxSizeLowersMaximums` | unit | test `Leaf` | |
| AC-5 | `Align.AC5_PositionsContentInTightSpace` | unit | test `Leaf` | |
| AC-6 | `Align.AC6_WrapsContentInLooseSpace` | unit | test `Leaf` | includes overflow |
| AC-7 | `SizeModifiers.AC7_InvalidParametersAreRejected` | unit | — | includes `align` |
| AC-8 | `SizeModifiers.AC8_TypeIds` | unit | — | |

The new test file has its own `Leaf` (a copy of SPEC-0016's, about 20 lines). Sharing it through a
test-support header would modify SPEC-0016's frozen test file; that can be a refactor later, once
several layout test files need it.

## 7. Non-Functional Requirements
- No allocation in the modifiers' layout functions.

## 8. Architectural Impact
- Module: `layout` only. New public types and functions: `Alignment`, `BoxConstraints::enforce`, `size`,
  `minSize`, `maxSize`, `align`, `center`.
- No new dependency; no existing API changes.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — When `size` and incoming constraints disagree.** *Recommended: constraints win* (Compose
  `size`, Flutter `SizedBox` under tight constraints): a parent that forces a size always gets it. A
  forcing variant (`requiredSize`) can be added later if a real case needs it.
- **Q2 — Alignment representation.** *Recommended:* fractions from −1 to 1 with nine named positions
  (Flutter `Alignment`, Compose `BiasAlignment`). Custom positions such as "a quarter from the left"
  come for free. The alternative, separate start/centre/end enums per axis, is simpler but can't express
  them.
- **Q3 — `start`/`end` vs `left`/`right`.** *Recommended: `start`/`end`*, meaning left/right until
  right-to-left layout is specified; then they flip without renaming the API.
- **Q4 — Single-axis variants** (`width(w)`, `height(h)`, …). *Recommended: later*, when a widget
  needs them. `minSize`/`maxSize` already express one-axis limits.
- **Q5 — Content larger than its rectangle under `align`.** *Recommended: keep its size* and let it
  extend beyond the rectangle (rule 5), so nothing is squashed silently. Clipping comes with rendering.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0017): …`):
   - **Stubs:** `enforce` returns `*this`; `alignment.hpp` complete except the named positions return
     `{}`; the five factories return a modifier that changes nothing, with type id `""`, and never
     fail.
   - **Tests:** AC-1 added to `box_constraints_test.cpp`; `size_and_alignment_test.cpp` for AC-2–AC-8.
     Lint the test code.
   - **Expected Red:** all 8 fail on assertions; the other 150 pass.
2. **Green:** `enforce`; the named positions; the four modifier classes (`size`, `minSize`, `maxSize`
   share one class parameterized by the constraints they enforce); validation.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-004 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/layout/include/coil/layout/box_constraints.hpp`, `src/box_constraints.cpp` | modify | `enforce` |
| `modules/layout/include/coil/layout/alignment.hpp` | create | `Alignment` |
| `modules/layout/include/coil/layout/modifier.hpp`, `src/modifier.cpp` | modify | Five factories, two modifier classes |
| `modules/layout/tests/box_constraints_test.cpp` | modify | New test AC-1 (existing tests unchanged) |
| `modules/layout/tests/size_and_alignment_test.cpp` | create | AC-2–AC-8 |
| `modules/layout/tests/CMakeLists.txt` | modify | New test file |
| `docs/epics/EPIC-004-layout-engine.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Adding a test to a file with frozen tests** (`box_constraints_test.cpp`): only a new `TEST` is
  appended; the frozen tests are byte-for-byte unchanged, which the Red commit's diff shows.
- **`size(s)` with `s` = 0×0** is valid (an invisible element).

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors (macOS locally)
- [ ] Sanitizer preset passes
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [ ] AC-1 → `BoxConstraints.AC1_EnforceClampsIntoConstraints`
- [ ] AC-2 → `SizeModifiers.AC2_SizeAsksForExactSize`
- [ ] AC-3 → `SizeModifiers.AC3_MinSizeRaisesMinimums`
- [ ] AC-4 → `SizeModifiers.AC4_MaxSizeLowersMaximums`
- [ ] AC-5 → `Align.AC5_PositionsContentInTightSpace`
- [ ] AC-6 → `Align.AC6_WrapsContentInLooseSpace`
- [ ] AC-7 → `SizeModifiers.AC7_InvalidParametersAreRejected`
- [ ] AC-8 → `SizeModifiers.AC8_TypeIds`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q5 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: all 8 new tests fail on assertions (stubs: `enforce` returns its own constraints; the factories return a modifier that changes nothing, has type id `""` and never fails; named alignments return the centre); the other 150 pass. `box_constraints_test.cpp` only gains the new test (10 added lines, 0 removed). `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
