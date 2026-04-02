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
- Status: Done on 2026-04-02
- Evidence: startup path traced from `main.cpp`; runtime loop and attachment points verified in `Robot`

### Task 1.2 Project Map

- Goal: map module boundaries, entry points, and repo shape
- Primary inputs: `CMakeLists.txt`, `core/`, `hal/`, `platform/`, `tests/`, `tools/`, `webui-svelte/`
- Deliverable: `PROJECT_MAP.md`
- Exit criteria: high-risk files and unknowns are called out
- Status: Done on 2026-04-02
- Evidence: root and subdir CMake files, frontend manifest, repo tree, and runtime entry points inspected

### Task 1.3 Hardware Map

- Goal: capture confirmed and likely bindings without inventing facts
- Primary inputs: `hal/HardwareInterface.h`, serial driver code, platform I/O, tests, docs
- Deliverable: `HARDWARE_MAP.md`
- Exit criteria: each major signal has source and confidence
- Status: Done on 2026-04-02
- Evidence: `SerialRobotDriver`, Linux I2C/UART code, `Config.cpp`, `Robot.cpp`, and `alfred/firmware/rm18.ino` cross-checked for signal ownership and confidence

### Task 1.4 Safety Map

- Goal: model stop, recovery, and degraded-mode paths
- Primary inputs: `core/Robot.cpp`, `core/op/*`, `core/navigation/*`, tests
- Deliverable: `SAFETY_MAP.md`
- Exit criteria: immediate stop, controlled stop, and recovery are separated
- Status: Done on 2026-04-02
- Evidence: `Robot`, `SerialRobotDriver`, active ops, `LineTracker`, STM32 firmware watchdog/timeout logic, and safety-related tests reviewed for stop, degrade, and recovery paths

### Task 1.5 Build And Deployment Notes

- Goal: explain local build, Alfred deployment, and rollback concerns
- Primary inputs: root `CMakeLists.txt`, `webui-svelte/package.json`, probes, docs
- Deliverable: `BUILD_NOTES.md`
- Exit criteria: reproducible local build path and deployment unknowns documented
- Status: [x] Done on 2026-04-02
- Evidence: root and submodule CMake files, frontend manifest, installer and flash scripts, `main.cpp`, and Alfred switchover/test/flashing docs cross-checked for build, startup, deploy, and rollback behavior

## Phase 2 - Static Analysis

### Task 2.1 Static Bug Sweep

- Goal: capture current and suspected failures with evidence level
- Deliverable: `BUG_REPORT.md`
- Scope: runtime logic, safety transitions, planner edge cases, communication resilience
- Status: Done on 2026-04-02
- Evidence: static review of `Robot`, `MqttClient`, `SerialRobotDriver`, active ops, `StateEstimator`, and runtime wiring in `main.cpp`; findings recorded in `BUG_REPORT.md` with `Critical / High / Medium / Low / Plausible` severity buckets

### Task 2.2 Improvement Backlog

- Goal: gather pragmatic improvements with clear prioritization
- Deliverable: `IMPROVEMENT_BACKLOG.md`
- Scope: diagnostics, reliability, deployment safety, testability
- Status: Done on 2026-04-02
- Evidence: prioritized backlog derived from `BUG_REPORT.md`, discovery maps, build/deploy notes, runtime memory, and known-issues register; each item in `IMPROVEMENT_BACKLOG.md` includes category, problem, evidence, benefit, modules, risk, effort, priority, and dependencies

### Task 2.3 Future Features

- Goal: define realistic extensions without mixing them into current bug work
- Deliverable: `FUTURE_FEATURES.md`
- Scope: safety, navigation, serviceability, remote operations, maintenance
- Status: Done on 2026-04-02
- Evidence: feature roadmap derived from discovery maps, safety and hardware evidence, build/deploy notes, bug findings, improvement backlog, RTK context, MQTT topic notes, and architecture memory; every item in `FUTURE_FEATURES.md` is mapped to existing modules, constraints, blockers, and confidence

## Phase 3 - Controlled Execution

