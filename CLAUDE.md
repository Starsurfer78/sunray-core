# CLAUDE.md
# sunray-core Runtime Context Router

## Mission
Work on **sunray-core**, the production operating system stack for Alfred autonomous mower hardware.

Platform:
- Raspberry Pi 4B
- STM32F103
- PlatformIO
- C++17
- RTK GPS
- MQTT
- Home Assistant

This is a **safety-critical robotics system**.

Primary priorities:
1. Safety
2. Functional correctness
3. Deterministic runtime behavior
4. Minimal diffs
5. Hardware confidence tracking
6. Verification before completion
7. Cost-efficient context loading

Never invent undocumented hardware facts.

---

## Core Working Law
Always follow:

**Describe → Prove → Change**

Meaning:
1. describe current state
2. prove assumptions from code, logs, docs, or measurements
3. only then change implementation

No exceptions for safety-critical paths.

---

## Mandatory Workflow
For every non-trivial task:

1. Read `TASK.md`
2. Identify active phase and task
3. Select matching `ctx-profile`
4. Load only matching markdown context
5. Inspect impacted modules
6. Execute the smallest safe change
7. Run verification logic
8. Update:
   - `TASK.md`
   - relevant `.memory/*`
   - maps if architecture knowledge changed
9. Trigger verifier pass for relevant changes

---

## Context Routing Profiles

### ctx-profile: discovery
Load only:
- `TASK.md`
- `SYSTEM_OVERVIEW.md`
- `PROJECT_MAP.md`
- `.claude-prompts/repo-scanner.md`

### ctx-profile: hardware
Load only:
- `TASK.md`
- `HARDWARE_MAP.md`
- `.memory/hardware-pinout.md`
- `.memory/runtime-knowledge.md`
- `.claude-prompts/hardware-mapper.md`

### ctx-profile: safety
Load only:
- `TASK.md`
- `SAFETY_MAP.md`
- `.memory/safety-findings.md`
- `.claude-prompts/firmware-safety.md`
- `.claude-prompts/safety-auditor.md`

### ctx-profile: RTK
Load only:
- `TASK.md`
- `.memory/rtk-context.md`
- `.claude-prompts/rtk-integration.md`

### ctx-profile: MQTT
Load only:
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.memory/known-issues.md`
- `.claude-prompts/mqtt-protocol.md`

### ctx-profile: HA
Load only:
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.claude-prompts/ha-integration.md`

### ctx-profile: bugfix
Load only:
- `TASK.md`
- `BUG_REPORT.md`
- `.memory/known-issues.md`
- `.claude-prompts/bug-hunting.md`
- `.claude-prompts/verifier.md`

### ctx-profile: future
Load only:
- `TASK.md`
- `IMPROVEMENT_BACKLOG.md`
- `FUTURE_FEATURES.md`
- `.memory/architecture-decisions.md`
- `.claude-prompts/architect-future.md`

---

## Safety Rules
The following paths are always high-risk:
- motor enable
- emergency stop
- watchdog
- UART reconnect
- docking logic
- battery critical
- GPS invalid handling
- sensor failover
- recovery after communication loss

For these:
- state risks explicitly
- document assumptions
- require verifier pass
- update `SAFETY_MAP.md` if logic changed

---

## Hardware Confidence Rules
All hardware assumptions must be tagged as:
- FACT
- INFERENCE
- UNKNOWN

Never upgrade confidence without evidence from:
- code
- logs
- schematics
- measurements
- proven runtime behavior

---

## Cost & Token Discipline
Use strict task-scoped loading.

### Hard Rules
- never load all markdown files
- maximum:
  - 1 task file
  - 2 analysis files
  - 2 memory files
  - 1 specialist prompt
- reuse summaries before reloading raw files
- prefer memory snapshots over repeated historical scans

### Budget
- normal task: <= 25k tokens
- safety review: <= 30k
- discovery batch: <= 35k
- verifier: <= 20k

### Hard Stop
Split into follow-up task when:
- more than 3 subsystems are touched
- task drifts into redesign
- RTK + MQTT + safety mix in one patch
- repo scan turns into implementation work

Update `TASK.md` with follow-up tasks.

Reference:
`.memory/cost-optimization.md`

---

## Completion Rules
A task is DONE only if:
- requested scope is completed
- affected files were reviewed
- risks are stated
- hidden assumptions are eliminated
- relevant memory files updated
- `TASK.md` updated
- verifier prompt passed where required

Otherwise use:
- PARTIAL
- BLOCKED
- UNVERIFIED