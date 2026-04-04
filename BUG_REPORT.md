# Bug Report

## Status

This report contains the current evidence-based static bug sweep for `sunray-core`.
The findings below were derived from repository code inspection only.
No production code was changed during this sweep.
Regression coverage was expanded on 2026-04-02 for repeated dock-contact flaps, ChargeOp grace-expiry behavior, and degraded MQTT enabled lifecycle behavior; true injected UART transport-fault coverage still lacks a dedicated harness.
Runtime observability was also expanded on 2026-04-02 with a compact health telemetry surface for MCU comms, GPS degradation, battery guards, recovery state, and watchdog notices.

## Critical

### BUG-CRIT-001

- Title: Cross-thread control access to non-thread-safe `Robot` methods
- Status: Mitigated on 2026-04-02
- Evidence:
  - [Robot.h](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h#L21) states that only `run()` and `stop()` are thread-safe and that all other methods are not thread-safe.
  - WebSocket command callbacks call `robot.startMowing()`, `robot.startDocking()`, and `robot.emergencyStop()` directly from callback context in [main.cpp](/mnt/LappiDaten/Projekte/sunray-core/main.cpp#L204).
  - MQTT command callbacks do the same in [main.cpp](/mnt/LappiDaten/Projekte/sunray-core/main.cpp#L333).
  - Those methods mutate op state, map state, mission state, UI state, and event history without synchronization in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L652), [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L709), and [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L723).
- Reproduction:
  1. Run the control loop.
  2. Trigger WebSocket or MQTT commands while `Robot::run()` is active.
  3. Observe that callbacks can mutate shared state concurrently with the control loop.
- Risk:
  - Safety-critical state corruption
  - Invalid op transitions
  - Races between stop/start/dock commands and motion logic
- Minimal Fix:
  - Funnel non-thread-safe robot commands onto the control-loop thread via a synchronized command queue.
- Verification:
  - Add stress tests that issue commands concurrently with `run()`.
  - Run under TSAN if possible.
- Resolution note:
  - WebSocket and MQTT command entry points now enqueue operator actions into `Robot` and execute them on the control-loop thread during `run()`.

## High

### BUG-HIGH-001

- Title: MQTT callback violates `Robot` thread-safety contract
- Status: Mitigated on 2026-04-02
- Evidence:
  - `MqttClient` documents that command callbacks run on the mosquitto network thread and must be thread-safe in [MqttClient.h](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.h#L61).
  - The callback installed in [main.cpp](/mnt/LappiDaten/Projekte/sunray-core/main.cpp#L333) directly calls non-thread-safe `Robot` control methods.
  - MQTT reconnect and network handling run off-thread in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L88) and [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L286).
- Reproduction:
  1. Enable MQTT.
  2. Deliver commands while the control loop is active.
  3. The callback path reaches unsynchronized `Robot` state mutation from the MQTT thread.
- Risk:
  - Command races
  - Unstable behavior during reconnect or broker flaps
  - Likely contributor to intermittent MQTT control issues
- Minimal Fix:
  - Marshal MQTT commands onto the control-loop thread before touching `Robot`.
- Verification:
  - Concurrent MQTT command stress tests
  - Broker disconnect/reconnect scenario tests
- Resolution note:
  - MQTT command callbacks now use the same external-command queue as WebSocket control paths before any `Robot` state mutation occurs.

### BUG-HIGH-002

- Title: Stop button path is bypassed while diagnostics are active
- Status: Mitigated on 2026-04-02
- Evidence:
  - `run()` executes `tickDiag()` before `tickButtonControl()` in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L114).
  - If a diagnostic is active, `run()` returns early in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L115).
  - During diagnostics, motors are actively driven in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L357).
  - The only hard fallback inside the diagnostic path is the 15 s safety timeout in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L384).
- Reproduction:
  1. Start a diagnostic motor action.
  2. Press the stop button while the diagnostic is running.
  3. The normal stop-button handler is skipped until the diagnostic path exits.
- Risk:
  - Delayed operator stop response
  - Unsafe continued motion during diagnostics
- Minimal Fix:
  - Handle stop-button and emergency-stop checks before or inside `tickDiag()`.
- Verification:
  - Test stop-button interruption during active diagnostics.
- Resolution note:
  - `tickDiag()` now aborts an active diagnostic immediately when the stop button is pressed.
  - `emergencyStop()` (triggered by stop button or remote `stop`/`reset` commands) now explicitly cancels `diagReq_` under lock, ensuring any diagnostic motion stops and the handler is notified.

### BUG-HIGH-003

- Title: MQTT subscription recovery is best-effort only after connect/reconnect
- Status: Mitigated on 2026-04-02
- Evidence:
  - The client subscribes once on connect in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L70).
  - If `mosquitto_subscribe()` fails, the code only logs a warning in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L72).
  - There is no explicit retry or degraded state tracking after that failure.
- Reproduction:
  1. Cause a transient subscribe failure during connect or reconnect.
  2. Observe that the client continues running without confirmed command subscription recovery.
- Risk:
  - Silent loss of remote control commands
  - Telemetry continues, giving a false impression of healthy MQTT command handling
- Minimal Fix:
  - Add subscribe retry or explicit disconnected/degraded command-channel state.
- Verification:
  - Broker test that injects subscription failures and verifies recovery.
