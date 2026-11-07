# SPEC-0026: Raw pointer events (`InputSource` port)

| Field | Value |
|-------|-------|
| Status | Approved |
| Author | Claude (for Emilio Pomares) |
| Date | 2026-11-07 |
| Module(s) | `coil::platform` (port, fake, contract suite); `backends/platform_sdl3` |
| Epic | [EPIC-007](../epics/EPIC-007-input-and-intents.md) (spec 1) |
| Related ADRs | ADR-0007 (device-agnostic input; Amendment 1: typed raw input events), ADR-0006 (DIPs), ADR-0003 (fakes), ADR-0010 (UI thread) |
| Depends on | SPEC-0006/0009 (window events, waiting), SPEC-0013 (DIPs and scale) |

> **Scope Gate checklist**
> - [x] One concern: the platform reports pointer movement, buttons and wheel, per window, in DIPs
> - [x] Every acceptance criterion is checkable by an automated test
> - [x] ≤ 8 acceptance criteria
> - [x] Touches 2 areas (`platform`, `backends/platform_sdl3`); no new dependency or layering edge
> - [x] Out-of-scope section is filled in
> - [x] Open Questions section is resolved (§9: all as recommended)

---

## 1. Problem / Motivation
Nothing in Coil sees input yet. ADR-0007 puts raw input behind an **`InputSource` port** whose events
an `InputMapper` turns into intents (EPIC-007 spec 3). This spec creates the port with its first
device class, the pointer (mouse, and for now touch or pen acting as a mouse): movement, buttons and
wheel, each tied to a window and positioned in DIPs.

SDL delivers window and input events in one queue, so the SDL adapter implements both ports on one
object, and neither port's polling may lose the other's events.

## 2. Scope
- Port `platform::InputSource` with `pollInput()`, and pointer event types.
- `FakeWindowSystem` also implements `InputSource`, with a test control to inject events.
- The SDL window system also implements `InputSource`; `platform_sdl3::inputSourceOf(WindowSystem&)`
  returns it.
- Contract suite `InputSourceContract` (IN1–IN6) for both, plus SDL- and fake-specific tests.

## 3. Out of Scope
- Keyboard and text input (EPIC-007 spec 2); intents and mapping (spec 3).
- Touch and pen as their own device classes (multi-touch, pressure); gamepads.
- Pointer capture, cursor shapes, relative mouse mode, enter/leave events.
- Fixing the Windows/X11 DIP gap (SPEC-0013 Q3; unchanged here).

## 4. Behavior Specification

### 4.1 Public API
```cpp
// modules/platform/include/coil/platform/input_source.hpp
namespace coil::platform {

/// Identifies a pointer device (a mouse, or a touch or pen acting as one). Stable while it exists.
struct PointerId { std::uint64_t value = 0; friend constexpr bool operator==(PointerId, PointerId) = default; };

/// Pointer buttons by role. Primary is the main (usually left) button.
enum class PointerButton : std::uint8_t { Primary, Secondary, Middle, Back, Forward };

/// Monotonic time of an event, from an adapter-defined origin; only differences are meaningful.
using InputTimestamp = std::chrono::nanoseconds;

struct PointerMoved {
    WindowId window; PointerId pointer; Point position; ///< DIPs, from the window's top-left.
    InputTimestamp time;
};
struct PointerPressed  { WindowId window; PointerId pointer; PointerButton button; Point position; InputTimestamp time; };
struct PointerReleased { WindowId window; PointerId pointer; PointerButton button; Point position; InputTimestamp time; };
/// Wheel or trackpad scroll, in wheel steps (fractional for precise devices). Positive y scrolls
/// content up (away from the user), positive x scrolls right, whatever the OS "natural" setting.
struct PointerScrolled { WindowId window; PointerId pointer; Point position; float deltaX = 0; float deltaY = 0; InputTimestamp time; };

/// A raw input event (ADR-0007). New device classes add alternatives.
using RawInputEvent = std::variant<PointerMoved, PointerPressed, PointerReleased, PointerScrolled>;

/// Port: raw input from the user's devices (ADR-0007). Used from the UI thread.
class InputSource {
public:
    virtual ~InputSource();
    // copy and move deleted
    /// Events since the previous call, in order, each once. Events for windows that are not live
    /// are dropped. Non-blocking.
    [[nodiscard]] virtual std::vector<RawInputEvent> pollInput() = 0;
protected:
    InputSource() = default;
};

} // namespace coil::platform
```
```cpp
// FakeWindowSystem (additions): also an InputSource.
    [[nodiscard]] std::vector<RawInputEvent> pollInput() override;
    /// Test control: queues `event` as if a device produced it. NotFound if its window is not live.
    [[nodiscard]] Expected<void> injectInput(RawInputEvent event);

// platform_sdl3 (addition)
/// The InputSource of an SDL window system; valid as long as `system`. InvalidArgument if `system`
/// was not created by createSdlWindowSystem.
[[nodiscard]] Expected<std::reference_wrapper<platform::InputSource>>
inputSourceOf(platform::WindowSystem& system);
```

### 4.2 Behavioral rules
1. `pollInput()` returns the pointer events that happened since the previous call, in the order they
   happened, each exactly once.
