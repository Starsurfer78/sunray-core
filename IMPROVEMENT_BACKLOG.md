# Improvement Backlog

## Status

This backlog is derived from current repository evidence only.
It is intentionally grounded in:

- `BUG_REPORT.md`
- `SYSTEM_OVERVIEW.md`
- `PROJECT_MAP.md`
- `HARDWARE_MAP.md`
- `SAFETY_MAP.md`
- `BUILD_NOTES.md`
- `.memory/runtime-knowledge.md`
- `.memory/known-issues.md`

No wishlist items are included.
No production code was changed while preparing this backlog.

## Prioritization Model

| Priority | Meaning |
| --- | --- |
| P1 | Safety or runtime-correctness risk reduction |
| P2 | Reliability, recovery, and deployment robustness |
| P3 | Maintainability, observability, and operator clarity |

## Backlog Items

### IB-001

- Title: Serialize external commands onto the control-loop thread
- Status: Done on 2026-04-02
- Category: architecture clarity, communication resilience, safety transparency
- Problem:
  - WebSocket and MQTT callbacks currently call non-thread-safe `Robot` control methods directly from foreign threads.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-CRIT-001`
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-001`
  - [SYSTEM_OVERVIEW.md](/mnt/LappiDaten/Projekte/sunray-core/SYSTEM_OVERVIEW.md)
  - [PROJECT_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)
- Expected benefit:
  - Removes a confirmed cross-thread safety defect.
  - Makes command handling deterministic and reviewable.
- Affected modules:
  - `main.cpp`
  - `core/Robot.cpp`
  - `core/Robot.h`
  - `core/MqttClient.cpp`
  - `core/WebSocketServer.cpp`
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - None
- Execution note:
  - Implemented via thread-safe external command requests that are consumed inside `Robot::run()`.

### IB-002

- Title: Restore stop-button authority during active diagnostics
- Status: Done on 2026-04-02
- Category: diagnostics and observability, reliability and recovery, safety transparency
- Problem:
  - Diagnostic execution currently bypasses the normal stop-button path until the diagnostic exits or times out.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-002`
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
- Expected benefit:
  - Reduces operator-stop latency in a high-risk service mode.
  - Makes diagnostics safer for field and bench use.
- Affected modules:
  - `core/Robot.cpp`
  - `core/Robot.h`
  - diagnostic API paths in `core/WebSocketServer.cpp`
- Implementation risk:
  - Medium
- Effort estimate:
  - Low to Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - None
- Execution note:
  - Implemented by cancelling active diagnostics in `tickDiag()` when the stop button is pressed, before diagnostic PWM can continue driving the motors.

### IB-003

- Title: Add an explicit Pi-side communication-loss safety transition
- Status: Done on 2026-04-02
- Category: reliability and recovery, communication resilience, safety transparency
- Problem:
  - MCU disconnect is detected, but repository evidence does not show an immediate robot-level fault or controlled-stop transition based solely on comms loss.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-MED-002`
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Reduces dependence on undocumented MCU-side-only fail-safe behavior.
  - Makes comms-failure recovery visible and testable on the Pi side.
- Affected modules:
  - `hal/SerialRobotDriver/SerialRobotDriver.cpp`
  - `core/Robot.cpp`
  - `core/navigation/StateEstimator.cpp`
  - tests for runtime fault handling
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Should be coordinated with confirmed STM32 watchdog behavior from active hardware.
- Execution note:
  - Implemented as a latched post-startup MCU-comms-loss guard in `Robot` that enters the existing `Error` path without depending on a new service or driver abstraction.

### IB-004

- Title: Harden MQTT reconnect and command-subscription recovery
- Status: Done on 2026-04-02
- Category: communication resilience, service startup robustness, reliability and recovery
- Problem:
  - MQTT reconnect is automatic, but command subscription recovery is best-effort only and known MQTT disconnect behavior remains under investigation.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-003`
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-PLAUS-001`
  - [.memory/known-issues.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/known-issues.md)
- Expected benefit:
  - Improves remote-control reliability.
  - Makes MQTT health state observable instead of silently degraded.
- Affected modules:
  - `core/MqttClient.cpp`
  - `core/MqttClient.h`
  - runtime integration in `main.cpp`
  - MQTT-related tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Broker-side expectations and production configuration remain only partially documented.
