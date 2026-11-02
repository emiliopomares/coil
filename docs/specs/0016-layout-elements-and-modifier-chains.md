# SPEC-0016: Layout elements, modifier chains and `padding`

| Field | Value |
|-------|-------|
| Status | Approved (complete; awaiting acknowledgement) |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::layout` only |
| Epic | [EPIC-004](../epics/EPIC-004-layout-engine.md) (spec 2) |
| Related ADRs | ADR-0019 (constraints, modifier chains), ADR-0005 Amendment 1 (no privileged access), ADR-0006 (two passes, DIPs) |
| Depends on | SPEC-0015 (geometry, `BoxConstraints`) |

> **Scope Gate checklist**
> - [x] One concern: the protocol by which any element (built-in or custom) is measured and arranged through its modifier chain, proven with the first modifier
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`layout`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
`BoxConstraints` say what sizes are allowed, but nothing yet measures or places anything. This spec adds
the **layout element protocol** that every later layout spec builds on: measure under constraints,
then arrange into a rectangle (ADR-0006), with an ordered **modifier chain** run around the element's
own content (ADR-0019). The protocol is **public**: a custom element or container written outside
`layout` (an application's, a plugin's, or a test's) works exactly like a built-in one (ADR-0005
Amendment 1; node-editor use case). `padding` is the first modifier, and a test-defined modifier
proves that modifiers are extensible too.

## 2. Scope
- `LayoutModifier`: the interface every modifier implements, built-in or custom.
- `ModifierChain`: an ordered, move-only list of modifiers, built fluently, with deferred validation.
- `padding(Insets)` and `padding(float)`: the first built-in modifier (type id `"coil.padding"`).
- `LayoutElement`: the abstract base of every layout element. Its public `measure`/`arrange` run the
  chain around the protected `measureContent`/`arrangeContent` that each element implements.
- `layoutRoot(element, size)`: lays out a root element at a given size (for example, a window's).

## 3. Out of Scope
- Built-in leaves and containers: `size`/`minSize`/`maxSize`/`align`/`center` (spec 3), `Row`/`Column`
  and `weight` (spec 4), `Grid` and `gridCell` (spec 5). Tests define their own elements.
- **Parent-data modifiers** (ADR-0019's second kind). They arrive with `weight` in spec 4; this spec's
  chain holds layout modifiers only.
- Enumerating an element's children, and the layout tree dump (spec 6).
- Intrinsic sizes, measuring a child more than once, baselines.
- Caching and dirty flags (ADR-0006 follow-up once widgets exist).
- Serializing chains (needs the type registry; ADR-0005).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/layout/include/coil/layout/modifier.hpp
namespace coil::layout {

/// One stage of an element's modifier chain (ADR-0019). Built-in and custom modifiers implement this
/// same interface. A stage receives constraints from outside, passes (possibly changed) constraints
/// inward, reports its own size from the inner size, and places the inner content in its rectangle,
/// like a wrapper would, without being a node in the tree.
class LayoutModifier {
public:
    LayoutModifier() = default;
    virtual ~LayoutModifier() = default;
    LayoutModifier(const LayoutModifier&) = delete;
    LayoutModifier& operator=(const LayoutModifier&) = delete;
    LayoutModifier(LayoutModifier&&) = delete;
    LayoutModifier& operator=(LayoutModifier&&) = delete;

    /// Stable type id, for example "coil.padding". Custom modifiers use their own prefix.
    [[nodiscard]] virtual std::string_view typeId() const noexcept = 0;
    /// The constraints for everything inside this stage, given the constraints this stage receives.
    [[nodiscard]] virtual BoxConstraints innerConstraints(const BoxConstraints& outer) const noexcept = 0;
    /// This stage's size, given its constraints and the measured size of everything inside it.
    [[nodiscard]] virtual Size outerSize(const BoxConstraints& outer, Size inner) const noexcept = 0;
    /// Where everything inside goes, given this stage's rectangle and the measured inner size.
    [[nodiscard]] virtual Rect innerBounds(Rect outer, Size inner) const noexcept = 0;
};

/// What a modifier factory returns: the modifier, or why its parameters were rejected.
using ModifierResult = Expected<std::unique_ptr<LayoutModifier>>;

/// An ordered list of modifiers. The first one added is the outermost. Move-only.
/// Built fluently: ModifierChain{}.then(padding(8)).then(myModifier(...)).
class ModifierChain {
public:
    ModifierChain() = default;
    ModifierChain(ModifierChain&&) noexcept = default;
    ModifierChain& operator=(ModifierChain&&) noexcept = default;
    ModifierChain(const ModifierChain&) = delete;
    ModifierChain& operator=(const ModifierChain&) = delete;
    ~ModifierChain() = default;

    /// Appends `modifier` as the new innermost stage. If it holds an error, the chain records it.
    /// Once the chain has an error, it keeps its first one and ignores further modifiers.
    [[nodiscard]] ModifierChain then(ModifierResult modifier) &&;

    /// The first error recorded, if any.
    [[nodiscard]] const std::optional<Error>& error() const noexcept;
    /// The type ids of the stages, outermost first.
    [[nodiscard]] std::vector<std::string_view> typeIds() const;
};

/// Space around the content. Fails with InvalidArgument unless every side is finite and >= 0.
[[nodiscard]] ModifierResult padding(Insets insets);
/// The same space on all four sides.
[[nodiscard]] ModifierResult padding(float all);

} // namespace coil::layout
```
```cpp
// modules/layout/include/coil/layout/layout_element.hpp
namespace coil::layout {

/// Anything that takes part in layout: a leaf or a container, built-in or custom (ADR-0005 A1).
/// Containers call measure() and arrange() on their children from measureContent() and
/// arrangeContent(); how they store their children is up to them.
class LayoutElement {
public:
    virtual ~LayoutElement() = default;
    LayoutElement(const LayoutElement&) = delete;
    LayoutElement& operator=(const LayoutElement&) = delete;
    LayoutElement(LayoutElement&&) = delete;
    LayoutElement& operator=(LayoutElement&&) = delete;

    /// Replaces the chain. Fails with the chain's first error, keeping the current chain.
    [[nodiscard]] Expected<void> setModifiers(ModifierChain chain);
    [[nodiscard]] const ModifierChain& modifiers() const noexcept;

    /// Measures the element through its chain (rule 3). Returns a finite size that satisfies
    /// `constraints`, also kept as measuredSize().
    [[nodiscard]] Size measure(const BoxConstraints& constraints);
    /// Places the element: `bounds` is its outer rectangle; the chain decides where the content goes
    /// (rule 4).
    void arrange(Rect bounds);

    /// Size from the last measure() (0x0 before any).
    [[nodiscard]] Size measuredSize() const noexcept;
    /// Outer rectangle from the last arrange() (all zero before any).
    [[nodiscard]] Rect bounds() const noexcept;
    /// Content rectangle (inside every modifier) from the last arrange() (all zero before any).
    [[nodiscard]] Rect contentBounds() const noexcept;

protected:
    LayoutElement() = default;

    /// The content's size under `constraints` (after every modifier has adjusted them).
    [[nodiscard]] virtual Size measureContent(const BoxConstraints& constraints) = 0;
    /// Places the content (a container arranges its children here).
    virtual void arrangeContent(Rect contentBounds) = 0;
};

/// Lays out `root` to fill `size`: measures it with tight constraints, then arranges it at (0, 0).
/// Fails with InvalidArgument (and does nothing) unless both sides are finite and >= 0.
[[nodiscard]] Expected<void> layoutRoot(LayoutElement& root, Size size);

} // namespace coil::layout
```

