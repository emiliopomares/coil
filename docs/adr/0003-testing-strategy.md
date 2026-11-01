# ADR-0003: Testing framework and strategy
Status: Accepted (2026-11-01)

## Decision
- **GoogleTest + GoogleMock**: mature, runs on every target, integrates with CTest
  (`gtest_discover_tests`), and supports death tests for contract violations.
- **Prefer fakes to mocks.** Every port ships with a deterministic fake in `modules/<m>/testing/`
  (built as `coil::<m>_testing`, linked only by tests; amended 2026-11-01, SPEC-0004: fakes never live
  in `backends/`): `FakeClock`, `FakeInputSource`, `FakeWindowSystem`,
  `RecordingRenderBackend` (stores DrawLists for assertions), `LoopbackTransport`,
  `InlineTaskScheduler`. Use mocks only to verify interaction protocols.
- **Test levels.**
  - *Unit* (in `modules/<m>/tests/`): headless and deterministic; no OS, GPU, network or sleeps.
  - *Integration* (in `tests/integration/`): several modules plus fakes, e.g. intent → command →
    remote loopback.
  - *Backend* (in `backends/<b>/tests/`): real adapters; may need a display or GPU and run on
    labeled CI runners.
  - *Visual golden tests* (later): render via real backends to images and compare with a tolerance.
    Needs its own ADR.
- Test names include the AC they prove: `TEST(Slider, AC2_ClampsValueToRange)`.
- TDD integrity rules are in CLAUDE.md §3.

## Consequences
+ Because the core is entirely behind ports, almost all behavior is unit-testable headlessly.
− Fakes must be maintained alongside ports. A fake that drifts from real behavior is a bug.
