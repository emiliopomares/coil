# EPIC-007: Input and intents

| Field | Value |
|-------|-------|
| Status | Approved |
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
| 1 | [SPEC-0026 Raw pointer events (`InputSource` port)](../specs/0026-raw-pointer-events.md) | Done | EPIC-005 spec 3 |
| 2 | Raw keyboard and text-input events (port, fake, SDL3, contract) | Planned | 1 |
| 3 | Intent types and the default device→intent mapping (`core`) | Planned | 2 |
| 4 | Example: print intents from real input (manual check) | Planned | 3 |

## Notes
- ADR-0007 (device-agnostic input). OVERVIEW §3 puts the mapping in `core`. Spec 3 creates the
  `core` module, which is already in `tools/layering.json`.
- **Decided in spec 1 (SPEC-0026):** a separate `InputSource` port (ADR-0007), implemented by the
  same object as the `WindowSystem` in each adapter, because SDL has one event queue. Raw events are
  typed (ADR-0007 Amendment 1).

## Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-01 | Epic proposed (roadmap) | |
| 2026-11-07 | Epic approved (roadmap to Milestone 2) | Emilio Pomares |
| 2026-11-07 | Spec 1 is SPEC-0026 | Emilio Pomares |