2. Positions are in DIPs relative to the window's client area, fractional values kept.
3. A press and its release carry the same button; buttons other than the five roles are dropped.
4. **Scroll direction** is normalized (rule in `PointerScrolled`); on SDL, a wheel event marked
   `SDL_MOUSEWHEEL_FLIPPED` is negated.
5. Events for windows that are not live when `pollInput()` runs, or that the system did not create,
   are dropped.
6. **Shared queue (SDL):** reading one port never loses the other's events. Window events read
   while polling input are kept for the next `pollEvents`/`waitEvents`, and input read while
   polling window events is kept for the next `pollInput`. Order within each port is preserved.
7. `waitEvents` may return early (with no window events) when only input arrived (allowed by
   SPEC-0009 rule 3); the caller then polls input.
8. Timestamps are non-decreasing in the order returned.
9. **Fake:** `injectInput` queues the event for `pollInput()`; it fails with `NotFound` (queuing
   nothing) if the event's window is not live.

### 4.3 Error handling and edge cases
| Condition | Result |
|-----------|--------|
| Event for a destroyed window | Dropped |
| Event for a window the system did not create (SDL) | Dropped |
| Unknown button (SDL button 6+) | Dropped |
| Flipped wheel (SDL) | Negated (rule 4) |
| `inputSourceOf` on a non-SDL system | `InvalidArgument` |
| `injectInput` for an unknown window | `NotFound` |

### 4.4 Threading and lifetime
UI thread. The input source lives as long as its window system.

### 4.5 Async behavior
Not applicable.

### 4.6 Automation and agent exposure
Not applicable (automation injects intents, not raw input, ADR-0015).

## 5. Acceptance Criteria
Contract tests run on the fake and on SDL's `dummy` driver. The SDL trait pushes real SDL mouse events
(`SDL_EVENT_MOUSE_MOTION`, `…_BUTTON_DOWN/UP`, `…_WHEEL`) for the window.

- **AC-1 (IN1, contract)** — With a window and nothing happening, `pollInput()` is empty.
- **AC-2 (IN2, contract)** — A move to (10.5, 20.25) in window A gives one `PointerMoved` for A at
  that position; a second `pollInput()` is empty.
- **AC-3 (IN3, contract)** — Pressing and releasing Primary, Secondary and Middle at (5, 6) gives
  `PointerPressed` and `PointerReleased` with the right button and position, in order.
- **AC-4 (IN4, contract)** — Scrolls of (0, 1) and (−2.5, 0) at (7, 8) give `PointerScrolled` with
  those deltas and that position.
- **AC-5 (IN5, contract)** — A move in A, a press in B and a scroll in A arrive in that order with
  their windows; timestamps are non-decreasing. Events for a window destroyed before `pollInput()`
  are dropped.
- **AC-6 (IN6, contract)** — Window and input events do not steal from each other: after a close
  request and a move, `pollInput()` then `pollEvents()` gives the move then the close request; the
  other way round (`pollEvents()` first) gives the same.
- **AC-7 (SDL)** — A flipped wheel event is negated; button 6 is dropped; a motion event for a
  window SDL created outside the port is dropped; `inputSourceOf` a fake system fails with
  `InvalidArgument`.
- **AC-8 (fake)** — `injectInput` for an unknown window fails with `NotFound` and queues nothing.

## 6. Test Plan
| AC | Test name | Type | Notes |
|----|-----------|------|-------|
| AC-1 | `InputSourceContract/{Fake,Sdl3}.IN1_NothingHappened` | unit / integration | New contract header |
| AC-2 | `…IN2_MoveIsReportedOnce` | | |
| AC-3 | `…IN3_ButtonsPressAndRelease` | | |
| AC-4 | `…IN4_ScrollDeltas` | | |
| AC-5 | `…IN5_OrderWindowsAndDrops` | | |
| AC-6 | `…IN6_PortsShareTheQueue` | | |
| AC-7 | `SdlInputSource.SI1_SdlSpecifics` | integration | |
| AC-8 | `FakeWindowSystem.FI1_InjectRequiresALiveWindow` | unit | |

## 7. Non-Functional Requirements
- No allocation per event beyond the event vectors.

## 8. Architectural Impact
- New port `InputSource` in `platform` (OVERVIEW §3 already lists it). No new edge.
- ADR-0007 Amendment 1 (§9 Q2), to be approved and committed before this spec.

## 9. Open Questions
Each has a recommendation. Say "as recommended" or name the ones you want changed.
- **Q1 — Who implements `InputSource`.** SDL has one queue for window and input events.
  *Recommended:* the same object implements both ports in each adapter (rule 6), reached through
  `inputSourceOf` on SDL and directly on the fake. A separate `FakeInputSource` would not exercise the
  shared-queue rule that SDL must follow.