- Resolution note:
  - `MqttClient` now tracks connection and command-subscription state separately, retries command-topic subscription after reconnect and after subscribe failures, and clears subscription health when publishes report `MOSQ_ERR_NO_CONN`.

### BUG-HIGH-004

- Title: RTK Float accepted as sufficient GPS quality to resume dock approach
- Status: Mitigated on 2026-04-02
- Evidence:
  - `GpsWaitFixOp::run()` resumes to the caller op when `ctx.gpsHasFloat || ctx.gpsHasFix` in [GpsWaitFixOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/GpsWaitFixOp.cpp#L19).
  - `Robot::monitorGpsResilience()` equally treats Float and Fix as signal recovery in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1347).
  - `StateEstimator::updateGps()` stores Float and Fix under separate flags but the ops do not distinguish them in [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp#L255).
  - `DockOp` makes no GPS-quality check before initiating or resuming a dock approach in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L25).
- Reproduction:
  1. Start a docking sequence in an area with poor RTK conditions (multipath, tree cover).
  2. GPS drops to no-signal mid-approach; robot enters GpsWait.
  3. GPS recovers to Float only (not Fixed).
  4. GpsWait immediately resumes DockOp.
  5. Dock approach executes with Float-level accuracy (±3–5 cm instead of ±1 cm).
  6. Charger contact is missed; DockOp retries up to the configured maximum.
- Risk:
  - Dock approach precision relies on RTK Fixed accuracy
  - Float-only resumption can cause systematic charger miss by several centimetres
  - Repeated misses exhaust retry budget and transition to Error
- Minimal Fix:
  - Let DockOp declare a minimum GPS quality requirement; have GpsWait wait for Fix before resuming dock-critical ops.
- Verification:
  - Simulate Float-only recovery during dock approach and observe charger contact rate.
- Resolution note:
  - `GpsWaitFixOp` now keeps dock-critical resume paths in `GpsWait` while GPS has only RTK Float and only continues dock recovery once RTK Fix is present again; non-dock recovery paths still resume on Float. Regression coverage now includes Float-only dock recovery staying blocked until Fix.

### BUG-HIGH-005

- Title: Dock retry replans route without validating position progress between attempts
- Status: Mitigated on 2026-04-02
- Evidence:
  - `DockOp::onNoFurtherWaypoints()` increments `mapRoutingFailedCounter` and calls `ctx.map->retryDocking()` without checking whether the robot moved closer to the charger since the previous attempt in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L55).
  - No position snapshot is taken at the start of each retry cycle; distance-to-dock is not compared across retries.
  - After `dock_retry_max_attempts` (default 5) identical non-progressive attempts the op transitions to Error in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L61).
- Reproduction:
  1. Block the charger approach physically (e.g., debris in front of the dock connector).
  2. DockOp drives the planned route, fails to touch charger, exhausts waypoints.
  3. `onNoFurtherWaypoints()` re-plans the identical route from the same position.
  4. The same blocked approach repeats up to 5 times before escalating to Error.
- Risk:
  - Robot executes up to 5 identical collision-prone approach cycles without any feedback
  - No distinction between "still far from dock" and "right at dock but not making contact"
  - Transition to Error is unavoidable even for easily resolvable blockage scenarios
- Minimal Fix:
  - Record position at retry start; if robot has not advanced toward the charger between retries, escalate after 2 non-progressive attempts.
- Verification:
  - Integration test with a blocked charger approach; verify early escalation on repeated same-position retries.
- Resolution note:
  - `DockOp` now snapshots the retry-start pose, measures movement between retry cycles, and escalates to `Error` after two consecutive non-progressive retry endings instead of consuming the full retry budget on the same stalled approach. Regression coverage now includes an explicit non-progressive dock retry scenario.

### BUG-HIGH-006

