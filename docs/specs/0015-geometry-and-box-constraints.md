# SPEC-0015: Geometry types and `BoxConstraints`

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::foundation` (geometry); `coil::layout` (new module, `BoxConstraints`) |
| Epic | [EPIC-004](../epics/EPIC-004-layout-engine.md) (spec 1) |
| Related ADRs | ADR-0019 (layout module, constraint model), ADR-0006 (DIPs), ADR-0013 (errors) |
| Depends on | SPEC-0002 (`coil::Error`) |

> **Scope Gate checklist**
> - [x] One concern: the value types every layout rule is written in (sizes, points, rectangles, insets, constraints)
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 modules (`foundation`, new `layout`); the `layout` module and its edges are approved by ADR-0019
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
Every later layout spec (padding, size, alignment, rows, columns, grids) is written in terms of sizes,
positions, rectangles, insets and Flutter-style box constraints. This spec adds those value types, so
the layout specs can concentrate on layout rules. The geometry types go in `foundation`, so `render`
can use them too (EPIC-006). `BoxConstraints` goes in a new `layout` module (ADR-0019).

## 2. Scope
- `foundation`: `Size`, `Point`, `Rect`, `Insets` in `float` DIPs, with equality, a few derived values,
  and `std::formatter`s for readable test messages.
- New module `layout` (depends on `foundation` only): `BoxConstraints` with validated factories,
  `constrain`, `isSatisfiedBy`, `deflate`, `loosen`, queries, and a formatter.
- Layering: `tools/layering.json` gains `layout` → `foundation`, and `core` gains `layout` (ADR-0019).
  OVERVIEW §3 is updated to match.

## 3. Out of Scope
- The layout element protocol, tree pass and modifiers (EPIC-004 spec 2 onward).
- Constraint operations needed only by later specs (for example, intersecting two constraints for
  `minSize`/`maxSize`, or tightening one axis). Each is added by the spec that needs it.
- Arithmetic on geometry (`Point + Point`, `Rect::contains`, translation, union): added when a spec needs
  it (arrangement, hit testing).
- `Color` (EPIC-006, `render`), transforms, integer pixel geometry (backends only).
- `COIL_EXPECTS` and the contract macros (ADR-0013): not needed here, see §9 Q2.

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/foundation/include/coil/foundation/geometry.hpp
namespace coil::foundation {

/// A width and height in DIPs.
struct Size {
    float width = 0.0F;
    float height = 0.0F;
    friend constexpr bool operator==(Size, Size) = default;
};

/// A position in DIPs. x grows rightwards, y grows downwards.
struct Point {
    float x = 0.0F;
    float y = 0.0F;
    friend constexpr bool operator==(Point, Point) = default;
};

/// An axis-aligned rectangle: its top-left corner and its size, in DIPs.
struct Rect {
    Point origin;
    Size size;

    [[nodiscard]] constexpr float left() const noexcept;   ///< origin.x
    [[nodiscard]] constexpr float top() const noexcept;    ///< origin.y
    [[nodiscard]] constexpr float right() const noexcept;  ///< origin.x + size.width
    [[nodiscard]] constexpr float bottom() const noexcept; ///< origin.y + size.height
    friend constexpr bool operator==(Rect, Rect) = default;
};

/// Space around the four edges of a box, in DIPs.
struct Insets {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;

    [[nodiscard]] constexpr float horizontal() const noexcept; ///< left + right
    [[nodiscard]] constexpr float vertical() const noexcept;   ///< top + bottom
    friend constexpr bool operator==(Insets, Insets) = default;
};

} // namespace coil::foundation

namespace coil {
using foundation::Insets;
using foundation::Point;
using foundation::Rect;
using foundation::Size;
} // namespace coil

// std::formatter specializations for Size ("100x50"), Point ("(10, 20)"),
// Rect ("(10, 20) 100x50") and Insets ("{1, 2, 3, 4}", in left, top, right, bottom order).
```
```cpp
// modules/layout/include/coil/layout/box_constraints.hpp
namespace coil::layout {

/// No upper bound on an axis.
inline constexpr float kUnbounded = std::numeric_limits<float>::infinity();

/// The sizes a parent allows a child to take (Flutter model, ADR-0019): for each axis a minimum
/// (finite, ≥ 0) and a maximum (≥ the minimum, possibly kUnbounded). Every BoxConstraints value
/// satisfies these invariants; the factories reject anything else.
class BoxConstraints {
public:
    /// Any size from 0×0 to unbounded on both axes. Same as unbounded().
    constexpr BoxConstraints() noexcept = default;

    /// Fails with InvalidArgument if any value is NaN, a minimum is negative or infinite, or a
    /// minimum exceeds its maximum.
    [[nodiscard]] static Expected<BoxConstraints> create(float minWidth, float maxWidth,
                                                         float minHeight, float maxHeight);
    /// Exactly `size`. Fails with InvalidArgument unless both sides are finite and ≥ 0.
    [[nodiscard]] static Expected<BoxConstraints> tight(Size size);
    /// From 0×0 up to `size`. Fails with InvalidArgument if a side is NaN or negative; kUnbounded
    /// sides are allowed.
    [[nodiscard]] static Expected<BoxConstraints> loose(Size size);
    /// From 0×0 to unbounded on both axes.
    [[nodiscard]] static constexpr BoxConstraints unbounded() noexcept;

    [[nodiscard]] float minWidth() const noexcept;
    [[nodiscard]] float maxWidth() const noexcept;
    [[nodiscard]] float minHeight() const noexcept;
    [[nodiscard]] float maxHeight() const noexcept;

    [[nodiscard]] bool isTight() const noexcept;           ///< min == max on both axes
    [[nodiscard]] bool hasBoundedWidth() const noexcept;   ///< maxWidth is finite
    [[nodiscard]] bool hasBoundedHeight() const noexcept;  ///< maxHeight is finite

    /// The size closest to `size` that these constraints allow (rule 5).
    [[nodiscard]] Size constrain(Size size) const noexcept;
    /// True if `size` lies within the constraints on both axes.
    [[nodiscard]] bool isSatisfiedBy(Size size) const noexcept;
    /// The constraints left for content inside `insets` (rule 6).
    [[nodiscard]] BoxConstraints deflate(Insets insets) const noexcept;
    /// Same maximums, minimums set to 0.
    [[nodiscard]] BoxConstraints loosen() const noexcept;

    friend bool operator==(const BoxConstraints&, const BoxConstraints&) = default;
};

} // namespace coil::layout

// std::formatter<coil::layout::BoxConstraints>: "w 0..100, h 20..inf"
```
(Private members are left to the implementation.)

