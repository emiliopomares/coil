# SPEC-0028: Intents and the default input mapping (`core`)

| Field | Value |
|-------|-------|
| Status | Done |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `coil::core` (new; already in `tools/layering.json`) |
| Epic | [EPIC-007](../epics/EPIC-007-input-and-intents.md) (spec 3) |
| Related ADRs | ADR-0007 (device-agnostic input, incl. Amendment 1), ADR-0004 (layering), ADR-0005 (widgets react to intents) |
| Depends on | SPEC-0026, SPEC-0027 (raw input events) |

> **Scope Gate checklist**
> - [x] One concern: raw input events become device-agnostic intents through rebindable bindings
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 1 module (`core`, new); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
Widgets must never see "Enter key" or "left mouse button" (ADR-0007): they handle **intents** such as
`Activate`, `Cancel` or `Navigate`. This spec creates the `core` module with the first intent types
and an `InputMapper` that turns raw input (SPEC-0026, SPEC-0027) into intents through bindings an
application or user can change. Routing intents to widgets is EPIC-008.

## 2. Scope
- New module `modules/core`: `intent.hpp` (intent types), `input_mapper.hpp` (`KeyChord`,
  `IntentAction`, `Bindings`, `InputMapper`).
- Default bindings for keys; fixed mapping for pointer and text events.
- Unit tests only (no adapters involved).