- Title: `sendCmd` silently drops commands when WebSocket is not open; callers receive no feedback
- Status: Mitigated on 2026-04-02
- Evidence:
  - `sendCmd()` returns `false` without any notification when the socket is not in `OPEN` state in [websocket.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/api/websocket.ts#L82).
  - Every call site — `startSelectedMission()`, `sendCmd("stop")`, `sendCmd("dock")` — discards the return value; no error path is exercised in [MissionWidget.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte#L92) and [Mission.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Mission.svelte#L241).
  - The stop button in the active mission card also discards the return value in [MissionWidget.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte#L163).
- Reproduction:
  1. Let the WebSocket connection drop (e.g., Wi-Fi interruption, robot reboot).
  2. Press "Mission starten", "Abbrechen", or any command button while the connection indicator still shows a stale state.
  3. `sendCmd` returns `false`; no toast, no error, no visual change.
  4. User assumes the command was received; robot has received nothing.
- Risk:
  - Operator believes the robot was commanded to stop or start; robot behavior is unaffected
  - No indication distinguishes "command sent" from "command silently dropped"
  - Highest impact for the stop/abort path where the operator acts in response to unexpected robot motion
- Minimal Fix:
  - Check the return value of `sendCmd` at every call site; show a transient error notice when `false` is returned.
- Verification:
  - Simulate WS disconnect while sending commands; verify a visible error state is shown and no false success feedback appears.
- Resolution note:
  - `sendCmd()` now surfaces disconnected or failed command sends through a shared frontend feedback store, and `BottomPanel` renders that notice globally across the main UI pages so dropped start/stop/dock commands are no longer silent.

### BUG-HIGH-007

- Title: Active mission widget disappears when robot enters `GpsWait`, `EscapeReverse`, or `WaitRain` mid-mission
- Status: Mitigated on 2026-04-02
- Evidence:
  - `missionIsRunning` gates the active mission card on `op` being one of `"Undock"`, `"NavToStart"`, or `"Mow"` in [MissionWidget.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte#L139).
  - `GpsWait`, `EscapeReverse`, and `WaitRain` are all normal transient states within an active mowing mission (see `MowOp` event handlers).
  - When `missionIsRunning` becomes `false`, the component renders the "Nächste Mission" or empty card instead.
- Reproduction:
  1. Start a mowing mission.
  2. Let the robot lose GPS signal briefly; op transitions to `GpsWait`.
  3. Mission widget switches from "Mission läuft" to "Nächste Mission" or the empty state.
  4. User sees no running mission despite the robot being mid-mission.
- Risk:
  - Operator loses visibility of the active mission during the states most likely to require attention
  - The "Abbrechen" button becomes inaccessible precisely when the operator may want to use it
  - Confusion about whether a mission is active or the robot is in an autonomous recovery state
- Minimal Fix:
  - Extend `missionIsRunning` to include `"GpsWait"`, `"EscapeReverse"`, `"EscapeForward"`, and `"WaitRain"` when a `mission_id` is present in telemetry.
- Verification:
  - Simulate GPS loss during mowing in the simulator; verify the active mission card remains visible throughout `GpsWait` and resumes.
- Resolution note:
  - `MissionWidget` now treats `GpsWait`, `EscapeReverse`, `EscapeForward`, and `WaitRain` as active mission states when a mission is still associated via telemetry, so the running-mission card and abort affordance remain visible through normal recovery phases.

## Medium

### BUG-MED-001

- Title: UART gap-based frame handling uses a stale timestamp for the whole read burst
- Status: Confirmed
- Evidence:
  - `pollRx()` captures `now` once at function start in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L353).
  - `rxLastByteMs_` is updated with that same constant inside the loop in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L380).
  - Gap-dispatch logic later relies on `rxLastByteMs_` in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L403).
- Reproduction:
  1. Receive fragmented or long UART bursts.
  2. Observe that the recorded “last byte” time may no longer represent the true last byte arrival time.
- Risk:
  - Increased framing ambiguity
  - Hard-to-reproduce UART desync on noisy or fragmented input
- Minimal Fix:
  - Update `rxLastByteMs_` with a fresh timestamp for each successful read burst.
- Verification:
  - Serial fuzz tests with fragmented and back-to-back frames.

### BUG-MED-002

- Title: Pi-side runtime does not enforce a direct safe-state on MCU communication loss
- Status: Mitigated on 2026-04-02
- Evidence:
  - MCU comms loss sets `mcuConnected_ = false` in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L189).
  - `readOdometry()` then returns zero ticks and marks odometry invalid in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L247).
  - `StateEstimator` simply resets update state on missing MCU connection in [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp#L204).
  - No direct `Robot`-level safety transition on `mcuConnected == false` was found in the sweep.
- Reproduction:
  1. Lose UART traffic to the STM32.
  2. Observe that Linux-side logic keeps running without a dedicated fault transition tied to comms loss.
- Risk:
  - Safety behavior depends on external MCU watchdog assumptions
  - Weak local containment if MCU-side timeout regresses
- Minimal Fix:
  - Add an explicit robot-level comms-loss fault or controlled stop path.
- Verification:
  - Simulate UART loss and verify robot-side safety transition.
- Resolution note:
  - `Robot` now latches a post-startup MCU communication loss, zeros motor PWM locally, records a safety event, surfaces `ERR_MCU_COMMS`, and requests the existing `Error` op on the Pi side.

### BUG-MED-003

- Title: Dock detect is inferred purely from charge-voltage threshold
- Status: Partially mitigated on 2026-04-02
- Evidence:
  - Dock contact is derived from `chargeVoltage > threshold` in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L24).
  - The same threshold logic is used in motor-frame and summary-frame parsing in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L523) and [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L567).
  - Dock and charge ops rely on `chargerConnected` for their transitions in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L27) and [ChargeOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/ChargeOp.cpp#L53).
- Reproduction:
  1. Present unstable or noisy charge voltage around the threshold.
  2. Observe possible flapping between docked and undocked interpretations.
- Risk:
  - Docking mismatch
  - Spurious dock retries, undock failures, or unexpected charge exits
- Minimal Fix:
  - Add debounce/hysteresis or a dedicated dock-contact signal if hardware supports it.
- Verification:
  - Simulate noisy charger voltage around the threshold and verify stable state behavior.
- Resolution note:
  - `SerialRobotDriver` now debounces charger-contact state across consecutive samples before updating `battery_.chargerConnected`, and telemetry now exposes the debounced `charger_connected` state explicitly.

### BUG-MED-004

- Title: Critical battery path can escalate directly to shutdown on unstable dock signal
- Status: Partially mitigated on 2026-04-02
- Evidence:
  - Critical battery handling immediately stops motors, enables buzzer, requests `onBatteryUndervoltage()`, and triggers power-down in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1112).
  - Battery guards are bypassed only when `battery_.chargerConnected` is true in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1106).
  - `chargerConnected` is itself threshold-derived rather than hard-contact-based in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L24).
