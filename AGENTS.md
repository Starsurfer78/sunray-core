# AGENTS.md
# sunray-core Agent Operating Manifest

## Repository Identity
sunray-core is the production operating system stack for Alfred autonomous mower hardware.

Platform:
- Raspberry Pi 4B
- STM32F103
- PlatformIO
- C++17
- RTK GPS
- MQTT
- Home Assistant integration

This repository is:
- hardware-coupled
- timing-sensitive
- safety-relevant
- production-active

---

## Global Operating Constraints
Never:
- invent hardware bindings
- flatten module boundaries
- rewrite large files without need
- change safety paths casually
- mix discovery and implementation in one large step
- allow task scope drift across >3 subsystems

Always:
- prefer minimal diffs
- track confidence
- preserve deterministic behavior
- isolate high-risk changes
- persist architectural discoveries

---

## Required Execution Workflow
For every task:

1. read `TASK.md`
2. identify active phase
3. select matching ctx-profile
4. load only required markdown files
5. inspect impacted code paths
6. plan smallest viable change
7. verify
8. update:
   - `TASK.md`
   - affected maps
   - relevant `.memory/*`
9. run verifier review if relevant

Guiding law:

**Describe → Prove → Change**

---

## ctx-profile Routing

### discovery
- `TASK.md`
- `SYSTEM_OVERVIEW.md`
- `PROJECT_MAP.md`
- `.claude-prompts/repo-scanner.md`

### hardware
- `TASK.md`
- `HARDWARE_MAP.md`
- `.memory/hardware-pinout.md`
- `.claude-prompts/hardware-mapper.md`

### safety
- `TASK.md`
- `SAFETY_MAP.md`
- `.memory/safety-findings.md`
- `.claude-prompts/firmware-safety.md`
- `.claude-prompts/safety-auditor.md`

### RTK
- `TASK.md`
- `.memory/rtk-context.md`
- `.claude-prompts/rtk-integration.md`

### MQTT
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.memory/known-issues.md`
- `.claude-prompts/mqtt-protocol.md`

### HA
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.claude-prompts/ha-integration.md`

### bugfix
- `TASK.md`
- `BUG_REPORT.md`
- `.memory/known-issues.md`
- `.claude-prompts/bug-hunting.md`
- `.claude-prompts/verifier.md`

### future
- `TASK.md`
- `IMPROVEMENT_BACKLOG.md`
- `FUTURE_FEATURES.md`
- `.memory/architecture-decisions.md`
- `.claude-prompts/architect-future.md`

---

## Safety Escalation Rules
Mandatory verifier review when touching:
- motor control
- stop paths
- watchdog
- GPS failsafe
- docking
- battery protection
- UART resync
- MQTT emergency commands
- HA exposed control entities

Update:
- `SAFETY_MAP.md`
- `.memory/safety-findings.md`

---

## Context Budget Policy
Use strict token discipline.

### Limits
- standard task <= 25k
- discovery <= 35k
- safety <= 30k
- verifier <= 20k

### Required Behavior
- use per-module context loading
- use cached summaries first
- avoid full historical reloads
- split large discovery into batches
- persist summaries into `.memory/`

Reference:
`.memory/cost-optimization.md`

---

## Definition of Done
Done means:
- requested issue solved
- relevant checks passed
- risks documented
- no undocumented assumptions added
- maps updated if understanding changed
- memory updated
- task history updated
- verifier result attached if high-risk