## 3. Out of Scope
- Routing, focus, hit testing, `PointerEnter/Leave` (EPIC-008 specs 5–6).
- Drag intents, `ContextMenu`, `Zoom`, `TextEdit` (IME), custom namespaced intents (later specs).
- Keyboard shortcuts such as Cmd/Ctrl+Z and a "primary shortcut modifier" (with commands, EPIC-009).
- Loading bindings from configuration files (with the `json` module).
- Resolving Space as `Activate` versus text in a text field (§9 Q5; with text widgets).
- The intent-printing example (EPIC-007 spec 4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/core/include/coil/core/intent.hpp
namespace coil::core {

enum class NavigationDirection : std::uint8_t { Up, Down, Left, Right, Next, Previous };

struct Activate { platform::WindowId window; };
struct Cancel   { platform::WindowId window; };
struct Navigate { platform::WindowId window; NavigationDirection direction; };
/// Scroll content at `position` (DIPs) by wheel steps, as PointerScrolled defines them.
struct Scroll   { platform::WindowId window; Point position; float deltaX = 0; float deltaY = 0; };
struct PointerMove { platform::WindowId window; platform::PointerId pointer; Point position; };
struct PointerDown { platform::WindowId window; platform::PointerId pointer; platform::PointerButton button; Point position; };
struct PointerUp   { platform::WindowId window; platform::PointerId pointer; platform::PointerButton button; Point position; };
/// Text to insert, UTF-8.
struct TextInput { platform::WindowId window; std::string text; };

/// What widgets receive (ADR-0007). Every alternative has operator==.
using Intent = std::variant<Activate, Cancel, Navigate, Scroll, PointerMove, PointerDown, PointerUp,
                            TextInput>;

} // namespace coil::core
```
```cpp
// modules/core/include/coil/core/input_mapper.hpp
namespace coil::core {

/// A key with an exact set of modifiers, e.g. Shift+Tab.
struct KeyChord { platform::Key key; platform::KeyModifiers modifiers{}; /* operator==, operator<=> */ };

/// What a key binding produces.
enum class IntentAction : std::uint8_t {
    Activate, Cancel,
    NavigateUp, NavigateDown, NavigateLeft, NavigateRight, NavigateNext, NavigatePrevious,
};

/// Key bindings: each chord maps to at most one action. A value type.
class Bindings {
public:
    /// The defaults of §4.2 rule 2.
    [[nodiscard]] static Bindings defaults();
    /// Binds `chord` to `action`, replacing any action it had.
    void bind(KeyChord chord, IntentAction action);
    /// Removes the chord's binding, if any.
    void unbind(KeyChord chord);
    /// The chord's action, if bound.
    [[nodiscard]] std::optional<IntentAction> find(KeyChord chord) const;
    friend bool operator==(const Bindings&, const Bindings&) = default;
};

/// Turns raw input into intents (ADR-0007 layer 2). Stateless apart from its bindings.
class InputMapper {
public:
    explicit InputMapper(Bindings bindings = Bindings::defaults());
    [[nodiscard]] const Bindings& bindings() const noexcept;
    void setBindings(Bindings bindings);
    /// The intents for one raw event: none, or one (rules 3–7).
    [[nodiscard]] std::vector<Intent> map(const platform::RawInputEvent& event) const;
};

} // namespace coil::core
```

### 4.2 Behavioral rules
1. Every intent carries the window of the raw event it came from.
2. **Default key bindings** (no modifiers unless shown):

   | Chord | Action |
   |-------|--------|
   | Enter, Space | `Activate` |
   | Escape | `Cancel` |
   | Tab | `NavigateNext` |
   | Shift+Tab | `NavigatePrevious` |
   | Up, Down, Left, Right | `NavigateUp`, `…Down`, `…Left`, `…Right` |

3. **Keys:** a `KeyPressed` whose chord (key and exact modifiers) is bound gives that action's intent.
   Chords that are not bound, and every `KeyReleased`, give nothing.
4. **Repeats:** a repeated `KeyPressed` gives its intent only for the six `Navigate…` actions;
   `Activate` and `Cancel` happen once per press.
5. **Pointer:** `PointerMoved`, `PointerPressed` and `PointerReleased` give `PointerMove`,
   `PointerDown` and `PointerUp` with the same pointer, button and position. They are not bound:
   whether a press-and-release is a click on something is decided by the widget that received them
   (EPIC-011 spec 2).
6. **Scroll:** `PointerScrolled` gives `Scroll` with the same position and deltas.
7. **Text:** `TextEntered` gives `TextInput` with the same text.
8. `setBindings` replaces all bindings; it affects later `map` calls only.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Bound key with extra modifiers (Ctrl+Enter) | No intent (exact match) |
| `bind` on a bound chord | Replaces its action |
| `unbind` on an unbound chord | No effect |
| Empty bindings | Keys give nothing; pointer, scroll and text still map |

### 4.4 Threading and lifetime
UI thread; `InputMapper` is a plain value-owning object.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
`AutomationService::perform` will inject intents directly (ADR-0015), bypassing this mapper.

## 5. Acceptance Criteria
- **AC-1** — With the default bindings, a `KeyPressed` of each chord in rule 2's table gives exactly
  that one intent, with the event's window (`Navigate` with the matching direction).
- **AC-2** — Ctrl+Enter, Shift+Escape, an unbound key (A) and every `KeyReleased` (Enter, Tab) give
  no intents.
- **AC-3** — A repeated press gives `Navigate` for Down and Tab, and nothing for Enter, Space and
  Escape.
- **AC-4** — `PointerMoved`, `PointerPressed` (Secondary) and `PointerReleased` (Secondary) give
  `PointerMove`, `PointerDown` and `PointerUp` with the same window, pointer, button and position;
  `PointerScrolled` (−1.5, 2) at (3, 4) gives `Scroll` with those values.
- **AC-5** — `TextEntered` "é" gives `TextInput` "é".
- **AC-6** — Rebinding: after `bind(F2 → Activate)`, `unbind(Space)` and `bind(Enter → Cancel)`,
  F2 gives `Activate`, Space gives nothing, Enter gives `Cancel`; `unbind` of an unbound chord changes
  nothing (`==` to before); `setBindings(Bindings{})` makes every key give nothing while pointer, scroll
  and text still map.
- **AC-7** — `Bindings::defaults()` contains exactly the eight chords of rule 2 (checked chord by chord
  with `find`, plus a chord not in the table returning `nullopt`), and two `defaults()` compare equal.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `InputMapper.AC1_DefaultKeyBindings` | unit | Table written out in the test |
| AC-2 | `InputMapper.AC2_ExactChordsAndReleases` | unit | |
| AC-3 | `InputMapper.AC3_OnlyNavigationRepeats` | unit | |
| AC-4 | `InputMapper.AC4_PointerAndScroll` | unit | |
| AC-5 | `InputMapper.AC5_Text` | unit | |
| AC-6 | `InputMapper.AC6_Rebinding` | unit | |
| AC-7 | `Bindings.AC7_Defaults` | unit | |

## 7. Non-Functional Requirements
- `map` allocates only its result vector.

## 8. Architectural Impact
- New module `core` (planned in OVERVIEW §3 and `tools/layering.json`). It uses `platform` and
  `foundation` here; `layout`, `render` and `tasks` come with the widget tree.
- `core` depends on `platform` types (`RawInputEvent`, `WindowId`); intents reuse `PointerId`,
  `PointerButton` and `WindowId` from `platform` rather than redefining them.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — Tab: `Navigate(Next)` or `FocusNext`.** ADR-0007 lists both `Navigate{dir}` and
  `FocusNext/Prev`. *Recommended:* one intent, `Navigate` with `Next`/`Previous` directions (as
  EPIC-007 says), so focus code handles all navigation in one place.
- **Q2 — Is a click `Activate`?** ADR-0007's example maps `Pointer.PrimaryClick` to `Activate`, but
  a click needs the widget under the pointer for both press and release. *Recommended:* the mapper
  emits `PointerDown`/`PointerUp` only; the button turns down-and-up-inside into activation (EPIC-011
  spec 2, as its Done-when already says).
- **Q3 — How bindings are named.** *Recommended:* typed `KeyChord → IntentAction` for now; text names
  (`"Keyboard.Enter"`) arrive with configuration files, as a serialization of the same table.
- **Q4 — Which presses repeat.** *Recommended:* only navigation (rule 4): holding Enter must not press
  a button many times.
- **Q5 — Space in a text field.** The mapper gives both `Activate` (from the key) and `TextInput`
  (from the text). *Recommended:* leave both; when text widgets arrive, the focused widget that takes
  text suppresses key bindings that produce text (a routing rule, specified then).
- **Q6 — Timestamps on intents.** *Recommended:* none yet; added when double-click or drag thresholds
  need them.

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0028): …`):
   - **Stubs:** module `core` with the headers of §4.1; `Bindings` stores nothing (`find` → nullopt,
     `defaults()` empty); `map` returns no intents.
   - **Tests:** AC-1 to AC-7.
   - **Expected Red:** all seven fail on assertions; the other 253 pass.