### 4.2 Behavioral rules
1. **Coordinates.** All rectangles are in the root's coordinate space (DIPs, (0, 0) at the root's
   top-left). A container passes its children rectangles in that same space.
2. **Chain order.** Stage 0 is the first modifier added (outermost); the content is innermost.
3. **Measure.** With constraints `c₀` and stages `m₀ … mₙ₋₁`:
   - Going in: `cᵢ₊₁ = mᵢ.innerConstraints(cᵢ)`.
   - Content: `sₙ = cₙ.constrain(measureContent(cₙ))`, and then any side still not finite is
     replaced by `cₙ`'s minimum on that axis.
   - Coming out: `sᵢ = cᵢ.constrain(mᵢ.outerSize(cᵢ, sᵢ₊₁))`, with the same non-finite replacement.
   - The result is `s₀`, stored as `measuredSize()`; each `sᵢ` is kept for arrange.
4. **Arrange.** With `r₀ = bounds`: `rᵢ₊₁ = mᵢ.innerBounds(rᵢ, sᵢ₊₁)`, then `arrangeContent(rₙ)`.
   `bounds()` is `r₀` and `contentBounds()` is `rₙ`. The sizes come from the last `measure`, or 0×0
   if there was none. A parent MAY arrange a child into a rectangle of a different size than it
   measured.