### 4.2 Behavioral rules
1. Geometry types are plain aggregates. Default values are all zero, and equality compares every
   member. `Rect::right()`/`bottom()` and `Insets::horizontal()`/`vertical()` are the sums in §4.1.
2. Formatters use `std::format("{}")` for each number, so `100`, `12.5`, `inf`. Formats: `Size`
   `"100x50"`, `Point` `"(10, 20)"`, `Rect` `"(10, 20) 100x50"`, `Insets` `"{1, 2, 3, 4}"`,
   `BoxConstraints` `"w 0..100, h 20..inf"`.
3. **Invariants.** For every `BoxConstraints` value, on each axis: `0 ≤ min`, `min` is finite,
   `min ≤ max`, `max` may be `kUnbounded`, and nothing is NaN. Every factory and operation in this spec
   either returns a value that keeps them or fails with `InvalidArgument`.
4. **Factories.**
   - `create` stores the given values if they satisfy rule 3.
   - `tight(s)` is `min = max = s` on both axes.
   - `loose(s)` is `min = 0`, `max = s`.
   - `unbounded()` and the default constructor are `0..kUnbounded` on both axes.
5. **`constrain(s)`**, per axis: clamp `s` into `[min, max]`. A NaN component becomes `min`. The result
   always satisfies the constraints, so `isSatisfiedBy(constrain(s))` is always true. (An infinite
   `s` side under an unbounded maximum stays infinite. Layout specs must not return such sizes;
   that is their rule, not this one.)
6. **`deflate(i)`**, per axis (`i.horizontal()` for width, `i.vertical()` for height): subtract the
   insets from `min` and from `max`, and floor each result at 0. `kUnbounded` stays `kUnbounded`.
   A NaN inset component counts as 0. Negative insets enlarge the constraints (plain arithmetic; the
   `padding` modifier will reject negative values when it is created, in EPIC-004 spec 2).
7. **`loosen()`** sets both minimums to 0 and keeps the maximums.
8. `isTight` is true when `min == max` on both axes. `hasBoundedWidth`/`hasBoundedHeight` are true
   when the maximum is finite.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `create` with any NaN | `InvalidArgument` |
| `create` with a negative or infinite minimum | `InvalidArgument` |
| `create` with min > max on an axis | `InvalidArgument` |
| `create(0, kUnbounded, 0, kUnbounded)` | Succeeds, equals `unbounded()` |
| `tight` with a negative, NaN or infinite side | `InvalidArgument` |
| `loose` with a negative or NaN side | `InvalidArgument` |
| `loose({kUnbounded, kUnbounded})` | Succeeds, equals `unbounded()` |
| `constrain` with a NaN component | That component becomes `min` |
| `deflate` by more than the available space | Min and max floor at 0 (tight 0 on that axis) |
| `deflate` with a NaN inset component | Treated as 0 |

### 4.4 Threading and lifetime
Values only; no shared state. Safe to use from any thread.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widget or user-facing feature).

