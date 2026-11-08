# Coil product scope: what "close to Qt" means here

> Status: **Recorded** (2026-11-08), from the assessment of 2026-11-07. Not binding like an ADR: it
> guides which epics are proposed and in what order. Revisit it at each milestone.

## 1. Target
Coil is a toolkit for **professional desktop editors**: video editors, node-based FX and compositing
tools, and game-engine editors. The bar is "comparable to Qt **for these applications**", not
Qt's breadth. Precedents built for the same purpose: Unreal's Slate, Godot's editor UI, Blender's UI.

## 2. What we do not chase
These are left to the application or to other libraries. Proposing an epic for any of them needs a
reason tied to §1.
- Qt's non-UI modules: networking, SQL, XML, multimedia playback, serial ports, Bluetooth, and so on.
- A declarative UI language (QML) and its runtime.
- Video decoding, audio and media pipelines. Coil only *displays* frames the app provides (§4, need 2).
- A meta-object system (`moc`); modern C++ replaces it.
- Mobile platforms and form-factor adaptation.
- Native look and feel per OS; Coil has its own themeable look (§4, need 5).

## 3. Strengths to protect
These are where Coil can be better than Qt for its target. Changes that weaken them need an ADR.
- **Testability:** ports with fakes everywhere; headless and deterministic (ADR-0003, ADR-0004).
- **Device-agnostic intents:** gamepad- and keyboard-only editor UIs work by design (ADR-0007).
- **Async commands and remote frontends:** an editor driving a separate game process is designed in
  (ADR-0008, ADR-0014).
- **Automation tree and agent access:** a base for OS accessibility and agent-driven testing
  (ADR-0015, ADR-0016, ADR-0017).
- **Renderer independence with GPU embedding** (ADR-0009).

## 4. Future needs, ranked by risk
"Covered by" names the epic or ADR that plans each one; *none yet* means a future epic.

| # | Need | Why these apps need it | Covered by |
|---|------|------------------------|------------|
| 1 | Frame model: damage tracking, paint caching, continuous frames with vsync | Timelines playing at 60 fps; graphs with thousands of items | EPIC-015 (ADR A, specs 2–3); EPIC-003 |
| 2 | GPU backend and streamed content (video frames, engine viewports), ideally without copying | The core of video editors and engine editors | EPIC-015 (ADR B); ADR-0009 `GpuSurface`; GPU backend epic *none yet* |
| 3 | Scale: virtualized lists, trees and tables; canvas widgets drawing thousands of their own items, with pan and zoom | Node graphs, timelines, outliners, asset browsers | EPIC-015 (ADR D, specs 4–6) |
| 4 | Text: shaping, bidirectional text, font fallback, IME composition, editable and rich text, code-editor performance | Every app; scripting and expression editors | EPIC-010 (basics); follow-ups *none yet* |
| 5 | Styling and theming shared by built-in and custom widgets | Consistent dark editor themes; no hard-coded looks | EPIC-015 (ADR C); implementation *none yet* |
| 6 | Desktop basics: multiple windows, drag and drop (including OS file drops), clipboard, cursor shapes, native menus, file dialogs | Undocked panels, asset import, copy/paste | Docking: ADR-0012; the rest *none yet* |
| 7 | Undo/redo and an editor property model (inspectors) | Every editor is built on them | ADR-0008 (direction); *none yet* |
| 8 | OS accessibility bridges (UI Automation, NSAccessibility, AT-SPI) on the automation tree | Accessibility requirements; screen readers | ADR-0015 (foundation); *none yet* |
| 9 | Performance budgets as gates, with benchmarks | Keeps needs 1–3 from regressing | EPIC-015 (ADR E, spec 1) |

## 5. Effort and pacing
- As of 2026-11-07, after one week: about 29 specs and about 10k lines in `modules/` and `backends/`.
- A credible editor toolkit is estimated at 100–200k lines and several hundred specs: many months
  of steady work. The hardest areas (GPU, text, virtualization) are still ahead and will go slower
  than the early specs.
- **Rule of thumb:** settle a decision that shapes every widget (an ADR) before writing many widgets
  that depend on it. Prove the architecture where the target apps stress it (EPIC-015's stress demo)
  before broadening the standard widget set.

## 6. Open decisions
- **Throwaway spikes** (exploratory prototypes outside `modules/` and `backends/`, never merged) for
  GPU, text and caching designs. They would need a change to CLAUDE.md §1. Undecided.