- **Q2 — Event shape vs ADR-0007.** ADR-0007 sketches one generic record `{DeviceId, DeviceKind,
  ControlId, value/state, timestamp}`. *Recommended:* typed alternatives per device class
  (`PointerMoved`, `PointerPressed`, …), which carry positions and windows naturally and keep
  `std::variant` exhaustiveness. This changes the ADR's wording, so it becomes **ADR-0007
  Amendment 1**: "Raw input events are typed per device class (a `std::variant`); each carries its
  device id, window and timestamp. Data-driven bindings refer to them by name (for example
  `Pointer.Primary`)."
- **Q3 — Buttons.** *Recommended:* five roles (Primary, Secondary, Middle, Back, Forward); others
  dropped until something needs them.
- **Q4 — Scroll units and direction.** *Recommended:* wheel steps, fractional for trackpads, with a
  fixed direction (rule 4), so widgets never see the OS "natural scrolling" setting twice.
- **Q5 — Timestamps.** *Recommended:* monotonic nanoseconds from an adapter-defined origin (SDL's
  event timestamps); comparable within one source only.
- **Q6 — Touch and pen acting as a mouse.** SDL synthesizes mouse events from them.
  *Recommended:* pass them through as pointer events with their own pointer id, so touchscreens work
  now; their own device classes come later.
- **Q7 — Enter/leave.** *Recommended:* out of scope; hover is derived from moves and the widget tree
  (EPIC-008 spec 5). Leaving the window is added when hover needs it.

---

## 10. Implementation Plan

### 10.1 Steps
0. Commit ADR-0007 Amendment 1 once approved (`docs`).
1. **Red** (commit `test(0026): …`):
   - **Stubs:** port and types; the fake's `pollInput` returns nothing and `injectInput` succeeds
     without queuing; the SDL adapter implements `pollInput` returning nothing and keeps its current
     `pollEvents`; `inputSourceOf` returns the system.
   - **Tests:** contract IN1–IN6 with traits (`make`, `input`, `move`, `press`, `release`, `scroll`,
     `requestClose`); SI1; FI1.
   - **Expected Red:** IN2–IN6 fail on both (10), SI1 and FI1 fail; IN1 passes on both with the stub
     (kept as a regression test). The other 229 pass.
2. **Green:** fake input queue; SDL: one `pump()` that drains SDL into a window-event buffer and an
   input buffer, used by `pollEvents`, `waitEvents` and `pollInput`; translation of mouse events.
3. **Refactor:** `tools/check.sh`; `asan`; `tsan`; `leaks` on the SDL tests.

### 10.2 Files
| File | Action | Purpose |
|------|--------|---------|
| `modules/platform/{include/coil/platform/input_source.hpp, src/input_source.cpp}` | create | Port |
| `modules/platform/testing/…/fake_window_system.{hpp,cpp}` | modify | `pollInput`, `injectInput` |
| `modules/platform/tests/contract/coil/platform/input_source_contract.hpp` | create | IN1–IN6 |
| `modules/platform/tests/fake_window_system_test.cpp` | modify | Fake traits, FI1 |
| `backends/platform_sdl3/{include/…/sdl_window_system.hpp, src/sdl_window_system.cpp}` | modify | `InputSource`, `inputSourceOf`, shared queue |
| `backends/platform_sdl3/tests/sdl_window_system_test.cpp` | modify | SDL traits, SI1 |
| `docs/adr/0007-device-agnostic-input.md` | modify | Amendment 1 |
| `docs/epics/EPIC-007-input-and-intents.md` | modify | Spec table |

### 10.3 Risks / alternatives considered
- **Extending `WindowEvent` with input** would be simpler on SDL but contradicts ADR-0007's separate
  port and would make every window consumer see input.
- **Pushed SDL mouse events on the dummy driver:** SDL routes them by `windowID`, which the dummy
  driver supports; if a field (such as `which`) is rewritten by SDL, the trait sets it explicitly.

## 11. Definition of Done
- [x] ADR-0007 Amendment 1 committed
- [ ] Red phase: new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- ':(glob)**/tests/**'` empty)
- [ ] Full test suite passes on `dev`, `asan` and `tsan`; `leaks` 0 for SDL tests
- [ ] `clang-format`, `clang-tidy` and layering clean; no new `NOLINT`
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented

### AC verification
- [ ] AC-1 → `InputSourceContract/{Fake,Sdl3}.IN1_NothingHappened`
- [ ] AC-2 → `InputSourceContract/{Fake,Sdl3}.IN2_MoveIsReportedOnce`
- [ ] AC-3 → `InputSourceContract/{Fake,Sdl3}.IN3_ButtonsPressAndRelease`
- [ ] AC-4 → `InputSourceContract/{Fake,Sdl3}.IN4_ScrollDeltas`
- [ ] AC-5 → `InputSourceContract/{Fake,Sdl3}.IN5_OrderWindowsAndDrops`
- [ ] AC-6 → `InputSourceContract/{Fake,Sdl3}.IN6_PortsShareTheQueue`
- [ ] AC-7 → `SdlInputSource.SI1_SdlSpecifics`
- [ ] AC-8 → `FakeWindowSystem.FI1_InjectRequiresALiveWindow`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| 2026-11-07 | Spec and implementation plan created (Draft) | |
| 2026-11-07 | Spec, plan, Q1–Q7 and ADR-0007 Amendment 1 approved as recommended | Emilio Pomares |
