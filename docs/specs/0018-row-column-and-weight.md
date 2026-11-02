# SPEC-0018: `Row`, `Column` and the `weight` modifier

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-02 |
| Module(s) | `coil::layout` only |
| Epic | [EPIC-004](../epics/EPIC-004-layout-engine.md) (spec 4) |
| Related ADRs | ADR-0019 (containers, parent-data modifiers), ADR-0005 Amendment 1 (no privileged access) |
| Depends on | SPEC-0016 (elements, chains), SPEC-0017 (size modifiers) |

> **Scope Gate checklist**
> - [x] One concern: lay children out in a line (horizontal or vertical), sharing space by weight
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`layout`); no new dependency
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9)

---

## 1. Problem / Motivation
Rows and columns are the backbone of almost every UI: a toolbar, a form, a panel with a header and a
body. This spec adds `Row` and `Column`, with spacing, main-axis and cross-axis alignment, and the
`weight` modifier that shares leftover space between children (Compose's `Row`/`Column` with
`Arrangement` and `weight`, Flutter's `Flex` with `Expanded`).

`weight` is the first **parent-data modifier** (ADR-0019): it does nothing to the element itself and is
read by its parent. Containers read it through a **public** API on the child's chain, so a custom
container can read `weight` exactly as `Row` does (ADR-0005 Amendment 1). This also replaces the
`friend` declaration SPEC-0016 used for running the chain.

## 2. Scope
- Modifier kinds: a common base `Modifier` (type id); `LayoutModifier` (as in SPEC-0016) and the new
  `ParentDataModifier` both derive from it.
- `ModifierChain`: holds both kinds; public `layoutStages()` (the layout modifiers, outermost first)
  and `find<T>()` (the outermost parent-data modifier of type `T`). The `friend` goes.
- `Weight` and `weight(float)`.
- `Row` and `Column`: own their children; options for spacing and main-axis and cross-axis alignment.

## 3. Out of Scope
- `Grid` and `gridCell` (spec 5); reporting unused parent data (spec 6, the dump).
- Wrapping onto several lines, baselines, right-to-left order, reversing.
- Removing or reordering children (comes with the widget tree, EPIC-008).
- A "min" main-axis size mode with weights (weighted rows fill their bounded maximum, rule 4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/layout/include/coil/layout/modifier.hpp (changes)
namespace coil::layout {

/// Base of every modifier: the type id (for example "coil.padding"). Not copyable or movable.
class Modifier {
public:
    Modifier() = default;
    virtual ~Modifier() = default;
    // copy and move deleted
    [[nodiscard]] virtual std::string_view typeId() const noexcept = 0;
};

/// As in SPEC-0016, now derived from Modifier (typeId() is inherited).
class LayoutModifier : public Modifier { /* innerConstraints, outerSize, innerBounds */ };

/// Data for the parent (ADR-0019). It does not change how its own element is laid out; parents that
/// understand it read it with ModifierChain::find<T>(), and other parents ignore it.
class ParentDataModifier : public Modifier {};

/// Now holds either kind.
using ModifierResult = Expected<std::unique_ptr<Modifier>>;

class ModifierChain {
public:
    // … as in SPEC-0016 (then, error, typeIds; typeIds lists both kinds) …

    /// The layout modifiers, outermost first (parent-data modifiers are skipped).
    [[nodiscard]] std::span<const LayoutModifier* const> layoutStages() const noexcept;
    /// The outermost modifier of type T, or nullptr.
    template <std::derived_from<ParentDataModifier> T>
    [[nodiscard]] const T* find() const noexcept;
};

/// Share of the leftover main-axis space in a Row or Column. Type id "coil.weight".
class Weight final : public ParentDataModifier {
public:
    explicit Weight(float value) noexcept;
    [[nodiscard]] std::string_view typeId() const noexcept override;
    [[nodiscard]] float value() const noexcept;
};

/// Fails with InvalidArgument unless `value` is finite and > 0.
[[nodiscard]] ModifierResult weight(float value);

} // namespace coil::layout
```
```cpp
// modules/layout/include/coil/layout/linear_layout.hpp
namespace coil::layout {

enum class MainAxisAlignment { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
enum class CrossAxisAlignment { Start, Center, End, Stretch };

struct LinearLayoutOptions {
    float spacing = 0.0F; ///< Between neighbouring children, in DIPs.
    MainAxisAlignment mainAxis = MainAxisAlignment::Start;
    CrossAxisAlignment crossAxis = CrossAxisAlignment::Start;
};

/// Children left to right. Column is the same with the axes swapped (main axis vertical).
class Row final : public LayoutElement {
public:
    Row() = default;
    /// Fails with InvalidArgument (options unchanged) unless spacing is finite and >= 0.
    [[nodiscard]] Expected<void> setOptions(LinearLayoutOptions options);
    [[nodiscard]] const LinearLayoutOptions& options() const noexcept;
    /// Appends a child, which the row owns. Fails with InvalidArgument for a null child.
    [[nodiscard]] Expected<void> add(std::unique_ptr<LayoutElement> child);
    [[nodiscard]] std::size_t childCount() const noexcept;

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override;
    void arrangeContent(Rect contentBounds) override;
};

class Column final : public LayoutElement { /* same members as Row */ };

} // namespace coil::layout
```

### 4.2 Behavioral rules
"Main" is the row's horizontal axis (the column's vertical axis); "cross" is the other one. `n` is the
number of children; total spacing is `spacing × (n − 1)` (0 for no children).

1. **Chains.** `then` accepts both kinds. `layoutStages()` lists only layout modifiers, so parent data
   never changes the element's own measure or arrange (SPEC-0016 rules 3–4 run over `layoutStages()`).
   `find<T>()` returns the outermost match. `typeIds()` lists every modifier, in order.
2. **Child constraints, cross axis:** `0..crossMax` (loose), or for `Stretch` with a bounded `crossMax`,
   tight at `crossMax`.
3. **Unweighted children** (no `Weight`, or main axis unbounded) are measured first with main axis
   `0..∞`.
4. **Weighted children**, when the main maximum is bounded: leftover =
   max(0, mainMax − unweighted sizes − total spacing); each gets `leftover × weight / total weight` and
   is measured **tight** at that main size. With weights, the container's main size is `mainMax`.
   When the main maximum is unbounded, weights are ignored (rule 3).
5. **Container size:** main = sum of children's main sizes + total spacing (or `mainMax` under rule
   4); cross = largest child cross size; then constrained by the incoming constraints (SPEC-0016 rule 3).
6. **Arrange, main axis:** free = content main size − (children + total spacing). The first child
   starts at the content start plus a lead, and each next one after the previous plus spacing plus a gap:

   | Alignment | Lead | Extra gap |
   |-----------|------|-----------|
   | `Start` | 0 | 0 |
   | `Center` | free / 2 | 0 |
   | `End` | free | 0 |
   | `SpaceBetween` | 0 | max(free, 0) / (n − 1) (0 if n = 1) |
   | `SpaceAround` | max(free, 0) / (2n) | max(free, 0) / n |
   | `SpaceEvenly` | max(free, 0) / (n + 1) | max(free, 0) / (n + 1) |

   `Center` and `End` apply a negative free space (overflow) as is; the `Space*` modes then act like
   `Start`.
7. **Arrange, cross axis:** each child gets its measured cross size at `Start` (offset 0), `Center`
   ((content cross − child cross) / 2) or `End` (content cross − child cross); `Stretch` gives it the
   whole content cross size at offset 0. Main size is always the child's measured main size.
8. **`weight(v)`**: type id `"coil.weight"`; `Weight::value()` is `v`.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| `weight` with 0, a negative, NaN or infinite value | `InvalidArgument` |
| `setOptions` with a negative, NaN or infinite spacing | `InvalidArgument`; options unchanged |
| `add(nullptr)` | `InvalidArgument`; child count unchanged |
| No children | Size = incoming minimums |
| Weights under an unbounded main axis | Ignored (rule 4) |
| Children wider than the container | Overflow at the end (`Start`), negative lead (`Center`, `End`) |
| Several `Weight`s on one child | The outermost wins |

### 4.4 Threading and lifetime
As SPEC-0016. A `Row`/`Column` owns its children and destroys them with itself.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
Children are test `Leaf`s (as in SPEC-0016): `a` prefers 50×30, `b` 40×20, `c` 30×40. Bounds are
listed for `a`, `b`, `c`. Unless stated, spacing is 10 and the container is arranged at `(0, 0)` with
its measured size.

- **AC-1** — `Row` under `loose(500×100)` measures 140×40; bounds `(0, 0) 50×30`, `(60, 0) 40×20`,
  `(110, 0) 30×40`.
- **AC-2** — `Column` under `loose(500×500)` measures 50×110; bounds `(0, 0) 50×30`, `(0, 40) 40×20`,
  `(0, 70) 30×40`.
- **AC-3** — `Row` under `tight(290×40)` (free 150), children's x positions: `Start` 0, 60, 110;
  `Center` 75, 135, 185; `End` 150, 210, 260; `SpaceBetween` 0, 135, 260; `SpaceAround` 25, 135, 235;
  `SpaceEvenly` 37.5, 135, 222.5.
- **AC-4** — `Row` under `tight(140×60)`, children's y positions and heights: `Start` 0, 0, 0 with
  30, 20, 40; `Center` 15, 20, 10; `End` 30, 40, 20; `Stretch` 0, 0, 0 with heights 60, 60, 60.
- **AC-5** — `Row` with spacing 0 under `tight(290×50)`: `a` unweighted, `b` (10×10) with `weight(1)`,
  `c` (10×10) with `weight(2)`: it measures 290×50; bounds `(0, 0) 50×30`, `(50, 0) 80×10`,
  `(130, 0) 160×10`.
- **AC-6** — The same row under `unbounded()` ignores the weights: it measures 70×30, with `b` and `c`
  10 wide. A leaf whose only modifier is `weight(3)` measures its preferred size;
  `find<Weight>()` gives 2 for `c`'s chain (built `weight(2)` then `weight(5)`), and `nullptr` for `a`'s;
  `typeIds()` of `c`'s chain is `{"coil.weight", "coil.weight"}` and `layoutStages()` is empty.
- **AC-7** — `weight` fails with `InvalidArgument` for 0, −1, NaN and ∞; `setOptions` fails for spacing
  −1, NaN and ∞ and keeps the previous options; `add(nullptr)` fails and the child count stays.
- **AC-8** — Overflow: `Row` under `tight(100×40)` (free −40): `Start` x 0, 60, 110; `Center` −20, 40,
  90; `SpaceBetween` 0, 60, 110.

## 6. Test Plan
| AC | Test name | Type | Fakes/adapters used |
|----|-----------|------|---------------------|
| AC-1 | `LinearLayout.AC1_RowWrapsChildren` | unit | test `Leaf` |
| AC-2 | `LinearLayout.AC2_ColumnWrapsChildren` | unit | test `Leaf` |
| AC-3 | `LinearLayout.AC3_MainAxisAlignment` | unit | test `Leaf` |
| AC-4 | `LinearLayout.AC4_CrossAxisAlignment` | unit | test `Leaf` |
| AC-5 | `LinearLayout.AC5_WeightsShareLeftoverSpace` | unit | test `Leaf` |
| AC-6 | `LinearLayout.AC6_WeightsAreParentData` | unit | test `Leaf` |
| AC-7 | `LinearLayout.AC7_InvalidInputsAreRejected` | unit | test `Leaf` |
| AC-8 | `LinearLayout.AC8_OverflowingChildren` | unit | test `Leaf` |

All in a new `linear_layout_test.cpp`. SPEC-0016 and SPEC-0017's tests must still pass unchanged:
they exercise the chain through the API this spec changes (§8).

## 7. Non-Functional Requirements
- Measuring a row allocates nothing beyond what its children need (no per-measure containers).

## 8. Architectural Impact
- Module: `layout` only. New: `Modifier`, `ParentDataModifier`, `Weight`, `weight`,
  `ModifierChain::layoutStages`/`find`, `MainAxisAlignment`, `CrossAxisAlignment`,
  `LinearLayoutOptions`, `Row`, `Column`.
- **API change (source compatible):** `ModifierResult` now holds `std::unique_ptr<Modifier>`, and
  `LayoutModifier` derives from `Modifier`. Existing factories and custom modifiers (including those in
  SPEC-0016/0017's frozen tests) compile unchanged, since `unique_ptr<Derived>` converts. The `friend`
  between `ModifierChain` and `LayoutElement` is removed.
- `find<T>()` uses `dynamic_cast` (RTTI). Plugins share the toolchain (ADR-0011), so this works across
  the plugin boundary.

## 9. Open Questions
All resolved as recommended (2026-11-02, Emilio Pomares).
- **Q1 — Finding parent data.** *Recommended:* typed lookup, `find<Weight>()`, through RTTI. The
  alternative, lookup by type id plus a cast, avoids RTTI but lets a wrong cast compile.
- **Q2 — Several weights on one child.** *Recommended: the outermost wins* (the first one added), as for
  any modifier, the outermost speaks first.
- **Q3 — Weighted children fill their share.** *Recommended: yes*, measured tight (Compose's default,
  Flutter's `Expanded`). A "may be smaller" variant (Flutter's `Flexible`) can come later.
- **Q4 — Weights under an unbounded main axis** (for example a row inside a horizontal scroll view).
  *Recommended: ignore them* (Compose). Flutter treats it as an error.
- **Q5 — Row size.** *Recommended:* wrap the children (rule 5), except that a row with weights fills its
  bounded maximum. A "fill" behaviour without weights is a `size` modifier or a tight parent.
- **Q6 — Overflow.** *Recommended:* as rule 6. Nothing is shrunk silently; clipping is for rendering.
- **Q7 — Children.** *Recommended:* `Row`/`Column` own their children (`unique_ptr`), `add` appends.
  Removal and reordering arrive with the widget tree.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0018): …`):
   - **Production refactor needed to compile:** the `Modifier` base, `ParentDataModifier`,
     `ModifierResult` change, `layoutStages()` with the engine using it, and the `friend` removed.
     These must keep SPEC-0016/0017's 158 tests green (they are pure refactoring, shown green in Red).
   - **Stubs:** `find` returns `nullptr`; `weight` returns a `Weight` and never fails; `Row`/`Column`
     store nothing, measure 0×0 and arrange nothing; `setOptions` and `add` succeed without storing.
   - **Tests:** `linear_layout_test.cpp`. Lint the test code.
   - **Expected Red:** all 8 fail on assertions; the other 158 pass.
2. **Green:** `find`; `Weight` validation; a shared flex algorithm parameterized by axis, used by
   `Row` and `Column`.
3. **Refactor:** `tools/check.sh`; `ctest --preset asan`; EPIC-004 spec table.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/layout/include/coil/layout/modifier.hpp`, `src/modifier.cpp` | modify | Modifier kinds, `layoutStages`, `find`, `weight` |
| `modules/layout/src/layout_element.cpp`, `include/…/layout_element.hpp` | modify | Run `layoutStages()`; no `friend` |
| `modules/layout/include/coil/layout/linear_layout.hpp`, `src/linear_layout.cpp` | create | `Row`, `Column`, options |
| `modules/layout/CMakeLists.txt`, `tests/CMakeLists.txt` | modify | New sources |
| `modules/layout/tests/linear_layout_test.cpp` | create | AC-1–AC-8 |
| `docs/epics/EPIC-004-layout-engine.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **The refactor in Red** touches code covered by frozen tests. Running all 158 earlier tests in the Red
  commit proves it changed no behaviour.
- **Floating-point positions:** AC-3's values (37.5, 222.5) are exact in `float`; other test values
  are integers.

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
- [ ] AC-1 → `LinearLayout.AC1_RowWrapsChildren`
- [ ] AC-2 → `LinearLayout.AC2_ColumnWrapsChildren`
- [ ] AC-3 → `LinearLayout.AC3_MainAxisAlignment`
- [ ] AC-4 → `LinearLayout.AC4_CrossAxisAlignment`
- [ ] AC-5 → `LinearLayout.AC5_WeightsShareLeftoverSpace`
- [ ] AC-6 → `LinearLayout.AC6_WeightsAreParentData`
- [ ] AC-7 → `LinearLayout.AC7_InvalidInputsAreRejected`
- [ ] AC-8 → `LinearLayout.AC8_OverflowingChildren`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-02 | Spec and implementation plan created (Draft) | |
| 2026-11-02 | Q1–Q7 confirmed as recommended; spec and plan approved | Emilio Pomares |