### Task 3.1 Prioritized Fix Execution

- Start with safety and correctness defects
- Require proof path: failing scenario, minimal fix, verification
- Avoid bundling unrelated refactors into safety patches
- Status: In progress on 2026-04-02
- Control surface: `PRIORITY_MATRIX.md`
- Rule: execute only one active `P1` item at a time

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
| 2026-04-02 | Task 1.1 System Overview | Done | `main.cpp`, `Robot`, build files, runtime attachment points reviewed | Use for hardware and safety mapping |
| 2026-04-02 | Task 1.2 Project Map | Done | Repo tree, CMake targets, frontend manifest, high-risk file set documented | Proceed to Task 1.3 and 1.4 |
| 2026-04-02 | Task 1.3 Hardware Map | Done | `SerialRobotDriver`, `platform/I2C`, `platform/I2cMux`, `platform/PortExpander`, `Config.cpp`, `Robot.cpp`, `alfred/firmware/rm18.ino` | Proceed to safety-path mapping |
| 2026-04-02 | Task 1.4 Safety Map | Done | `Robot`, active ops, `LineTracker`, `SerialRobotDriver`, STM32 watchdog/motor-timeout logic, and safety-oriented tests reviewed | Use as basis for later bug sweep and fixes |
| 2026-04-02 | Task 1.5 Build And Deployment Notes | Done | `CMakeLists`, `package.json`, `main.cpp`, install/flash/check scripts, and Alfred flashing/test/switchover docs reviewed | Use for future deployment validation and rollback planning |
| 2026-04-02 | Task 2.1 Static Bug Sweep | Done | `Robot`, `MqttClient`, `SerialRobotDriver`, `DockOp`, `ChargeOp`, `UndockOp`, `MowOp`, `NavToStartOp`, `StateEstimator`, and `main.cpp` reviewed; findings documented in `BUG_REPORT.md` | Prioritize fixes before Task 2.2 backlog shaping |
| 2026-04-02 | Task 2.2 Improvement Backlog | Done | `IMPROVEMENT_BACKLOG.md` rebuilt from bug sweep, discovery maps, build/deploy notes, architecture notes, runtime memory, and known issues | Use as input for Task 3.1 prioritized fix execution |
| 2026-04-02 | Task 2.3 Future Features | Done | `FUTURE_FEATURES.md` rebuilt from architecture, hardware, safety, deployment, bug, backlog, RTK, and MQTT evidence; future constraints recorded in memory | Use roadmap only after P1/P2 correctness work is stabilized |
| 2026-04-02 | Phase 3.1 P1: external command serialization | Done | WebSocket and MQTT start/dock/stop/mission/setpos commands now enqueue into `Robot` and are executed on the control-loop thread; backend build and `217/217` tests green | Continue with next P1 fix: diagnostic stop-path hardening |
| 2026-04-02 | Phase 3.1 P1: diagnostic stop-path hardening | Done | Active diagnostics now cancel immediately on stop-button press before diagnostic PWM continues; focused diag safety test added; backend build and `218/218` tests green | Continue with next P1 fix: Pi-side communication-loss safety transition |
| 2026-04-02 | Phase 3.1 P1: Pi-side MCU communication-loss safety transition | Done | Post-startup MCU comm loss now latches into a local safe-state, zeros motor PWM, records `ERR_MCU_COMMS`, and enters `Error`; focused startup/no-false-positive plus mowing-loss tests added; backend build and `220/220` tests green | Continue with next P1 fix: MQTT reconnect and subscription recovery |
| 2026-04-02 | Phase 3.1 P1: MQTT reconnect and subscription recovery | Done | `MqttClient` now retries command-topic subscription after reconnect and subscribe failure, tracks command-channel health separately from socket liveness, and clears degraded subscription state on `MOSQ_ERR_NO_CONN`; backend build, focused MQTT tests, and `220/220` full-suite tests green | Continue with next P1 fix: charger-contact debounce and dock-state observability |
| 2026-04-02 | Phase 3.1 P1: charger-contact debounce and dock-state observability | Done | `SerialRobotDriver` now debounces charger-contact state across consecutive samples and telemetry exposes `charger_connected`; focused driver and WebSocket tests plus `221/221` full-suite tests green | Continue with next P1 fix: regression coverage for UART, MQTT, and dock-contact edge cases |
| 2026-04-02 | Phase 3.1 P1: regression coverage for UART, MQTT, and dock-contact edge cases | Done | Added repeated charger-flap driver regression coverage, Charge grace-expiry recovery coverage, and degraded MQTT enabled lifecycle coverage; backend build, focused verifier runs, and `224/224` full-suite tests green | `P1` lane clear; re-prioritize before opening `P2` |
| 2026-04-02 | Phase 3.1 P2: deployment and rollback verification hardening | Done | Added read-only `scripts/check_deploy_state.sh`, tightened `BUILD_NOTES.md` with deployment verification and recovery checklist steps, and verified with `bash -n` plus a real checker run that produced only evidence-based warnings on this machine | `P2` lane clear; next likely candidate is `IB-009` |
| 2026-04-02 | Phase 3.1 P2: configuration ownership cleanup | Done | Aligned `check_alfred_hw.sh` to `/etc/sunray-core/config.json`, extended `config.example.json` with active Alfred runtime defaults from `Config.cpp`, and added a config drift regression test; `225/225` full-suite tests green | `P2` lane clear; next likely candidate is `IB-007` |
| 2026-04-02 | Phase 3.1 P2: centralized runtime health telemetry | Done | Added compact Web telemetry fields for `runtime_health`, MCU connectivity/loss, GPS degradation latches, battery low/critical guards, recovery state, and watchdog notice state; focused `[ws]`, `[run][telemetry]`, `[run][safety]`, and `225/225` full-suite tests green | `P2` lane clear; next likely candidate is `IB-010` |
| 2026-04-02 | Phase 3.1 reprioritization after new dock findings | Done | New confirmed entries `BUG-HIGH-004` and `BUG-HIGH-005` were pulled into `IMPROVEMENT_BACKLOG.md` and `PRIORITY_MATRIX.md` as `IB-012` and `IB-013` with score, phase, risk, and confidence | Resume with `IB-012` as the next single active `P1` item |
| 2026-04-02 | Phase 3.1 P1: dock recovery requires RTK Fix | Done | `GpsWaitFixOp` now blocks dock-critical resume on RTK Float and only continues the dock path once RTK Fix returns; focused `[a5_gps][dock]`, `[a5_gps]`, and `226/226` full-suite tests green | Continue with next P1 fix: `IB-013` non-progressive dock retry detection |
| 2026-04-02 | Phase 3.1 P1: non-progressive dock retry detection | Done | `DockOp` now snapshots retry-start pose and escalates after two consecutive non-progressive retry completions instead of burning the full retry budget on a stalled approach; added a dedicated `e2x_dock` regression and hardened one existing GPS recovery test against timing jitter; `227/227` full-suite tests green | `P1` lane clear; re-prioritize before opening the next item |
| 2026-04-02 | Phase 3.1 reprioritization after new frontend operator bugs | Done | New confirmed entries `BUG-HIGH-006` and `BUG-HIGH-007` were pulled into `IMPROVEMENT_BACKLOG.md` and `PRIORITY_MATRIX.md` as `IB-014` and `IB-015` with score, phase, risk, and confidence | Resume with `IB-014` as the next single active `P1` item |
| 2026-04-02 | Phase 3.1 P1: visible frontend feedback for dropped WebSocket commands | Done | `sendCmd()` now raises a shared transient error notice when no active WebSocket send path exists, and `BottomPanel` renders that notice globally across main UI pages; `npm run check` and `npm run build` green | Continue with next P1 fix: `IB-015` keep mission card visible through recovery ops |
| 2026-04-02 | Phase 3.1 P1: mission card stays visible during recovery ops | Done | `MissionWidget` now keeps the active mission view visible through `GpsWait`, `EscapeReverse`, `EscapeForward`, and `WaitRain` while telemetry still carries a `mission_id`; `npm run check` and `npm run build` green | `P1` lane clear; re-prioritize before opening the next item |
| 2026-04-02 | Phase 3.1 reprioritization after GPS coast field insight | Done | EKF/IMU coast behavior, premature `MowOp` GPS-stop logic, config-only tuning candidate, future optical-flow path, and an ADR for bounded mowing GPS coast were split across runtime knowledge, bug report, backlog, feature roadmap, and architecture decisions | Resume with `IB-016` as the next single active `P1` item |
| 2026-04-02 | Phase 3.1 P1: GPS-loss config tuning for Alfred runtime baseline | Done | Raised Alfred runtime defaults to `gps_no_signal_ms=15000`, `gps_recover_hysteresis_ms=3000`, and `ekf_gps_failover_ms=20000`, added drift guards in `test_config.cpp`, and verified with `[config]` plus `227/227` full-suite tests green | Continue with next P1 fix: `IB-017` bounded mowing GPS coast on degraded EKF fusion |
| 2026-04-02 | Phase 3.1 reprioritization after stuck-detection and dock-retry review | Done | New confirmed findings for encoder-visible stall without a stuck safety transition and same-geometry dock retries were split into `BUG-HIGH-009`, `BUG-MED-006`, backlog items `IB-018` and `IB-019`, and rescored in the matrix | Resume with `IB-018` as the next single active `P1` item |
| 2026-04-02 | Phase 3.1 P1: encoder-based stuck detection and recovery dispatch | Done | `Robot` now monitors commanded-motion stalls via existing `stuck_detect_*` config keys, dispatches `onStuck()`, and routes `Mow`/`Dock` into `EscapeReverse` while `Undock` escalates to `Error`; focused `[a8_sim][safety]`, `[run][safety]`, `[config]`, and `231/231` full-suite tests green | Continue with next P1 fix: `IB-017` bounded mowing GPS coast on degraded EKF fusion |
| 2026-04-02 | Phase 3.1 P1: bounded mowing GPS coast on degraded fusion | Done | `Robot::monitorGpsResilience()` now allows a bounded `mow_gps_coast_ms` window during `Mow` when degraded fusion remains operational after prior GPS signal; focused `[a5_gps]`, `[config]`, and full-suite verification green | Proceed to `IB-019` lateral dock retry geometry |
| 2026-04-02 | Phase 3.1 P2: lateral-offset dock retry geometry | Done | `DockOp` now varies retry geometry laterally through `Map::retryDocking(..., lateralOffsetM)` with configurable `dock_retry_lateral_offset_m` and fallback to the original approach when offset planning fails; focused `[e2x_dock]`, `[config]`, and `233/233` full-suite tests green | `P1` lane clear; re-prioritize before next execution |
| 2026-04-02 | Phase 3.1 follow-up: stuck recovery exhaustion and incident counters | Done | Repeated unresolved stuck recovery now escalates deterministically to `Error` with `ERR_STUCK` / `stuck_recovery_exhausted`, history summary now exposes grouped `event_reason_counts`, `event_type_counts`, and `event_level_counts`, and verification passed with focused stuck/history checks plus `234/234` full-suite tests green | `P1` lane remains clear; next execution should start from the updated matrix |
| 2026-04-02 | Phase 3.1 off-matrix: History and statistics WebUI page | Done | Added a dedicated `Verlauf` view backed by `/api/history/events`, `/api/history/sessions`, `/api/statistics/summary`, and `/api/history/export`; `npm run check` and `npm run build` green | Ready for on-robot UI validation and then resume matrix-driven execution |
| 2026-04-02 | Phase 3.1 off-matrix: robust systemd stop handling | Done | Replaced asynchronous `std::signal` stop handling in `main.cpp` with a dedicated `sigwait()` thread for `SIGINT`/`SIGTERM`; backend build and `234/234` full-suite tests green | Validate on Alfred that `systemctl restart sunray-core` no longer times out in `stop-sigterm` |
| 2026-04-02 | Phase 3.1 off-matrix: Alfred STM32 SWD proof and OTA staging | Done | Alfred field validation now proves OpenOCD SWD probe success against the STM32 (`SWD DPIDR 0x1ba01477`, Cortex-M3 detected, target halt works); docs and roadmap updated to treat uploaded prebuilt `.bin` flashing as the next staged WebUI milestone | Next step: add WebUI `.bin` upload plus controlled flash trigger before any on-device STM build flow |
| 2026-04-02 | Phase 3.1 off-matrix: History event timestamps use wall clock in WebUI | Done | History events now persist `wall_ts_ms` beside runtime uptime, the WebUI `Verlauf` event stream shows real date/time instead of `Uptime 35s`, and the history DB schema advanced to version 3 with build, frontend build, and `234/234` tests green | Alfred will recreate or migrate the local history DB on first start after deploy; older event rows without wall-clock time are intentionally superseded by the new schema |
| 2026-04-02 | Phase 3.1 off-matrix: LED semantics for shutdown and MCU comm loss | Done | The top system-status LED now blinks red whenever the current cycle has no valid MCU communication, and shutdown now forces a solid-red system LED before the Alfred power-off grace path; build and `236/236` tests green | Deploy to Alfred and verify visually during STM disconnect and `systemctl stop/restart` paths |
| 2026-04-02 | Phase 3.1 off-matrix: SWD probe no longer leaves STM halted | Done | `scripts/flash_alfred.sh probe` now finishes with `reset run` instead of leaving the STM32 in `reset halt`; `bash -n` green and docs updated so Alfred probe runs no longer strand UART comms afterward | On Alfred, one manual STM reset or power cycle may still be needed once if an older probe already left the MCU halted |
| 2026-04-02 | Phase 3.1 off-matrix: STM uploaded-binary path in WebUI | Done | Settings now support `.bin` upload, uploaded-firmware metadata, and a controlled flash action for the uploaded STM binary; backend, installer sudoers, and WebUI all updated, with frontend build and `236/236` tests green | Deploy to Alfred and validate upload plus flash from `Idle` or `Charge`; on-Pi compile from the WebUI remains intentionally out of scope for now |
| 2026-04-02 | Phase 3.1 off-matrix: Diagnose `Motorfehler` no longer flickers between STM frame types | Done | `SerialRobotDriver` now keeps separate fast (`AT+M`) and durable (`AT+S`) motor-fault sources and ORs them, so a low 50 Hz motor-frame bit can no longer clear a still-latched summary fault between summary updates. This matches Alfred field behavior with `RM18 v1.1.20`; build, `[driver]`, and `237/237` full-suite tests green. | Deploy to Alfred and verify that Diagnose no longer blinks `Motorfehler Ein` while runtime remains stable in `Charge` or `Idle` after STM flashing. |
| 2026-04-02 | Phase 3.1 off-matrix: RM18 mow-fault input no longer trips while mower is inactive | Done | `rm18.ino` now gates `motorMowFault` on an actually active mower state (`mowSpeedSet` or brake-release ramp), because Alfred field testing showed the mow-fault input can stay low while the brake relay holds the spindle stopped. This prevents permanent `Motorfehler` indication in `Idle`/`Charge` after flashing `RM18 v1.1.20` while still keeping the fault active during real mower operation. | Rebuild and upload the new STM `.bin`, then verify on Alfred that Diagnose stays clear in `Charge` and still reports a real fault when the mower is commanded. |
| 2026-04-02 | Phase 3.1 off-matrix: operator wording clarified to `Mähmotorfehler` | Done | Diagnostics, recovery hints, and human-readable error texts now refer to the current Alfred signal more precisely as `Mähmotorfehler` instead of the broader `Motorfehler`. The STM firmware version was bumped to `RM18 v1.1.21` for the inactive-mower fault-gating fix. | Pull the latest Pi/UI code and upload the freshly built `RM18 v1.1.21` binary so wording and firmware semantics stay in sync. |
