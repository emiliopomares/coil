# EPIC-007: Input and intents

| Field | Value |
|-------|-------|
| Status | Proposed |
| Date | 2026-11-01 |
| Origin | Roadmap to a button (2026-11-01) |

## Goal
User input reaches Coil as device-agnostic **intents** (ADR-0007). Raw pointer and keyboard events
come from the platform in DIPs, and a configurable mapping layer turns them into `Pointer*`,
`Activate`, `Navigate`, `Cancel`, `Scroll` and `TextInput` intents. Nothing above that layer knows
which device was used.

## Done when
- The platform reports raw pointer events (move, button down/up, wheel) and keyboard events (key
  down/up, text input) in DIPs, with a fake and SDL implementations passing a contract suite.
- Intent types and a default mapping exist (for example, Enter/Space → `Activate`, Tab →
  `Navigate(next)`, Escape → `Cancel`, wheel → `Scroll`, pointer → `PointerMove/Down/Up`), and the
  bindings can be changed in tests.
- An example prints the intents produced by real input (manual check).

## Out of scope
- Routing intents to widgets, hit testing and focus (EPIC-008).
- Gamepads, touch and pen, IME composition, clipboard, cursor shapes (later epics).

## Specs (in dependency order)
| # | Spec | Status | Depends on |
|---|------|--------|------------|
| 1 | Raw pointer events in DIPs (port, fake, SDL3, contract) | Planned | EPIC-005 spec 3 |
| 2 | Raw keyboard and text-input events (port, fake, SDL3, contract) | Planned | 1 |
| 3 | Intent types and the default device→intent mapping (`core`) | Planned | 2 |
| 4 | Example: print intents from real input (manual check) | Planned | 3 |

## Notes
- ADR-0007 (device-agnostic input). OVERVIEW §3 puts the mapping in `core`. Spec 3 creates the
  `core` module, which is already in `tools/layering.json`.
- **Open question for spec 1:** whether raw input extends the `WindowEvent` stream or is a separate
  `InputSource` port (OVERVIEW lists `InputSource`).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