## 5. Acceptance Criteria
- **AC-1** — Given default-constructed `Size`, `Point`, `Rect` and `Insets`, then all members are 0;
  values with equal members compare equal and values differing in any one member compare unequal;
  `Rect{{10, 20}, {100, 50}}` has right 110 and bottom 70; `Insets{1, 2, 3, 4}` has horizontal 4 and
  vertical 6.
- **AC-2** — Given `Size{100, 50}`, `Point{10, 20.5}`, `Rect{{10, 20}, {100, 50}}` and
  `Insets{1, 2, 3, 4}`, when formatted, then they read `"100x50"`, `"(10, 20.5)"`,
  `"(10, 20) 100x50"` and `"{1, 2, 3, 4}"`.
- **AC-3** — Given `create` with valid values (including `kUnbounded` maximums), then it succeeds and
  the accessors return those values. Given NaN anywhere, a negative or infinite minimum, or a minimum
  above its maximum, then it fails with `InvalidArgument`.
- **AC-4** — `tight({100, 50})` is 100..100 × 50..50 and `isTight`; `loose({100, 50})` is 0..100 ×
  0..50 and bounded on both axes; `unbounded()` equals the default value and `create(0, kUnbounded, 0,
  kUnbounded)`, is not tight and is unbounded on both axes; `loose({kUnbounded, kUnbounded})` equals
  `unbounded()`. `tight` fails with `InvalidArgument` for a negative, NaN or infinite side, and `loose`
  for a negative or NaN side.
- **AC-5** — Given constraints 10..100 × 20..kUnbounded, then `constrain` maps `{5, 5}` → `{10, 20}`,
  `{50, 60}` → `{50, 60}`, `{500, 1000}` → `{100, 1000}`, and `{NaN, NaN}` → `{10, 20}`; and
  `isSatisfiedBy` is true for `{10, 20}` and `{100, 1e9}` and false for `{9, 20}`, `{101, 20}` and
  `{50, 19}`.
- **AC-6** — Given constraints 30..100 × 0..kUnbounded, then `deflate(Insets{5, 10, 15, 20})` is
  10..80 × 0..kUnbounded; `deflate(Insets{60, 0, 60, 0})` is 0..0 width; a NaN inset component is
  treated as 0; and `deflate(Insets{-5, 0, -5, 0})` is 40..110 width.
- **AC-7** — Given constraints 30..100 × 40..kUnbounded, then `loosen()` is 0..100 × 0..kUnbounded.
- **AC-8** — `BoxConstraints` formats as `"w 0..100, h 20..inf"` for 0..100 × 20..kUnbounded, and a
  tight 100×50 formats as `"w 100..100, h 50..50"`.

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used | Notes |
|----|-----------|------|---------------------|-------|
| AC-1 | `Geometry.AC1_ValuesDefaultToZeroAndCompareByValue` | unit | — | `foundation` tests |
| AC-2 | `Geometry.AC2_FormatsReadably` | unit | — | |
| AC-3 | `BoxConstraints.AC3_CreateValidates` | unit | — | `layout` tests; one case per invalid input |
| AC-4 | `BoxConstraints.AC4_Factories` | unit | — | includes the factory error cases |
| AC-5 | `BoxConstraints.AC5_ConstrainClampsIntoRange` | unit | — | includes NaN |
| AC-6 | `BoxConstraints.AC6_DeflateSubtractsInsets` | unit | — | includes over-deflation, NaN, negative |
| AC-7 | `BoxConstraints.AC7_LoosenDropsMinimums` | unit | — | |
| AC-8 | `BoxConstraints.AC8_FormatsReadably` | unit | — | |

Tests compare floats with `EXPECT_EQ`: every expected value is exactly representable and computed by
at most one addition or subtraction, so results are exact. Test files define gtest `PrintTo` for these
types through their formatters, so failures print readable values.

## 7. Non-Functional Requirements
- All geometry members and `BoxConstraints` operations are `noexcept` (factories return `Expected`).
- No allocation except in formatters and in the error path of factories.

## 8. Architectural Impact
- Layers/modules touched: `foundation` (new header); new module `layout` (ADR-0019).
- New public types: `Size`, `Point`, `Rect`, `Insets` (also aliased into `coil`); `coil::layout::BoxConstraints`,
  `kUnbounded`.
- New inter-module edges: `layout` → `foundation`; `core` → `layout` (recorded now; `core` does not
  exist yet). Both approved by ADR-0019.
- OVERVIEW §3: new `layout` row; the `foundation` row lists `Size`, `Point`, `Rect`, `Insets` (instead
  of `Vec2`, `Rect`, `Color`; `Color` moves to `render`, as EPIC-006 already plans); the `core` row
  takes layout from the `layout` module.