- Execution note:
  - Implemented by adding explicit `connected` and `cmdSubscribed` state in `MqttClient` plus periodic re-subscribe attempts after reconnect or subscribe failure, without changing the external MQTT API surface.

### IB-005

- Title: Add charger-contact debounce and dock-state observability
- Status: Done on 2026-04-02
- Category: hardware abstraction quality, reliability and recovery, safety transparency
- Problem:
  - Dock truth is currently inferred from a charger-voltage threshold, creating risk of dock/contact flapping and incorrect critical-battery handling.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-MED-003`
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-MED-004`
  - [HARDWARE_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/HARDWARE_MAP.md)
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
- Expected benefit:
  - Reduces false dock/charge transitions.
  - Makes docking and low-battery recovery less sensitive to analog noise.
- Affected modules:
  - `hal/SerialRobotDriver/SerialRobotDriver.cpp`
  - `core/Robot.cpp`
  - `core/op/DockOp.cpp`
  - `core/op/ChargeOp.cpp`
  - `core/op/UndockOp.cpp`
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - True hardware dock-contact capability remains `UNKNOWN` in current repo evidence.
- Execution note:
  - Implemented as source-level charger-contact debounce in `SerialRobotDriver` plus explicit `charger_connected` telemetry so dock/charge truth is less flap-prone and easier to inspect.

### IB-006

- Title: Add fault-injection and soak coverage for UART, MQTT, and dock-contact edge cases
- Category: testability, communication resilience, reliability and recovery
- Problem:
  - Several documented failure modes are known or plausible, but current automated coverage does not prove these degraded paths.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md)
  - [PROJECT_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)
  - [.memory/known-issues.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/known-issues.md)
- Expected benefit:
  - Converts plausible field failures into repeatable regression checks.
  - Improves confidence before safety-path changes.
- Affected modules:
  - `tests/test_serial_driver.cpp`
  - `tests/test_mqtt_client.cpp`
  - runtime/op integration tests
  - optional hardware check scripts under `scripts/`
- Implementation risk:
  - Low
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Some scenarios need a test seam or harness for injected transport faults.
- Execution note:
  - Added regression coverage for repeated charger-contact flaps in `tests/test_serial_driver.cpp`, short-flap-versus-grace-expiry charge behavior in `tests/test_op_machine.cpp`, and degraded MQTT enabled start/stop lifecycle in `tests/test_mqtt_client.cpp`.
  - Residual gap: true injected UART transport-fault coverage still needs a dedicated seam or harness.

### IB-007

- Title: Centralize runtime health telemetry for watchdog, comms, and recovery state
- Status: Done on 2026-04-02
- Category: diagnostics and observability, safety transparency
- Problem:
  - Safety-relevant runtime state exists across watchdogs, GPS recovery, charger state, and MCU comms, but the documented telemetry contract does not expose a compact health model for operators or future debugging.
- Evidence / source:
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
  - [IMPROVEMENT_BACKLOG.md](/mnt/LappiDaten/Projekte/sunray-core/IMPROVEMENT_BACKLOG.md) previous draft emphasized watchdog telemetry
- Expected benefit:
  - Faster root-cause analysis in the field.
  - Better basis for UI, MQTT, and service monitoring.
- Affected modules:
  - `core/Robot.cpp`
  - `core/WebSocketServer.cpp`
  - `core/MqttClient.cpp`
  - telemetry docs and tests
- Implementation risk:
  - Low
- Effort estimate:
  - Medium
- Priority suggestion:
  - P2
- Blockers / dependencies:
  - More valuable after command-threading and comms-loss semantics are clarified.
- Execution note:
  - Implemented as a compact telemetry surface in `Robot` and `WebSocketServer` with `runtime_health`, MCU connectivity/loss, GPS degradation latches, battery low/critical guards, recovery state, and watchdog-notice state, backed by focused Robot and WebSocket regression tests.

### IB-008

- Title: Make deployment and rollback procedure explicit and verifiable
- Status: Done on 2026-04-02
- Category: deployment safety, service startup robustness
- Problem:
  - Productive deployment is partly documented, but service topology, rollback expectations, and target-device reality remain only partially proven.
