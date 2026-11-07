# SPEC-0027: Raw keyboard and text-input events

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `coil::platform` (event types, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-007](../epics/EPIC-007-input-and-intents.md) (spec 2) |
| Related ADRs | ADR-0007 (incl. Amendment 1: typed raw input events), ADR-0010 (UI thread) |
| Depends on | SPEC-0026 (`InputSource`, shared queue) |

> **Scope Gate checklist**
> - [x] One concern: the platform reports key presses and releases (with modifiers) and entered text, per window
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 areas (`platform`, `backends/platform_sdl3`); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
The mapping layer (EPIC-007 spec 3) needs keys for intents: Enter and Space → `Activate`, Tab →
`Navigate(next)`, Escape → `Cancel`, arrows → `Navigate`. Text fields later need the text the user
typed, which is not the same thing as keys (dead keys, layouts, IMEs). This spec adds both to the
`InputSource` port as new event types, with Coil's own key names so bindings never depend on SDL.

## 2. Scope
- New raw events `KeyPressed`, `KeyReleased`, `TextEntered`; the `Key` enum and `KeyModifiers`.
- SDL adapter: translate key and text events; start text input on every window it creates.
- Contract suite: KB1–KB4 in `InputSourceContract`, with new traits (`key`, `text`).
- SDL-specific test of the key table.

## 3. Out of Scope
- Intents and bindings (EPIC-007 spec 3).
- IME composition (`TextEdit`), candidate windows, on-screen keyboards, clipboard.
- Physical key positions (scancodes) for games; key state queries ("is Shift down?", ADR-0007 forbids
  them for widgets).