5. **`padding(i)`:** inner constraints `c.deflate(i)`; outer size: inner size plus `i.horizontal()` and
   `i.vertical()`; inner bounds: the rectangle moved in by `left`/`top` and shrunk by the insets, with
   width and height floored at 0. Type id `"coil.padding"`. `padding(a)` is `padding(Insets{a, a, a, a})`.
6. **Chains.** `then` appends; an error (from the argument or already recorded) makes the chain keep its
   first error. `typeIds()` lists only the stages added before the first error.
7. **`setModifiers`** with a chain that has an error fails with that error's code and leaves the
   element's chain unchanged. On success the new chain is used from the next `measure`.
8. **`layoutRoot(e, s)`** = `e.measure(BoxConstraints::tight(s))` then `e.arrange({{0, 0}, s})`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `padding` with a negative, NaN or infinite side | `InvalidArgument` |
| `then` with an error, or on a chain with an error | Chain keeps its first error |
| `setModifiers` with a chain that has an error | Fails with that code; chain unchanged |
| `measureContent` returns NaN, infinity or an out-of-range size | Clamped (rule 3); never escapes |
| A modifier's `outerSize` returns an invalid size | Same clamping |
| `arrange` before any `measure` | Uses 0×0 sizes |
| Arranged into a rectangle smaller than the padding | Content gets a 0-sized rectangle |
| `layoutRoot` with a negative, NaN or infinite size | `InvalidArgument`; nothing measured or arranged |

### 4.4 Threading and lifetime
Single-threaded, like the widget tree that will own it (UI thread, ADR-0010). An element owns its chain;
a chain owns its modifiers. Destroying a container while it is being measured is a programmer error.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (no widget yet).

## 5. Acceptance Criteria
Tests use **test-defined** elements and modifiers, which also proves the protocol is public: `Leaf`
(prefers a given size, records the constraints and content rectangle it was given), `Stack` (a custom
container that owns its children, measures each under loose constraints of its own maximums, places
them top to bottom at their measured sizes, and fills its constraints itself) and `FixedSize` (a custom modifier that
forces a size; type id `"test.fixedSize"`).

- **AC-1** — Given a `Leaf` preferring 50×30 with no modifiers, before any measure its measured size and
  bounds are zero; measured under `loose(100×100)` it returns 50×30 and the leaf saw those constraints;
  under `tight(80×80)` it returns 80×80; after `arrange((10, 20) 80×80)` both `bounds()` and
  `contentBounds()` are that rectangle and the leaf's content got it.
- **AC-2** — Given leaves whose content returns `{NaN, 500}` under 10..100 × 20..200, and `{∞, ∞}`
  under `unbounded()`, then `measure` returns `{10, 200}` and `{0, 0}`.
- **AC-3** — Given a `Leaf` preferring 50×30 with `padding({5, 10, 15, 20})`, under `loose(100×100)`
  the leaf sees 0..80 × 0..70 and the element measures 70×60; under `tight(100×100)` the leaf sees
  tight 80×70 and the element measures 100×100.
- **AC-4** — After the loose measure of AC-3, `arrange((10, 10) 70×60)` gives bounds `(10, 10) 70×60`
  and content bounds `(15, 20) 50×30`, and the leaf's content gets the latter. Arranged into
  `(0, 0) 10×10`, the content bounds are `(5, 10) 0×0`.
- **AC-5** — Order matters, and custom modifiers work: under `unbounded()`, `padding(8)` then
  `FixedSize(100×100)` measures 116×116 with content bounds `(8, 8) 100×100` after arranging at
  `(0, 0) 116×116`; `FixedSize(100×100)` then `padding(8)` measures 100×100, the leaf sees tight
  84×84, and the content bounds are `(8, 8) 84×84` after arranging at `(0, 0) 100×100`. `typeIds()` are `{"coil.padding",
  "test.fixedSize"}` and `{"test.fixedSize", "coil.padding"}`.
