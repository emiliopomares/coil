# ADR-0007: Device-agnostic input via intents
Status: Accepted (2026-11-01)

## Context
Coil must never assume a keyboard or mouse. Gamepads, touch, pens, VR controllers and remote
frontends are all valid input sources.

## Decision
Three layers:
1. **`InputSource` port** emits `RawInputEvent`s: `{DeviceId, DeviceKind, ControlId, value/state,
   timestamp}`. Device kinds are open-ended. Pointers carry a `PointerId`.
2. **`InputMapper`** (core) turns raw events into **intents** using data-driven, user-rebindable
   bindings (e.g. `Keyboard.Enter`, `Gamepad.South` and `Pointer.PrimaryClick` all map to `Activate`).
3. **Widgets handle intents only.** Core intents: `Activate`, `Cancel`, `Navigate{dir}`,
   `FocusNext/Prev`, `Scroll{delta}`, `PointerEnter/Leave/Move/Down/Up{pointerId, button-role}`,
   `DragBegin/Update/End`, `TextInput{utf8}`, `TextEdit{op}` (IME composition, caret moves),
   `ContextMenu`, `Zoom{factor}`. The set is extensible via namespaced custom intents.
- Routing: focus path for non-positional intents; hit-test path for positional intents; bubbling up the
  tree until a widget returns `Reply::handled()`. Pointer capture is supported.
- **Focus navigation** (directional and tab order) is a first-class core feature, so a UI can be driven
  entirely without a pointer.

## Consequences
+ The same widget code works with gamepad, touch and remote drivers. Tests inject intents directly.
− Widgets cannot read "is key X down". Modifier-like needs are expressed as intents or input-context
  queries (`InputContext::isActive("MultiSelect")`).