- Turning text input on and off per focused widget (§9 Q4).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/input_source.hpp (additions)
namespace coil::platform {

/// Keys by their meaning in the current keyboard layout (SPEC-0027 Q1).
enum class Key : std::uint8_t {
    A, B, /* … */ Z,
    Digit0, /* … */ Digit9,
    F1, /* … */ F12,
    Enter, Escape, Backspace, Tab, Space,
    Left, Right, Up, Down, Home, End, PageUp, PageDown, Insert, Delete,
    Shift, Control, Alt, Super,   // either side (Q3); Super is Command on macOS, Windows key elsewhere
};

/// Modifier keys held when the event happened.
struct KeyModifiers {
    bool shift = false; bool control = false; bool alt = false; bool super = false;
    friend constexpr bool operator==(KeyModifiers, KeyModifiers) = default;
};

struct KeyPressed  { WindowId window; Key key; KeyModifiers modifiers; bool repeat = false; InputTimestamp time{}; };
struct KeyReleased { WindowId window; Key key; KeyModifiers modifiers; InputTimestamp time{}; };
/// Text the user entered (one character or a composed string), UTF-8. Never empty.
struct TextEntered { WindowId window; std::string text; InputTimestamp time{}; };

using RawInputEvent = std::variant<PointerMoved, PointerPressed, PointerReleased, PointerScrolled,
                                   KeyPressed, KeyReleased, TextEntered>;

} // namespace coil::platform
```
All new structs have `operator== = default`.

### 4.2 Behavioral rules
1. A key going down gives `KeyPressed` for the window that has keyboard focus; auto-repeat gives
   further `KeyPressed` with `repeat = true`; going up gives `KeyReleased`.
2. `key` is the key's meaning in the current layout (on AZERTY, the key that types "a" is `Key::A`).
   Keys outside the enum are dropped (no event).
3. `modifiers` is the state when the event happened, either side counting (left or right Shift).
4. Text the platform produces gives one `TextEntered` per platform text event, its UTF-8 kept
   byte for byte. Empty text gives no event. Text and key events for the same keystroke both arrive,
   in the platform's order (usually key first).
5. **SDL:** every window it creates has text input started (`SDL_StartTextInput`), so text arrives
   without any call from the application (§9 Q4).
6. As for pointer events (SPEC-0026): order is kept across all input kinds, each event is returned
   once, timestamps do not decrease, events for windows that are not live are dropped, and the
   window-event queue is not affected.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Key not in the enum (e.g. media keys) | Dropped |
| Key event with no focused window (SDL `windowID` 0) | Dropped |
| Empty text event | Dropped |
| Invalid UTF-8 from the platform | Passed through unchanged (validation is the text widget's job) |

### 4.4 Threading and lifetime
Unchanged (UI thread).

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable.

## 5. Acceptance Criteria
Contract tests run on the fake and on SDL's `dummy` driver; the SDL trait pushes real
`SDL_EVENT_KEY_DOWN/UP` and `SDL_EVENT_TEXT_INPUT` events.

- **AC-1 (KB1, contract)** — Pressing and releasing Enter gives `KeyPressed{Enter}` then
  `KeyReleased{Enter}`, with no modifiers and `repeat == false`.
- **AC-2 (KB2, contract)** — Z pressed with Control and Shift gives `modifiers {shift, control}`;
  A with Alt and Super gives `{alt, super}`; a repeated press of A gives `repeat == true`.
- **AC-3 (KB3, contract)** — Text "é" then "漢字" gives two `TextEntered` with exactly those UTF-8
  bytes.
- **AC-4 (KB4, contract)** — A key press in A, a pointer move in B and text in A arrive in that order;
  keyboard events for a window destroyed before `pollInput()` are dropped.
- **AC-5 (SDL)** — Each of the 67 keys in the enum is translated from its SDL keycode (`SDLK_A` →
  `Key::A`, `SDLK_RETURN` → `Enter`, `SDLK_LSHIFT` and `SDLK_RSHIFT` → `Shift`, …); a key outside
  the table (`SDLK_VOLUMEUP`) and a key event with `windowID` 0 are dropped; an empty text event is
  dropped.
- **AC-6 (SDL)** — A window created by the SDL window system has text input active
  (`SDL_TextInputActive`).

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `InputSourceContract/{Fake,Sdl3}.KB1_KeyPressAndRelease` | unit / integration | New traits `key(system, id, Key, modifiers, down, repeat)`, `text(system, id, utf8)` |
| AC-2 | `…KB2_ModifiersAndRepeat` | | |
| AC-3 | `…KB3_TextIsUtf8` | | |
| AC-4 | `…KB4_OrderAcrossDevicesAndDrops` | | |
| AC-5 | `SdlInputSource.SK1_KeyTable` | integration | Table written out in the test |
| AC-6 | `SdlInputSource.SK2_TextInputIsStarted` | integration | |

The contract helpers from SPEC-0026 (`withoutTimeOrDevice`, the event printer) are extended to the new
alternatives; their behaviour for pointer events does not change (§10.3).

## 7. Non-Functional Requirements
- None beyond SPEC-0026's.

## 8. Architectural Impact
- `RawInputEvent` gains three alternatives (ADR-0007 Amendment 1 foresaw this). `TextEntered` holds a
  `std::string`, so the variant is no longer trivially copyable.
- No new edge.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — Layout keys or physical keys.** *Recommended:* layout keys (SDL keycodes). Shortcuts like
  Ctrl+Z follow the letter printed on the key in the user's layout; physical positions (for WASD in
  games) are a later addition if the editor needs them.
- **Q2 — Which keys.** *Recommended:* the 67 above: everything the default bindings and common editor
  shortcuts need. Others are dropped until a spec adds them; the enum's `std::uint8_t` has room.
- **Q3 — Left and right modifiers.** *Recommended:* one key each (`Shift`, `Control`, `Alt`,
  `Super`), either side, and modifiers as four flags.
- **Q4 — When text input is on.** SDL only sends text events after `SDL_StartTextInput(window)`, and
  on phones that shows the on-screen keyboard. *Recommended:* start it for every window now (desktop
  only matters today); the focus system (EPIC-008 spec 6) switches it per focused text widget later,
  through a port function added then.
- **Q5 — macOS Command vs Control.** *Recommended:* report them as they are (`super` and `control`);
  mapping "primary shortcut modifier" (Command on macOS, Control elsewhere) is the binding layer's job
  (spec 3).

---

## 10. Implementation Plan

### 10.1 Steps
1. **Red** (commit `test(0027): …`):
   - **Stubs:** the new types and alternatives; the SDL adapter does not translate them yet and does
     not start text input. The fake needs no change (`injectInput` already queues any event).
   - **Tests:** KB1–KB4 with the new traits; SK1, SK2. Contract helpers extended.
   - **Expected Red:** on Sdl3, KB1–KB4 fail (4); SK1 and SK2 fail (2). On the fake, KB1–KB4 **pass**
     with no production change, because the fake's `injectInput` already stores any event (SPEC-0026
     rule 9); they are recorded as passing in Red. The other 243 pass.
2. **Green:** key table, modifier and text translation, `SDL_StartTextInput` on create.
3. **Refactor:** `tools/check.sh`; `asan`; `tsan`; `leaks` on the SDL tests.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/include/coil/platform/input_source.hpp` | modify | Types |
| `modules/platform/tests/contract/coil/platform/input_source_contract.hpp` | modify | KB1–KB4, helpers |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | Fake traits |
| `backends/platform_sdl3/src/sdl_window_system.cpp` | modify | Translation, text input |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | SDL traits, SK1, SK2 |
| `docs/epics/EPIC-007-input-and-intents.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Changing SPEC-0026's contract helpers.** `withoutTimeOrDevice` clears `pointer`, which keyboard
  events don't have; it gains an `if constexpr` for that. The printer gains cases. Neither changes a
  SPEC-0026 assertion; the Red diff shows it.
- **Pushed text events:** `SDL_TextInputEvent::text` is a pointer; the trait points it at static
  strings, which outlive the event.

## 11. Definition of Done
- [x] Red phase: new tests shown failing on assertions where expected (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes on `dev`, `asan` and `tsan`; `leaks` 0 for SDL tests
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented

### AC verification
- [ ] AC-1 → `InputSourceContract/{Fake,Sdl3}.KB1_KeyPressAndRelease`
- [ ] AC-2 → `InputSourceContract/{Fake,Sdl3}.KB2_ModifiersAndRepeat`
- [ ] AC-3 → `InputSourceContract/{Fake,Sdl3}.KB3_TextIsUtf8`
- [ ] AC-4 → `InputSourceContract/{Fake,Sdl3}.KB4_OrderAcrossDevicesAndDrops`
- [ ] AC-5 → `SdlInputSource.SK1_KeyTable`
- [ ] AC-6 → `SdlInputSource.SK2_TextInputIsStarted`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan and Q1–Q5 approved as recommended | Emilio Pomares |
| 2026-11-07 | Red: on Sdl3, KB1–KB4 fail (empty input) and SK1–SK2 fail (empty input; text input not active), all on assertions; on the fake, KB1–KB4 pass with no production change, as planned; the other 243 pass. SPEC-0026's contract helpers extended as in §10.3, and its garbled header comment rewritten | — |
