# SPEC-NNNN: <Short imperative title>

| Field | Value |
|-------|-------|
| Status | Draft · Approved · In Progress · Done · Rejected · Superseded |
| Author | |
| Date | YYYY-MM-DD |
| Module(s) | e.g. `coil::core` (max 2) |
| Related ADRs | e.g. ADR-0007 |
| Depends on | e.g. SPEC-0003 |

> **Scope Gate checklist.** All boxes must be ticked before Status can become `Approved`.
> - [ ] One concern, explainable in two sentences
> - [ ] Every acceptance criterion is checkable by an automated test
> - [ ] ≤ 8 acceptance criteria (otherwise split)
> - [ ] Touches ≤ 2 modules; no new dependency or layering edge without an approved ADR
> - [ ] Out-of-scope section is filled in
> - [ ] Open Questions section is empty

---

## 1. Problem / Motivation
*Why is this needed? Which user or caller benefits? One short paragraph.*

## 2. Scope
*What this spec delivers, as a bullet list.*
-

## 3. Out of Scope
*What is explicitly NOT included, even if related. Future specs go here.*
-

## 4. Behavior Specification

### 4.1 Public API
*Exact declarations (signatures, types, header paths). This is the contract the tests will use.*
```cpp
// modules/<m>/include/coil/<m>/<file>.hpp
namespace coil::<m> {

} // namespace coil::<m>
```

### 4.2 Behavioral rules
*Normative, numbered statements using MUST / MUST NOT / MAY.*
1. `X` MUST …
2.

### 4.3 Error handling and edge cases
*Each invalid input or failure mode, and the resulting `coil::Error` or precondition violation.*
| Condition | Result |
|-----------|--------|
| | |

### 4.4 Threading and lifetime
*Which thread may call this? Who owns what? What happens on destruction while in use?*

### 4.5 Async behavior
*Delete only if no command handler, I/O or long-running work is involved.*
- Which operations are async? Concurrency policy (`Allow`/`IgnoreWhileRunning`/`CancelPrevious`/`Queue`)?
- Behavior on: completion · failure · cancellation · timeout · originating widget destroyed mid-flight
- Progress reporting and the observable state (`commands.<id>.running` etc.)

### 4.6 Automation and agent exposure
*Required for every widget or user-facing feature.*
| AutomationId pattern | Role | Name source | Value | Actions | Sensitive? |
|----------------------|------|-------------|-------|---------|------------|
| | | | | | |
- MCP exposure: new tools/resources? Capability tier (`observe`/`interact`/`command`)? `agentExposed`?

## 5. Acceptance Criteria
*Given / When / Then. Each one is independently testable and maps to at least one test in §6.*

- **AC-1** — Given …, when …, then ….
- **AC-2** —

## 6. Test Plan
| AC | Test name | Type (unit/integration) | Fakes/adapters used | Notes |
|----|-----------|-------------------------|---------------------|-------|
| AC-1 | `Suite.AC1_Description` | unit | `FakeClock` | |

*Edge and error cases from §4.3 must appear here as well.*

## 7. Non-Functional Requirements
*Only those that apply. Each must be measurable.*
- Performance budget (e.g. "layout of 10k widgets < 2 ms on reference machine"):
- Memory / allocation constraints:
- Platform-specific behavior:
- ABI / plugin compatibility impact:

## 8. Architectural Impact
- Layers/modules touched:
- New public types:
- New dependencies (third-party or inter-module): *none* / ADR-XXXX
- Does this change an existing public API or ABI? If yes, what is the migration?

## 9. Open Questions
*Must be empty before approval. Move resolved items into the relevant section above.*
-

---

## 10. Implementation Plan
*Filled in by Claude after the spec is approved, then approved separately by the user.*

### 10.1 Steps
1. Red: add stubs (`…`) and tests (`…`); expected failing tests: …
2. Green: implement …
3. Refactor: …

### 10.2 Files
| File | Action (create/modify) | Purpose |
|------|------------------------|---------|
| | | |

### 10.3 Risks / alternatives considered
-

## 11. Definition of Done
- [ ] Red phase: all new tests shown failing on assertions (output recorded in the Change Log)
- [ ] Green phase: tests unchanged since the Red commit (`git diff <red> HEAD -- '*/tests/*'` empty)
- [ ] Full test suite passes
- [ ] `clang-format` and `clang-tidy` clean; no new `NOLINT`
- [ ] Builds with warnings-as-errors on all CI targets
- [ ] Sanitizer presets pass
- [ ] Every AC ticked below with its proving test
- [ ] Public API documented (Doxygen comments on new public declarations)
- [ ] (Widgets/features) Semantic info verified by a test through `AutomationService`
- [ ] (Async) Completion, failure and cancellation each covered by a test

### AC verification
- [ ] AC-1 → `…`

## 12. Change Log
| Date | Change | Approved by |
|------|--------|-------------|
| | Spec created | |