- **AC-6** — `padding` fails with `InvalidArgument` for a negative, NaN or infinite side (both
  overloads). A chain given such a result has that error, keeps it when valid modifiers follow, and
  lists only earlier type ids. `setModifiers` with it fails with `InvalidArgument`, and the element keeps
  its previous chain (same type ids, same measured size).
- **AC-7** — Given a `Stack` (custom container) holding a 50×30 leaf with `padding(10)` and a 40×20
  leaf, when `layoutRoot(stack, 200×100)`, then the stack's bounds are `(0, 0) 200×100`, the first
  child's bounds are `(0, 0) 70×50` with content `(10, 10) 50×30`, and the second child's bounds are
  `(0, 50) 40×20`.
- **AC-8** — `layoutRoot` with a size that has a negative, NaN or infinite side fails with
  `InvalidArgument`, and the element's measured size and bounds are unchanged.

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used | Notes |
|----|-----------|------|---------------------|-------|
| AC-1 | `LayoutElement.AC1_MeasuresAndArrangesContent` | unit | test `Leaf` | |
| AC-2 | `LayoutElement.AC2_ContentSizesAreClamped` | unit | test `Leaf` | |
| AC-3 | `Padding.AC3_ShrinksConstraintsAndGrowsSize` | unit | test `Leaf` | |
| AC-4 | `Padding.AC4_InsetsTheContent` | unit | test `Leaf` | |
| AC-5 | `ModifierChain.AC5_OrderMattersAndCustomModifiersWork` | unit | test `Leaf`, `FixedSize` | |
| AC-6 | `ModifierChain.AC6_InvalidModifiersAreRejected` | unit | test `Leaf` | |
| AC-7 | `LayoutElement.AC7_CustomContainerLaysOutChildren` | unit | test `Stack`, `Leaf` | |
| AC-8 | `LayoutElement.AC8_LayoutRootRejectsInvalidSizes` | unit | test `Leaf` | |

The test elements and `FixedSize` are not fakes of the unit under test: the unit is the protocol and the
chain machinery in `LayoutElement`, plus `padding`. They are ordinary clients of the public API, as any
custom element would be.

## 7. Non-Functional Requirements
- `measure` and `arrange` allocate nothing in steady state beyond what the element's chain size
  requires (the per-stage sizes may live in a vector sized when the chain is set).
- ABI: `LayoutModifier` and `LayoutElement` are plugin-facing interfaces (ADR-0011: same toolchain).

## 8. Architectural Impact
- Module: `layout` only.
- New public types: `LayoutModifier`, `ModifierResult`, `ModifierChain`, `LayoutElement`; functions
  `padding`, `layoutRoot`.
- No new dependency; no existing API changes.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — Modifier interface.** *Recommended:* the three functions above (`innerConstraints`,
  `outerSize`, `innerBounds`). They cover padding, size, min/max, alignment, aspect ratio, offsets, and
  need no callbacks. The alternative is Compose's `measure(constraints, measureInner)` callback, which
  allows measuring the inside several times but needs a function-reference type that C++23 lacks
  (`std::function_ref` is C++26), plus more machinery than any planned modifier needs.
- **Q2 — Building and validating chains.** *Recommended:* every factory, built-in or custom, returns
  `ModifierResult`; `then` takes it and keeps the first error; `setModifiers` reports it once. That gives
  `ModifierChain{}.then(padding(8)).then(glow(…))` with no special syntax for built-ins. The
  alternative, checking each factory's `Expected` before adding it, is safer per step but verbose.
- **Q3 — Ownership.** *Recommended:* the chain owns its modifiers with `std::unique_ptr` and is
  move-only. Sharing immutable modifiers between chains (`std::shared_ptr<const …>`) could come later if
  copying chains turns out to be needed.
- **Q4 — Children.** *Recommended:* the protocol doesn't own or enumerate children; each container
  stores them its own way. This keeps the door open for widgets owning layout elements later. Child
  enumeration is added with the tree dump (spec 6).