- Evidence / source:
  - [BUILD_NOTES.md](/mnt/LappiDaten/Projekte/sunray-core/BUILD_NOTES.md)
  - [.memory/known-issues.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/known-issues.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Reduces deployment mistakes and recovery delays.
  - Makes release validation reproducible.
- Affected modules:
  - `scripts/install_sunray.sh`
  - deployment docs
  - possibly service-check scripts
- Implementation risk:
  - Low
- Effort estimate:
  - Low to Medium
- Priority suggestion:
  - P2
- Blockers / dependencies:
  - Requires target-device confirmation for the real installed service layout.
- Execution note:
  - Added a read-only verifier at `scripts/check_deploy_state.sh` for build artefacts, runtime files, `sunray-core.service`, and the documented `sunray` rollback anchor.
  - Tightened `BUILD_NOTES.md` with explicit deployment verification and recovery checklist steps grounded in repo scripts and docs.

### IB-009

- Title: Clarify and stabilize configuration ownership for Alfred runtime defaults
- Status: Done on 2026-04-02
- Category: configuration hygiene, architecture clarity
- Problem:
  - Critical defaults for UART, I2C, charger threshold, and runtime behavior are spread across config defaults, docs, and scripts, while some productive overrides are expected but not versioned.
- Evidence / source:
  - [BUILD_NOTES.md](/mnt/LappiDaten/Projekte/sunray-core/BUILD_NOTES.md)
  - [HARDWARE_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/HARDWARE_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Fewer silent config drifts.
  - Easier bring-up and safer field support.
- Affected modules:
  - `core/Config.cpp`
  - `config.example.json`
  - installer and hardware-check scripts
  - config-related tests
- Implementation risk:
  - Low
- Effort estimate:
  - Medium
- Priority suggestion:
  - P2
- Blockers / dependencies:
  - Must preserve documented Alfred-specific behavior.
- Execution note:
  - Aligned `scripts/check_alfred_hw.sh` with the active `/etc/sunray-core/config.json` runtime path.
  - Extended `config.example.json` with active Alfred runtime defaults that previously existed only in `Config.cpp`.
  - Added a config regression test that checks key runtime defaults in `config.example.json`.

### IB-010

- Title: Add hardware-capability and safety-assumption self-report at startup
- Category: diagnostics and observability, hardware abstraction quality, safety transparency
- Problem:
  - Current runtime knowledge still contains important `UNKNOWN`s around hard-stop behavior, dock-contact truth source, and deployed supervision.
- Evidence / source:
  - [HARDWARE_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/HARDWARE_MAP.md)
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Makes deployed hardware assumptions visible in logs and support workflows.
  - Helps distinguish unsupported configurations from software faults.
- Affected modules:
  - `hal/SerialRobotDriver/SerialRobotDriver.cpp`
  - `core/Robot.cpp`
  - startup logging and diagnostics docs
- Implementation risk:
  - Low
- Effort estimate:
  - Medium
- Priority suggestion:
  - P3
- Blockers / dependencies:
  - Some capabilities cannot be proven automatically with current hardware evidence.

### IB-011

- Title: Break out and document high-risk safety policy seams inside `Robot`
- Category: architecture clarity, testability
- Problem:
  - `Robot.cpp` remains the central place for battery policy, GPS resilience, watchdog logic, session handling, operator commands, and telemetry flow, which makes review of safety-critical changes harder.
- Evidence / source:
  - [PROJECT_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md) marks `core/Robot.cpp` as high risk.
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md) shows multiple critical guards concentrated in `Robot`.
  - [.memory/architecture-decisions.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/architecture-decisions.md) states Pi-side runtime owns high-level policy.
- Expected benefit:
  - Smaller review surfaces for later safety fixes.
  - Easier targeted testing of battery, GPS, watchdog, and stop policy.
- Affected modules:
  - `core/Robot.cpp`
  - `core/Robot.h`
  - related tests
- Implementation risk:
  - Medium to High
- Effort estimate:
  - High
- Priority suggestion:
  - P3
- Blockers / dependencies:
  - Should follow the P1 correctness fixes, not precede them.

### IB-012

- Title: Require RTK Fix before resuming dock-critical approach
- Status: Done on 2026-04-02
- Category: safety transparency, navigation and RTK resilience, reliability and recovery
- Problem:
  - Dock-critical resume currently accepts RTK Float and RTK Fix equally, even though final dock contact and approach precision are more sensitive to GPS quality than ordinary route continuation.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-004`
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Reduces avoidable charger misses after GPS degradation.
  - Makes dock recovery semantics explicit and testable.
- Affected modules:
  - `core/op/GpsWaitFixOp.cpp`
  - `core/op/DockOp.cpp`
  - `core/Robot.cpp`
  - dock/GPS integration tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Should preserve non-dock recovery behavior that can safely continue on RTK Float.
- Execution note:
  - Implemented by making `GpsWaitFixOp` require RTK Fix only when its resume target chain leads back to `Dock`, while keeping Float-based recovery for non-dock mission flow. Added a regression test that proves Float-only dock recovery remains blocked until Fix returns.

### IB-013

- Title: Detect non-progressive dock retries and escalate earlier
- Status: Done on 2026-04-02
- Category: reliability and recovery, safety transparency, diagnostics and observability
- Problem:
  - Dock retries currently replan repeatedly without checking whether the robot actually made progress toward the charger between attempts, so the same blocked approach can repeat until the retry budget is exhausted.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-005`
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [PROJECT_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)
- Expected benefit:
  - Reduces repeated futile dock attempts in blocked-contact scenarios.
  - Improves fault transparency by distinguishing true retry progress from a stalled approach loop.
- Affected modules:
  - `core/op/DockOp.cpp`
  - `core/Map.cpp`
  - dock retry integration tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Benefits from clear evidence or telemetry for dock-attempt progress and retry state.
- Execution note:
  - Implemented as a local `DockOp` retry-progress guard that snapshots the retry-start pose and escalates after two consecutive retry completions with less than meaningful motion, while preserving the existing retry path for attempts that did make progress.

### IB-014

- Title: Surface failed WebSocket command sends to the operator UI
- Status: Done on 2026-04-02
- Category: diagnostics and observability, communication resilience, safety transparency
- Problem:
  - Frontend command actions currently call `sendCmd()` without checking whether the command was actually sent, so disconnected WebSocket control attempts fail silently.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-006`
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Prevents false operator confidence for start/stop/dock actions.
  - Makes command-delivery failure visible at the moment of control interaction.
- Affected modules:
  - `webui-svelte/src/lib/api/websocket.ts`
  - `webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte`
  - `webui-svelte/src/lib/pages/Mission.svelte`
  - frontend command feedback tests
- Implementation risk:
  - Low
- Effort estimate:
  - Low to Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Needs a lightweight frontend error-notice pattern if no shared transient notice helper exists.
- Execution note:
  - Implemented by letting `sendCmd()` push a transient error into a shared command-feedback store when the WebSocket is not open or the send fails, and rendering that notice from `BottomPanel` so the warning is visible on the main operator pages without duplicating per-button error UI.

### IB-015

- Title: Keep the active mission card visible through transient recovery ops
- Status: Done on 2026-04-02
- Category: safety transparency, diagnostics and observability
- Problem:
  - The active mission widget hides itself when the runtime enters normal mid-mission recovery states like `GpsWait`, `EscapeReverse`, `EscapeForward`, or `WaitRain`, which removes status and abort controls exactly when the operator may need them.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-007`
  - [SAFETY_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/SAFETY_MAP.md)
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
- Expected benefit:
  - Preserves operator awareness during autonomous recovery.
  - Keeps mission abort/control affordances visible during degraded but still active mission flow.
- Affected modules:
  - `webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte`
  - telemetry-derived mission state helpers
  - frontend mission widget tests
- Implementation risk:
  - Low
- Effort estimate:
  - Low
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Best implemented against telemetry fields that reliably distinguish active mission context from idle recovery states.
- Execution note:
  - Implemented in `MissionWidget` by extending the active-mission op set to include normal recovery states like `GpsWait`, `EscapeReverse`, `EscapeForward`, and `WaitRain` while a `mission_id` is still present in telemetry.

### IB-016

- Title: Tune GPS-loss thresholds for safer EKF coast through short shadow zones
- Category: reliability and recovery, navigation and RTK resilience
- Problem:
  - Current GPS-loss thresholds are short enough that routine tree-cover and building-edge shadowing can force unnecessary mission pauses before the estimator's degraded fusion modes are exhausted.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-008`
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md) field-observed GPS coast window
  - [core/Config.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp)