- No existing API changes.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — `Rect` representation.** *Recommended: origin + size* (`Rect{Point origin; Size size;}`), as
  layout produces "where" and "how big" separately. Alternative: left, top, right, bottom.
- **Q2 — Invalid input.** ADR-0013's `COIL_EXPECTS` macros don't exist yet. *Recommended:* the
  factories validate and return `Expected` (outside values arrive only through them: the window size
  and modifier parameters), and every operation on an existing value is total and keeps the invariants
  (rules 5–6 say what happens with NaN). Alternative: a separate spec first adds `COIL_EXPECTS` and
  these become preconditions (abort on violation).
- **Q3 — Negative insets in `deflate`.** *Recommended:* plain arithmetic (they enlarge), since
  `deflate` is math and the `padding` modifier validates its own input. Alternative: treat negative
  components as 0.
- **Q4 — Names.** *Recommended:* `Size`, `Point`, `Rect`, `Insets` in `coil::foundation`, aliased into
  `coil` like `Error`. `Insets` rather than Flutter's `EdgeInsets` or Compose's `PaddingValues`, because
  they will also serve borders and margins.
- **Q5 — Formats** in rule 2. *Recommended as written.* They appear in test failures and, later, in
  the layout tree dump (EPIC-004 spec 6).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0015): …`):
   - **Stubs:** `geometry.hpp` with the aggregates, the derived functions returning 0, equality written
     by hand returning `false`, and formatters printing `""`. `layout` module with `BoxConstraints`
     whose factories return a default value, operations returning `*this` or the input size, queries
     returning `false`, and a formatter printing `""`. `layering.json` and the CMake wiring for
     `layout` go in this commit, so the stubs build and the layering gate stays green.
   - **Tests:** `modules/foundation/tests/geometry_test.cpp`,
     `modules/layout/tests/box_constraints_test.cpp`. Lint the test code.
   - **Expected Red:** all 8 new tests fail on assertions; the other 132 tests pass.
2. **Green:** defaulted equality; derived values; formatters; `BoxConstraints` storage, validation,
   operations and formatter, per §4.2.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; OVERVIEW §3; EPIC-004 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/foundation/include/coil/foundation/geometry.hpp` | create | Geometry types and formatters |
| `modules/foundation/tests/geometry_test.cpp` | create | AC-1, AC-2 |
| `modules/foundation/tests/CMakeLists.txt` | modify | Add the test file |
| `modules/layout/CMakeLists.txt` | create | `coil_add_module(layout … DEPENDS coil::foundation)` |
| `modules/layout/include/coil/layout/box_constraints.hpp` | create | `BoxConstraints`, `kUnbounded`, formatter |
| `modules/layout/src/box_constraints.cpp` | create | Factories and operations |
| `modules/layout/tests/{CMakeLists.txt,.clang-tidy,box_constraints_test.cpp}` | create | AC-3–AC-8 (`.clang-tidy` as in other test dirs) |
| `CMakeLists.txt` | modify | `add_subdirectory(modules/layout)` |
| `tools/layering.json` | modify | `layout: [foundation]`; `core` gains `layout` |
| `docs/architecture/OVERVIEW.md` | modify | §3 module table |
| `docs/epics/EPIC-004-layout-engine.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Header-only geometry vs a `.cpp`:** all geometry is `constexpr` or a formatter, so it stays in the
  header. `foundation` needs no new source file.
- **`-ffast-math`** would break NaN handling. It isn't used, and the warning set doesn't enable it.
- **A class with private fields vs a plain struct for `BoxConstraints`:** a class keeps rule 3's
  invariants impossible to break. The cost is accessors instead of members.

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
- [ ] AC-1 → `Geometry.AC1_ValuesDefaultToZeroAndCompareByValue`
- [ ] AC-2 → `Geometry.AC2_FormatsReadably`
- [ ] AC-3 → `BoxConstraints.AC3_CreateValidates`
- [ ] AC-4 → `BoxConstraints.AC4_Factories`
- [ ] AC-5 → `BoxConstraints.AC5_ConstrainClampsIntoRange`
- [ ] AC-6 → `BoxConstraints.AC6_DeflateSubtractsInsets`
- [ ] AC-7 → `BoxConstraints.AC7_LoosenDropsMinimums`
- [ ] AC-8 → `BoxConstraints.AC8_FormatsReadably`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q5 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: all 8 new tests fail on assertions (stubs: hand-written equality returning `false`, derived values reading the wrong member, empty formatters, factories returning the default value, operations returning their input or a default, queries returning `false`); the other 132 tests pass. AC-6 and AC-7 were first written as comparisons with `make(...)`, which passed against the stubs (both sides came from the stubbed factory); before this commit they were rewritten to check each range. Test printers are `operator<<` in an unnamed inline namespace (internal linkage, still found by GoogleTest). `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