- **Q5 — Coordinates.** *Recommended:* root coordinates everywhere (rule 1). Simpler for hit testing and
  painting now; parent-relative offsets would only pay off with transforms (EPIC-012), which can map
  coordinates when they arrive.
- **Q6 — Bad content sizes.** *Recommended:* clamp them (rule 3) so an element can never report an
  invalid size, rather than treating them as errors.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0016): …`):
   - **Stubs:** the headers in §4.1. `then` returns the chain unchanged; `error()` is always empty;
     `typeIds()` is empty; `padding` returns a modifier that changes nothing; `setModifiers` succeeds
     without storing; `measure` returns 0×0; `arrange` stores nothing; `layoutRoot` succeeds without
     doing anything.
   - **Tests:** `modules/layout/tests/layout_element_test.cpp` (test elements, `FixedSize`, AC-1–AC-8).
     Lint the test code.
   - **Expected Red:** all 8 fail on assertions; the other 141 pass.
2. **Green:** chain storage and errors; `padding`; the measure and arrange passes of rules 3–4,
   keeping per-stage sizes; `layoutRoot`.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-004 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/layout/include/coil/layout/modifier.hpp` | create | `LayoutModifier`, `ModifierChain`, `padding` |
| `modules/layout/include/coil/layout/layout_element.hpp` | create | `LayoutElement`, `layoutRoot` |
| `modules/layout/src/modifier.cpp` | create | Chain, `padding` |
| `modules/layout/src/layout_element.cpp` | create | Measure and arrange passes |
| `modules/layout/CMakeLists.txt` | modify | New sources |
| `modules/layout/tests/layout_element_test.cpp` | create | AC-1–AC-8 |
| `modules/layout/tests/CMakeLists.txt` | modify | New test file |
| `docs/epics/EPIC-004-layout-engine.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Template method vs a separate "layout policy" object:** non-virtual `measure`/`arrange` around
  protected virtual hooks guarantee every element (built-in or custom) runs its chain and clamps its
  size. A custom element cannot skip its modifiers by accident.
- **Re-entrancy:** a container measuring a child calls that child's `measure`, which may run its own
  chain; each element keeps its own per-stage sizes, so nesting is safe.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Builds with warnings-as-errors (macOS locally)
- [x] Sanitizer preset passes
- [x] Every AC ticked below with its proving test
- [x] Public API documented (Doxygen comments on new public declarations)

### AC verification
- [x] AC-1 → `LayoutElement.AC1_MeasuresAndArrangesContent`
- [x] AC-2 → `LayoutElement.AC2_ContentSizesAreClamped`
- [x] AC-3 → `Padding.AC3_ShrinksConstraintsAndGrowsSize`
- [x] AC-4 → `Padding.AC4_InsetsTheContent`
- [x] AC-5 → `ModifierChain.AC5_OrderMattersAndCustomModifiersWork`
- [x] AC-6 → `ModifierChain.AC6_InvalidModifiersAreRejected`
- [x] AC-7 → `LayoutElement.AC7_CustomContainerLaysOutChildren`
- [x] AC-8 → `LayoutElement.AC8_LayoutRootRejectsInvalidSizes`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q6 confirmed as recommended; spec and plan approved | Emilio Pomares |
| 2026-11-02 | **Red**: all 8 new tests fail on assertions (stubs: `then` drops modifiers, `padding` accepts anything and changes nothing, `setModifiers` stores without checking, `measure` returns 0×0, `arrange` records nothing, `layoutRoot` does nothing); the other 141 pass. `tools/check.sh` format, layering and tidy clean. | Emilio Pomares |
| 2026-11-02 | **Green + Refactor**: chains store modifiers and keep their first error; `padding` validates and implements rule 5; `LayoutElement` runs rules 3–4 with per-stage constraints and sizes (iterators, no unchecked indexing), sanitizing every size; `layoutRoot` validates through `BoxConstraints::tight`. Implementation notes: `ModifierChain` declares `LayoutElement` a friend so the stages stay private (no public API added); a factory result holding a null modifier is recorded as an `InvalidArgument` error (not covered by the spec; see amendment proposal). `tools/check.sh` exit 0 (149/149); `ctest --preset asan` 149/149. `git diff 98e99ee -- ':(glob)**/tests/**'` empty. | Emilio Pomares |