2. **Green:** `Bindings` as a sorted vector (or `std::map`) of chords; the defaults; `map` by
   `std::visit`.
3. **Refactor:** `tools/check.sh`; `asan`; `tsan`.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/core/{CMakeLists.txt, include/coil/core/intent.hpp, include/coil/core/input_mapper.hpp, src/input_mapper.cpp}` | create | Module |
| `modules/core/tests/{CMakeLists.txt, .clang-tidy, input_mapper_test.cpp}` | create | AC-1–AC-7 |
| `CMakeLists.txt` | modify | `add_subdirectory(modules/core)` |
| `docs/epics/EPIC-007-input-and-intents.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Mapping in `platform`:** rejected by ADR-0007 (mapping is `core`, layer 2).
- **One generic intent record** (name + payload): loses exhaustive `std::visit` and typed fields, as
  ADR-0007 Amendment 1 argued for raw events.

## 11. Definition of Done
- [x] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [x] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [x] Full test suite passes on `dev`, `asan` and `tsan`
- [x] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [x] Every AC ticked below with its proving test
- [x] Public API documented

### AC verification
- [x] AC-1 → `InputMapper.AC1_DefaultKeyBindings`
- [x] AC-2 → `InputMapper.AC2_ExactChordsAndReleases`
- [x] AC-3 → `InputMapper.AC3_OnlyNavigationRepeats`
- [x] AC-4 → `InputMapper.AC4_PointerAndScroll`
- [x] AC-5 → `InputMapper.AC5_Text`
- [x] AC-6 → `InputMapper.AC6_Rebinding`
- [x] AC-7 → `Bindings.AC7_Defaults`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q6 approved as recommended | Emilio Pomares |
| 2026-11-07 | Red: all seven fail on assertions (no intents, empty bindings); the other 253 pass. AC-2 first passed against the stub, since it only asserted absences; before committing Red it gained a positive control (plain Enter gives `Activate`), so its absences prove rejection | — |
| 2026-11-07 | Green (`5fae969`): 260/260 on `dev`, `asan` and `tsan`; tests unchanged since Red (`7550d2d`); all gates pass. Status → Done | — |