- Expected benefit:
  - Reduces false `GpsWait` transitions in oak-canopy and wall-shadow routes without immediate code-path risk.
  - Creates a safer operating baseline before deeper op-layer GPS coast logic is changed.
- Affected modules:
  - `core/Config.cpp`
  - `config.example.json`
  - GPS resilience tests and notes
- Implementation risk:
  - Low
- Effort estimate:
  - Low
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Should stay bounded to conservative values because true antenna-fault behavior must still degrade safely.
- Execution note:
  - Implemented by raising the configured Alfred runtime baseline to `gps_no_signal_ms=15000`, `gps_recover_hysteresis_ms=3000`, and `ekf_gps_failover_ms=20000` in `Config.cpp` and `config.example.json`, with regression guards in `test_config.cpp`.

### IB-017

- Title: Add bounded mowing GPS-coast window on degraded EKF fusion
- Status: Done on 2026-04-02
- Category: reliability and recovery, navigation and RTK resilience, safety transparency
- Problem:
  - The mowing op currently ignores the estimator's degraded but still operational fusion modes and stops too early for short outages.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-008`
  - [.memory/runtime-knowledge.md](/mnt/LappiDaten/Projekte/sunray-core/.memory/runtime-knowledge.md)
  - [core/op/MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp)
  - [core/navigation/StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp)
