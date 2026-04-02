# Task

## Project State

- Project: `sunray-core`
- Platform: Raspberry Pi 4B plus STM32F103 based Alfred mower controller
- Runtime status: Backend builds and test suite is green on current `master`
- Frontend status: `webui-svelte` builds, but dependencies are local install artifacts and should not be treated as canonical source
- Documentation status: bootstrap phase restarted; this file is the current task anchor
- Safety status: stop paths, watchdog behavior, dock safety, battery critical handling, and GPS degradation are central review areas

## Context Snapshot

`sunray-core` is the Pi-side runtime and systems layer for Alfred. The codebase already contains core runtime logic, HAL abstractions, Linux platform drivers, navigation, local planner pieces, WebSocket/MQTT interfaces, tests, and low-level probes. The repo is in a deliberate cleanup phase: legacy docs were removed, the runtime and tests were recently stabilized, and a fresh documentation baseline is being created.

## Phase 1 - Baseline Discovery

### Task 1.1 System Overview

- Goal: describe runtime topology, responsibilities, and operating modes
- Primary inputs: `main.cpp`, `core/Robot.cpp`, `core/WebSocketServer.cpp`, `core/MqttClient.cpp`, `hal/*`, `platform/*`
- Deliverable: `SYSTEM_OVERVIEW.md`
- Exit criteria: Pi responsibilities, STM32 responsibilities, and handoff boundaries are explicit

### Task 1.2 Project Map

- Goal: map module boundaries, entry points, and repo shape
- Primary inputs: `CMakeLists.txt`, `core/`, `hal/`, `platform/`, `tests/`, `tools/`, `webui-svelte/`
- Deliverable: `PROJECT_MAP.md`
- Exit criteria: high-risk files and unknowns are called out

### Task 1.3 Hardware Map

- Goal: capture confirmed and likely bindings without inventing facts
- Primary inputs: `hal/HardwareInterface.h`, serial driver code, platform I/O, tests, docs
- Deliverable: `HARDWARE_MAP.md`
- Exit criteria: each major signal has source and confidence

### Task 1.4 Safety Map

- Goal: model stop, recovery, and degraded-mode paths
- Primary inputs: `core/Robot.cpp`, `core/op/*`, `core/navigation/*`, tests
- Deliverable: `SAFETY_MAP.md`
- Exit criteria: immediate stop, controlled stop, and recovery are separated

### Task 1.5 Build And Deployment Notes

- Goal: explain local build, Alfred deployment, and rollback concerns
- Primary inputs: root `CMakeLists.txt`, `webui-svelte/package.json`, probes, docs
- Deliverable: `BUILD_NOTES.md`
- Exit criteria: reproducible local build path and deployment unknowns documented

## Phase 2 - Static Analysis

### Task 2.1 Static Bug Sweep

- Goal: capture current and suspected failures with evidence level
- Deliverable: `BUG_REPORT.md`
- Scope: runtime logic, safety transitions, planner edge cases, communication resilience

### Task 2.2 Improvement Backlog

- Goal: gather pragmatic improvements with clear prioritization
- Deliverable: `IMPROVEMENT_BACKLOG.md`
- Scope: diagnostics, reliability, deployment safety, testability

### Task 2.3 Future Features

- Goal: define realistic extensions without mixing them into current bug work
- Deliverable: `FUTURE_FEATURES.md`
- Scope: safety, navigation, serviceability, remote operations, maintenance

## Phase 3 - Controlled Execution

### Task 3.1 Prioritized Fix Execution

- Start with safety and correctness defects
- Require proof path: failing scenario, minimal fix, verification
- Avoid bundling unrelated refactors into safety patches

### Task 3.2 Verifier Review

- Run adversarial review after each non-trivial change
- Confirm build impact, regression surface, and safety implications

### Task 3.3 Documentation Consolidation

- Promote stable findings into the root docs and memory files
- Remove stale assertions rather than letting duplicates accumulate

## Task Execution Template

### Title

- Short name

### Goal

- What success looks like

### Inputs

- Code paths
- Test cases
- Runtime evidence

### Method

1. Discover
2. Verify assumptions
3. Change minimally
4. Rebuild and retest
5. Update docs

### Output

- Files changed
- Evidence
- Residual risk

## Task History Template

| Date | Task | Status | Evidence | Follow-up |
| --- | --- | --- | --- | --- |
| 2026-04-02 | Repo cleanup and runtime test stabilization | Done | Backend build, frontend build, `217/217` tests | Fresh documentation baseline |