- Reproduction:
  1. Be physically docked.
  2. Let `chargeVoltage` temporarily drop below threshold while battery is low or critical.
  3. The robot can enter the non-docked critical-battery path instead of a contact-recovery path.
- Risk:
  - Unnecessary shutdown while physically on dock
  - Recovery behavior depends on noisy analog threshold interpretation
- Minimal Fix:
  - Add dock-contact debounce and a safer critical-battery exception path when recent dock contact is known.
- Verification:
  - Test charger voltage dips during docked low-battery conditions.
- Resolution note:
  - The immediate risk from single-sample dock-contact flaps is reduced by charger-contact debounce in `SerialRobotDriver`, but there is still no independent hardware dock-contact source in current repo evidence.

### BUG-HIGH-008

- Title: Mowing transitions into `GpsWait` too early despite valid EKF+IMU coast capability
- Status: Confirmed
- Evidence:
  - `monitorGpsResilience()` fires `onGpsNoSignal()` after `gps_no_signal_ms` of signal absence, defaulting to 3000 ms in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1341).
  - `onGpsNoSignal()` in `MowOp` immediately transitions to `GpsWaitFixOp` in [MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp).
  - `StateEstimator::update()` continues prediction from odometry while odometry is valid in [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp#L188).
  - `updateImu()` keeps IMU-assisted heading integration active in [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp#L275).
  - `StateEstimator::fusionMode()` already tracks degraded EKF modes (`"EKF+IMU"`, `"Odo"`) in [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp#L294), but the op layer does not use them to gate the mowing transition.
  - Field observation reported acceptable short GPS bridging under oak-tree and wall-shadow conditions while EKF prediction remained stable.
- Reproduction:
  1. Mow under a tree line or near a building with intermittent GPS multipath.
  2. GPS signal drops for 3+ seconds (shadow, branch obstruction).
  3. Mission pauses immediately; robot stands still in GpsWait.
  4. GPS recovers; mission resumes — but every similar shadow causes another pause.
- Risk:
  - Unnecessary mowing interruption in tree cover and near buildings
  - Op layer does not exploit already-available EKF+IMU coast capability
  - User-visible: mission appears unreliable in partial-shadow gardens even when estimator quality remains operational
- Minimal Fix:
  - Introduce `mow_gps_coast_ms` and keep mowing active while fusion remains `EKF+IMU` or `Odo` inside a bounded coast window before transitioning to `GpsWait`.
- Verification:
  - Simulate repeated 3–10 s GPS outages and compare mission continuity versus current behavior.
  - Manual validation under oak-tree shadow and wall-multipath routes.
- Resolution note:
  - The immediate configuration baseline was improved: Alfred runtime defaults now use `gps_no_signal_ms=15000`, `gps_recover_hysteresis_ms=3000`, and `ekf_gps_failover_ms=20000` in [Config.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp) and [config.example.json](/mnt/LappiDaten/Projekte/sunray-core/config.example.json). The remaining gap is still op-layer behavior in `MowOp`, which continues to transition into `GpsWait` instead of exploiting bounded degraded-fusion coast.
  - Completed on 2026-04-02: `Robot::monitorGpsResilience()` now allows a bounded `mow_gps_coast_ms` window during `Mow` when degraded fusion remains operational (`EKF+IMU` or `Odo`) and there was prior confirmed GPS signal. Startup without prior signal still degrades to `GpsWait` as before.

### BUG-HIGH-009

- Title: Commanded motion can stall for multiple seconds without any encoder-based stuck safety transition
- Status: Confirmed
- Evidence:
  - `DifferentialDriveController::compute()` already calculates desired wheel speeds versus measured wheel speeds from encoder ticks in [DifferentialDriveController.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/control/DifferentialDriveController.cpp#L35).
  - `Robot::tickSafetyGuards()` currently checks battery, MCU comms, GPS, perimeter, map change, and op watchdogs, but no encoder-based stuck guard in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L255).
  - `StateEstimator::groundSpeed()` is already available through `OpContext::stateEst` in [Op.h](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h#L74).
  - Alfred config already carries unused stuck-detection keys in [Config.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp#L182), which indicates an intended but currently unimplemented seam.
- Reproduction:
  1. Let the robot command forward motion in `Mow` or `Dock`.
  2. Physically high-center the chassis or let both drive wheels spin without meaningful movement.
  3. Encoder ticks and/or fused ground speed remain near zero while the op keeps commanding motion.
  4. No dedicated stuck event fires; recovery depends on unrelated bumper, timeout, or operator intervention.
- Risk:
  - Repeated wheel spin or stall under commanded motion without a dedicated recovery path
  - Unnecessary battery drain, turf damage, or delayed recovery when the robot is perched on an obstacle
  - Safety architecture already has obstacle escape logic, but it is not invoked for encoder-visible stall conditions
- Minimal Fix:
  - Add a `Robot` safety guard that observes commanded linear speed versus estimated ground speed over a bounded window and calls a new `onStuck()` op event.
- Verification:
  - Simulate `Mow` or `Dock` with commanded forward motion, `mcuConnected=true`, and near-zero odometry/ground speed; verify `EscapeReverse` or `Error` depending on the active op.
- Resolution note:
  - `Robot` now monitors persistent commanded-motion stalls via existing `stuck_detect_timeout_ms` / `stuck_detect_min_speed_ms`, `DifferentialDriveController` exposes the last commanded linear speed, and active ops receive a new `onStuck()` event. `Mow` and `Dock` now route this to `EscapeReverse`; `Undock` escalates to `Error`.

### BUG-MED-014

- Title: Repeated stuck-recovery loops did not escalate to a terminal fault after recovery exhaustion
- Status: Confirmed
- Evidence:
  - The initial stuck handling added `onStuck()` dispatch into recovery ops, but no separate counter bounded how many failed stuck recoveries could repeat before another normal op cycle resumed.
  - `Robot` already carried a runtime config seam for `stuck_recovery_max_attempts` in [Config.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Config.cpp), but repeated stuck recovery was not yet latched into a dedicated terminal error.
  - Telemetry and persisted history already supported error/event reporting, but there was no dedicated `ERR_STUCK` / `stuck_recovery_exhausted` path surfacing repeated failed recovery.
- Reproduction:
  1. Start `Mow` with commanded forward motion and valid MCU comms.
  2. Force a stall until the runtime triggers `EscapeReverse`.
  3. Let the recovery op finish without resolving the physical blockage.
  4. Trigger the same stall again.
  5. Without an exhaustion latch, the robot can re-enter another recovery cycle instead of escalating deterministically.
- Risk:
  - Recovery behavior can loop without a terminal fault when the underlying blockage persists.
  - Field diagnosis is harder because repeated stalls look like isolated recovery events instead of a bounded failure pattern.
  - Operators do not get a dedicated stuck-exhaustion reason in telemetry/history.
- Minimal Fix:
  - Count repeated stuck-recovery events and escalate to `Error` with a dedicated `ERR_STUCK` / `stuck_recovery_exhausted` reason when `stuck_recovery_max_attempts` is exhausted.
- Verification:
  - Reproduce two consecutive unresolved stuck events with `stuck_recovery_max_attempts=2`; verify `Error`, `ERR_STUCK`, and persisted safety-event history.
- Resolution note:
  - Completed on 2026-04-02: `Robot` now latches repeated unresolved stuck events, transitions to `Error` with `ERR_STUCK`, exposes `stuck_recovery_exhausted` in telemetry/history, and resets the recovery counter once real motion resumes or the runtime returns to a non-recovery-safe steady state.

### BUG-MED-006

- Title: Dock retries still repeat the same corridor approach geometry even when contact failures suggest a lateral alignment miss
- Status: Confirmed
- Evidence:
  - `Map::retryDocking()` is still only `startDocking(robotX, robotY)` in [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp#L540).
  - `DockOp::onNoFurtherWaypoints()` retries routing after missed contact, but every retry uses the same approach geometry in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L58).
  - The map already exposes `dockApproachTarget()` and free-path replanning primitives in [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp#L505) and [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp#L916).
- Reproduction:
  1. Place the robot slightly laterally offset from the ideal dock corridor.
  2. Let docking miss charger contact while the route itself remains otherwise valid.
  3. Subsequent retries rebuild essentially the same approach and can miss in the same way again.
- Risk:
  - Dock retries may burn budget on near-identical geometric misses
  - Contact-capable docks with a small funnel/trichter can remain unreached despite a trivially solvable lateral offset
- Minimal Fix:
  - Let later dock retries vary the approach target laterally before falling back to the unchanged corridor path.
- Verification:
  - Simulate repeated no-contact dock finishes with a small lateral miss and verify later retries choose distinct approach geometry.
- Resolution note:
  - Completed on 2026-04-02: later `DockOp` retries now apply a configured lateral offset (`dock_retry_lateral_offset_m`) on retry 3 and 4 before falling back to the original corridor path when offset routing is not possible.

### BUG-MED-006

- Title: `UndockOp` escalates any bumper contact directly to Error without escape attempt
- Status: Confirmed
- Evidence:
  - `UndockOp::onObstacle()` calls `changeOp(ctx, ctx.opMgr.error())` unconditionally in [UndockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/UndockOp.cpp#L57).
  - `DockOp::onObstacle()` and `MowOp::onObstacle()` both attempt `EscapeReverse` before any escalation.
  - The charger connector housing or a loose cable is a plausible physical obstacle during backward undock motion.
- Reproduction:
  1. Initiate undock from a charger station with a protruding connector or cable.
  2. Robot backs into the obstacle during the undock manoeuvre.
  3. Bumper fires; robot enters Error state immediately.
  4. Operator must manually clear Error before any recovery attempt is possible.
- Risk:
  - Asymmetric escalation compared to all other moving states
  - Transient or trivial physical contact during undocking requires operator intervention every time
  - Reduces operational autonomy in installations where undock geometry is constrained
- Minimal Fix:
  - Route `UndockOp::onObstacle()` through `EscapeReverse` (return target: Undock or Idle) the same way DockOp handles obstacles; escalate to Error only on repeated contact.
- Verification:
  - Inject a bumper event during undock in the simulator and verify recovery path.

### BUG-MED-007

- Title: Rain sensor detection has no debounce; a single sensor frame triggers a mission pause
- Status: Confirmed
- Evidence:
  - `tickSafetyStop()` fires `onRainTriggered()` on the first rising edge of `sensors_.rain` in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L538).
  - No persistence requirement: one control cycle (≈20 ms) of rain signal is sufficient to pause the mission.
  - `MowOp::onRainTriggered()` transitions immediately to `WaitRainOp` in [MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp).
- Reproduction:
  1. Mow in conditions with occasional EMI or sensor noise on the rain input line.
  2. A single spurious high pulse on the rain sensor fires `onRainTriggered()`.
  3. Mission pauses; robot waits for rain to clear (which may take additional minutes).
- Risk:
  - Electrical noise or a brief dew detection causes an unnecessary mission abort
  - `WaitRainOp` may autonomously dock if rain does not clear within its window, extending the interruption
- Minimal Fix:
  - Require rain to be detected for a minimum consecutive duration (e.g., 500 ms) before firing `onRainTriggered()`.
- Verification:
  - Inject a single-frame rain pulse in simulation and verify the mission is not paused.

### BUG-MED-008

- Title: Perimeter safety check defaults to "inside" when the map is not loaded
- Status: Confirmed
- Evidence:
  - `assembleOpContext()` sets `ctx.insidePerimeter = true` when `!map_.isLoaded()` in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L237).
  - `monitorPerimeterSafety()` reads `ctx.insidePerimeter` and fires only on a false→true→false edge in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1268).
  - If the map fails to load (file missing, parse error, storage fault), the robot operates with the perimeter guard permanently suppressed.
- Reproduction:
  1. Delete or corrupt the map file before starting the robot.
  2. Robot initialises, map load fails silently, `map_.isLoaded()` returns false.
  3. Robot starts mowing or is driven manually; perimeter is never checked.
  4. No `onPerimeterViolated()` event is ever fired regardless of robot position.
- Risk:
  - Complete loss of perimeter safety when storage or file-system issues prevent map loading
  - Silent: no fault or warning is surfaced to the operator when the fallback is active
  - High-risk for installations where the mow area is adjacent to roads, slopes, or pools
- Minimal Fix:
  - When `map_.isLoaded()` is false, surface an explicit UI warning that perimeter protection is inactive; optionally prevent autonomous operation until a valid map is confirmed.
- Verification:
  - Start the robot with a missing map file and verify that a perimeter-inactive warning is displayed and autonomous start is blocked.

### BUG-MED-009

- Title: `moveZonePoint` skips self-intersection validation that `addPoint` enforces
- Status: Confirmed
- Evidence:
  - `addPoint` for zone tools calls `wouldCreateSelfIntersection()` before committing a new vertex in [map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts#L361).
  - `moveZonePoint` moves a vertex directly without any intersection check in [map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts#L401).
  - `moveExclusionPoint` has the same omission in [map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts#L410).
- Reproduction:
  1. Draw a valid convex zone polygon.
  2. Drag one vertex across the polygon boundary via the map editor.
  3. The polygon becomes self-intersecting; the store marks it dirty and allows saving.
  4. Backend receives a self-intersecting zone polygon; mow route planning produces undefined results.
- Risk:
  - Silently corrupts zone geometry in the stored map
  - Mow route generation for a self-intersecting zone is undefined and may produce erratic paths
  - No on-save rejection: the corrupted polygon passes through to the backend
- Minimal Fix:
  - Run `wouldCreateSelfIntersection` in `moveZonePoint` and `moveExclusionPoint`; reject the move if the result would self-intersect.
- Verification:
  - Drag a zone vertex across an edge in the map editor and verify the move is rejected with a visible hint.

### BUG-MED-010

- Title: Mission page has no unsaved-changes indicator; edits can be silently lost on navigation
- Status: Confirmed
- Evidence:
  - The map editor tracks unsaved state via `$mapStore.dirty` and shows a `*` suffix on the save button in [Map.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Map.svelte#L785).
  - The mission editor has no equivalent dirty flag; zone assignments, schedule changes, and overrides live only in reactive local state until `saveCurrentMission()` is explicitly called in [Mission.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Mission.svelte#L118).
  - Navigating away from the mission page silently discards all unsaved changes without warning.
- Reproduction:
  1. Open a mission in the mission editor.
  2. Change the schedule (e.g., add a weekday or shift the start time).
  3. Navigate to the Dashboard without pressing "Speichern".
  4. Return to the mission page: changes are gone, no warning was shown.
- Risk:
  - Operator's schedule and zone configuration changes are silently lost
  - No feedback whether the currently displayed state matches the backend state
- Minimal Fix:
  - Track a `missionDirty` boolean in Mission.svelte; show a `*` on the save button when true and add a `beforeunload`/navigation guard.
- Verification:
  - Edit a mission schedule, navigate away without saving, return and verify the change is gone — then add the guard and verify a warning is shown.

### BUG-MED-011

- Title: Map draft auto-save runs `structuredClone` + `localStorage.setItem` on every store change without debounce
- Status: Confirmed
- Evidence:
  - `saveDraft()` is triggered by the reactive statement `$: if (hydrationDone) { saveDraft(); }` in [Map.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Map.svelte#L733).
  - `saveDraft()` performs `structuredClone($mapStore.map)` followed by `JSON.stringify` and `localStorage.setItem` on every execution in [Map.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Map.svelte#L241).
  - Because the reactive block depends on `$mapStore`, it fires on every individual point move event during drag operations, not only on pointer-up.
- Reproduction:
  1. Open the map editor on a low-end device or browser with reduced performance.
  2. Drag a perimeter point across the canvas.
  3. Observe frame drops during drag; DevTools shows repeated `structuredClone` calls on every `pointermove` event.
- Risk:
  - Visible jank during map editing on underpowered hardware
  - Excessive localStorage writes can trigger browser storage quota warnings on maps with many points
- Minimal Fix:
  - Debounce `saveDraft()` to fire at most once per 500 ms; trigger a final save on pointer-up or focus-loss.
- Verification:
  - Profile drag operations before and after debounce on a constrained device; verify frame rate improves and localStorage write frequency drops.

### BUG-MED-012

- Title: `deleteMission` and `removeZoneFromMission` execute without a confirmation step
- Status: Confirmed
- Evidence:
  - `deleteMission(missionId)` calls `missionStore.deleteMission()` and then `deleteMissionDocument()` immediately on button click in [Mission.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Mission.svelte#L157).
  - `removeZoneFromMission(zoneId)` removes a zone from the active mission's `zoneIds` without confirmation in [Mission.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/pages/Mission.svelte#L227).
  - No undo mechanism exists for either operation.
- Reproduction:
  1. Open the mission editor with a mission that has a configured weekly schedule.
  2. Click the ✕ button next to the mission name.
  3. Mission and its schedule are deleted immediately; no recovery path exists.
- Risk:
  - Scheduled missions with zone configurations can be lost by a single misclick
  - Backend deletion is immediate; the browser back button does not restore the mission
- Minimal Fix:
  - Add a `confirm()` dialog or an inline "Sicher?" two-step confirmation before executing either delete operation.
- Verification:
  - Click the delete button and verify that confirmation is required before the mission is removed.

### BUG-MED-013

- Title: Zone index used as 1-based for name lookup but 0-based for progress display; zone index 0 is never named
- Status: Confirmed
- Evidence:
  - `runningZoneName` guards on `$telemetry.mission_zone_index > 0` and looks up `zoneIds[index - 1]` in [MissionWidget.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte#L119).
  - The progress counter displays the raw `mission_zone_index` without subtraction in [MissionWidget.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Dashboard/MissionWidget.svelte#L152).
  - If the backend ever emits `mission_zone_index = 0` for the first zone, the guard suppresses the zone name while the counter shows "Zone 0".
- Reproduction:
  1. Start a mowing mission where the backend begins with `mission_zone_index = 0`.
  2. Progress counter shows "Zone 0 / N" with no zone name displayed.
  3. Alternatively, if index is 1-based, "Zone 1" is correct in the counter but relies on `zoneIds[0]` for the name — which is consistent only if the implicit contract never changes.
- Risk:
  - Zone name is not displayed when the first zone is active, leaving users without context about which zone is being mowed
  - The implicit 1-based assumption is not documented and could break if backend telemetry changes
- Minimal Fix:
  - Clarify and enforce a single indexing convention; remove the `> 0` guard and align the name lookup with the display index explicitly.
- Verification:
  - Verify with a running mission that the zone name and zone counter display the same zone at all times.

## Low

### BUG-LOW-001

- Title: `ErrorOp` safety posture depends on repeated software stop commands, not a distinct hardware disable path
- Status: Confirmed
- Evidence:
  - `ErrorOp::run()` repeatedly calls `ctx.stopMotors()` in [ErrorOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/ErrorOp.cpp#L20).
  - No distinct hardware-level motor-disable or relay-cut path is visible in this Linux-side code path.
- Reproduction:
  1. Enter `ErrorOp`.
  2. Observe that the safety mechanism is repeated software motor stop rather than a proven hard disable path.
- Risk:
  - Limited containment depth if lower-level stop semantics change
- Minimal Fix:
  - Document or wire a confirmed hard-disable path if the hardware supports one.
- Verification:
  - On-device confirmation of hardware stop behavior in fault state.

### BUG-LOW-002

- Title: `checkBattery()` treats zero voltage as missing MCU data and suppresses battery safety handling
- Status: Confirmed
- Evidence:
  - `checkBattery()` returns early when `battery_.voltage < 0.1f` in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L1105).
- Reproduction:
  1. Present a telemetry collapse or malformed battery reading to `0.0`.
  2. Observe that battery safety logic is skipped for that cycle.
- Risk:
  - Real battery problems can be masked as telemetry absence
- Minimal Fix:
  - Differentiate “telemetry unavailable” from “zero/invalid voltage” explicitly.
- Verification:
  - Fault-injection tests for invalid battery telemetry values.

### BUG-LOW-003

- Title: `UndockOp` success depends entirely on pose delta plus charger-drop state
- Status: Confirmed
- Evidence:
  - `UndockOp` tracks distance only through `ctx.x/y` deltas in [UndockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/UndockOp.cpp#L28).
  - It marks success when charger contact dropped and travelled distance exceeds the configured threshold in [UndockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/UndockOp.cpp#L34).
- Reproduction:
  1. Use degraded localization with stale or poor position updates.
  2. Observe that undock success/failure can be misclassified.
- Risk:
  - Operational fragility during localization degradation
- Minimal Fix:
  - Cross-check pose-based completion with stronger dock-release evidence.
- Verification:
  - Undock tests under degraded odometry/GPS conditions.

### BUG-LOW-004

- Title: WebSocket reconnect timer can fire after `stopTelemetry()` in a race between timer expiry and flag reset
- Status: Confirmed
- Evidence:
  - `scheduleReconnect()` sets a 3-second `setTimeout`; the callback checks `if (active) connect()` in [websocket.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/api/websocket.ts#L19).
  - `stopTelemetry()` calls `clearReconnectTimer()` before setting `active = false` in [websocket.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/api/websocket.ts#L71).
  - If the timer callback has already been placed in the microtask queue by the time `clearTimeout` is called (timer already elapsed but callback not yet executed), `active` is still `true` at callback entry, and `connect()` is called after `stopTelemetry()` has returned.
- Reproduction:
  1. Let a reconnect cycle begin (WS disconnects, 3-second timer starts).
  2. Navigate away from the app at exactly the moment the timer fires.
  3. A new WebSocket connection is opened after `stopTelemetry()` completed.
- Risk:
  - Orphaned WebSocket connection that is never closed
  - Telemetry updates continue to arrive and mutate Svelte stores after navigation
  - Low-probability but reproducible on slow or throttled event loops
- Minimal Fix:
  - Set `active = false` before calling `clearReconnectTimer()`; re-check `active` inside `connect()` as a guard.
- Verification:
  - Simulate timer expiry concurrent with `stopTelemetry()` call; verify no connection is opened after stop.

### BUG-LOW-005

- Title: Battery percentage voltage range and GPS start-accuracy threshold are hardcoded with no UI explanation
- Status: Confirmed
- Evidence:
  - Battery percent maps `22 V → 0 %` and `29.4 V → 100 %` unconditionally in [telemetry.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/telemetry.ts#L18).
  - GPS readiness requires `gps_acc <= 0.08 m` but the preflight hint only says "Auf RTK Fix oder guten RTK Float warten" without stating the threshold in [robotUi.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/utils/robotUi.ts#L21).
  - Neither constant is read from the backend config; a different battery chemistry or stricter accuracy requirement silently breaks the calculations.
- Reproduction:
  1. Install a 7S or 4S battery pack instead of 6S.
  2. Observe that the battery percentage display is wrong (could show 0 % at a healthy voltage, or 100 % prematurely).
  3. Or: set `gps_acc` to 0.09 m (valid for RTK Float in many conditions); preflight blocks start with no actionable explanation of the 0.08 m requirement.
- Risk:
  - Low for standard configurations; high if hardware ever deviates from the hardcoded 6S assumption
  - Operator confusion during GPS preflight when accuracy is just above the invisible threshold
- Minimal Fix:
  - Read voltage range and GPS accuracy threshold from the config API or expose them as named constants with a short preflight hint message that states the required value explicitly.
- Verification:
  - Confirm that the preflight GPS hint text shows the current accuracy requirement numerically.

## Plausible But Unproven

### BUG-PLAUS-001

- Title: MQTT 30-minute disconnect symptom may be amplified by reconnect/session design
- Status: Plausible, partially mitigated on 2026-04-02
- Evidence:
  - MQTT uses `clean_session=true` in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L248).
  - Reconnect is automatic with backoff in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L268).
  - Subscription recovery is best-effort only in [MqttClient.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp#L70).
- Reproduction:
  - Not proven from repository alone.
- Risk:
  - Intermittent loss of remote command channel despite apparent client liveness
- Minimal Fix:
  - Add reconnect-state verification and subscription health checks.
- Verification:
  - Long-duration MQTT soak tests with broker restarts and idle periods.
- Resolution note:
  - Subscription health and reconnect recovery are now more explicit inside `MqttClient`, but the long-run broker-specific disconnect symptom remains unproven without soak testing against the productive broker path.

### BUG-PLAUS-002

- Title: I2C mux/channel switching may create intermittent GPIO-side effects
- Status: Plausible
- Evidence:
  - LED writes explicitly select the EX3 mux channel in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L603).
  - Other I2C users also switch mux channels in [SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp#L155).
- Reproduction:
  - No concrete failing path proven in this static sweep.
- Risk:
  - Intermittent LED or auxiliary I2C behavior under mixed access patterns
- Minimal Fix:
  - Centralize mux ownership or serialize channel-sensitive I2C access.
- Verification:
  - Hardware soak testing with concurrent IMU and LED activity.

### BUG-PLAUS-003

- Title: Dock-route failure path is operationally brittle after upstream navigation degradation
- Status: Plausible
- Evidence:
  - `Mow` and `NavToStart` escalate certain failures to `Dock` in [MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp#L42) and [NavToStartOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/NavToStartOp.cpp#L53).
  - `DockOp::begin()` goes directly to `Error` if no dock route can be built in [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp#L15).
- Reproduction:
  - Not proven in this sweep without dynamic setup.
- Risk:
  - Recovery path collapses from “go home safely” to immediate hard fault
- Minimal Fix:
  - Define an explicit degraded recovery behavior when dock routing is unavailable.
- Verification:
  - Integration tests with GPS/perimeter degradation and unreachable dock route.

## Notes

- This report is based on static repository evidence only.
- The severities above reflect runtime and safety impact, not ease of fixing.
- No production code changes were made while preparing this report.