- Expected benefit:
  - Lets mowing bridge short GPS outages while estimator fusion remains operational.
  - Reduces stop/resume fragmentation after the config-only tuning ceiling is reached.
- Affected modules:
  - `core/op/MowOp.cpp`
  - `core/Robot.cpp`
  - GPS/mowing integration tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Best executed after conservative config tuning is in place and validated.
- Execution note:
  - Implemented by letting `Robot::monitorGpsResilience()` keep `Mow` active for a bounded `mow_gps_coast_ms` window when degraded fusion (`EKF+IMU` or `Odo`) remains operational after prior confirmed GPS signal. Startup without prior GPS still falls through to `GpsWait`.

### IB-018

- Title: Add encoder-based stuck detection and recovery dispatch
- Status: Done on 2026-04-02
- Category: reliability and recovery, safety transparency
- Problem:
  - The runtime already knows commanded drive speed, encoder ticks, and fused ground speed, but it does not turn persistent commanded-motion stalls into an explicit recovery event.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-HIGH-009`
  - [core/control/DifferentialDriveController.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/control/DifferentialDriveController.cpp)
  - [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
  - [core/op/Op.h](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h)
- Expected benefit:
  - Detects wheel-spin/high-center stall conditions that are invisible to bumper-only recovery.
  - Reuses existing escape/error op paths instead of inventing a second recovery architecture.
- Affected modules:
  - `core/Robot.cpp`
  - `core/Robot.h`
  - `core/control/DifferentialDriveController.*`
  - `core/op/*`
  - robot/op-machine tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Low to Medium
- Priority suggestion:
  - P1
- Blockers / dependencies:
  - Should reuse existing `stuck_detect_*` config keys and avoid triggering on pure in-place turns or startup transitions.
- Execution note:
  - Implemented by adding a `Robot` stuck guard keyed off commanded linear speed versus fused ground speed, reusing the existing `stuck_detect_*` config seam, and dispatching a new `onStuck()` event to active ops. `Mow` and `Dock` now recover through `EscapeReverse`; `Undock` escalates to `Error`.
  - Extended on 2026-04-02: repeated unresolved stuck events now honor `stuck_recovery_max_attempts` and escalate deterministically to `Error` with `ERR_STUCK` / `stuck_recovery_exhausted` instead of looping recovery indefinitely.

### IB-020

- Title: Persist field-usable incident counters in the history summary
- Status: Done on 2026-04-02
- Category: diagnostics and observability, safety transparency
- Problem:
  - Runtime events were already persisted individually, but operator analytics and field optimization still lacked a compact counter surface for repeated incident types such as lift, battery undervoltage, GPS loss, or stuck recovery exhaustion.
- Evidence / source:
  - [core/storage/HistoryDatabase.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/storage/HistoryDatabase.cpp)
  - [core/storage/EventRecord.h](/mnt/LappiDaten/Projekte/sunray-core/core/storage/EventRecord.h)
  - [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
  - user requirement on 2026-04-02 to retain field-optimization statistics for safety/runtime incidents
- Expected benefit:
  - Gives UI, diagnostics, and future analytics a stable counter surface without re-scanning the full event list.
  - Makes patterns like repeated lift events, battery-critical shutdowns, MCU comm loss, or GPS faults visible for optimization work.
- Affected modules:
  - `core/storage/HistoryDatabase.cpp`
  - `core/storage/HistoryDatabase.h`
  - history regression tests
- Implementation risk:
  - Low
- Effort estimate:
  - Low
- Priority suggestion:
  - P2
- Blockers / dependencies:
  - Benefits grow with consistent event naming across runtime paths.
- Execution note:
  - Implemented by extending `HistoryDatabase::buildSummary()` with grouped `event_reason_counts`, `event_type_counts`, and `event_level_counts`, backed by regression coverage in `tests/test_history_database.cpp`.
  - Extended on 2026-04-02: the WebUI now exposes a dedicated `Verlauf` page that consumes `/api/history/events`, `/api/history/sessions`, `/api/statistics/summary`, and the export endpoint so field statistics can be inspected without direct DB access.

### IB-019

- Title: Add lateral-offset dock retry geometry for repeated contact misses
- Status: Done on 2026-04-02
- Category: reliability and recovery, navigation and RTK resilience
- Problem:
  - Dock retries can still repeat the same approach geometry even when the failure mode looks like a small lateral alignment miss rather than a routing failure.
- Evidence / source:
  - [BUG_REPORT.md](/mnt/LappiDaten/Projekte/sunray-core/BUG_REPORT.md) `BUG-MED-006`
  - [core/op/DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp)
  - [core/navigation/Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- Expected benefit:
  - Improves dock-contact recovery without new sensors.
  - Makes later retries materially different instead of just repeating the same miss geometry.
- Affected modules:
  - `core/op/DockOp.cpp`
  - `core/navigation/Map.*`
  - docking regression tests
- Implementation risk:
  - Medium
- Effort estimate:
  - Medium
- Priority suggestion:
  - P2
- Blockers / dependencies:
  - Must preserve current docking fallback behavior when offset routing is impossible.
- Execution note:
  - Implemented by adding `dock_retry_lateral_offset_m` and letting `DockOp` use a positive offset on retry 3 and a negative offset on retry 4 through `Map::retryDocking(..., lateralOffsetM)`, with fallback to the original retry path if the offset route cannot be planned.

## Recommended Execution Order

1. `IB-001` Serialize external commands onto the control-loop thread
2. `IB-002` Restore stop-button authority during active diagnostics
3. `IB-003` Add an explicit Pi-side communication-loss safety transition
4. `IB-004` Harden MQTT reconnect and command-subscription recovery
5. `IB-005` Add charger-contact debounce and dock-state observability
6. `IB-006` Add fault-injection and soak coverage for UART, MQTT, and dock-contact edge cases
7. `IB-008` Make deployment and rollback procedure explicit and verifiable
8. `IB-009` Clarify and stabilize configuration ownership for Alfred runtime defaults
9. `IB-007` Centralize runtime health telemetry for watchdog, comms, and recovery state
10. `IB-010` Add hardware-capability and safety-assumption self-report at startup
11. `IB-011` Break out and document high-risk safety policy seams inside `Robot`
12. `IB-012` Require RTK Fix before resuming dock-critical approach
13. `IB-013` Detect non-progressive dock retries and escalate earlier
14. `IB-014` Surface failed WebSocket command sends to the operator UI
15. `IB-015` Keep the active mission card visible through transient recovery ops
16. `IB-016` Tune GPS-loss thresholds for safer EKF coast through short shadow zones
17. `IB-017` Add bounded mowing GPS-coast window on degraded EKF fusion
18. `IB-018` Add encoder-based stuck detection and recovery dispatch
19. `IB-020` Persist field-usable incident counters in the history summary
19. `IB-019` Add lateral-offset dock retry geometry for repeated contact misses

## Summary

- P1 items are driven mostly by confirmed safety and concurrency findings.
- P2 items focus on recovery, deployment confidence, and configuration drift reduction.
- P3 items improve maintainability and operator clarity after correctness risks are reduced.
