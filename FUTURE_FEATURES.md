# Future Features

## Status

This roadmap contains architecture-grounded feature candidates for `sunray-core`.
It is based only on current repository evidence, known weak points, and documented operational opportunities.

Feature means:
- new operator-facing capability
- new service/runtime capability
- new recoverability or validation workflow

Feature does not mean:
- fixing a confirmed bug
- refactoring for clarity only
- generic modernization without a landing zone

## Phases

| Phase | Meaning |
| --- | --- |
| F1 | feasible after current P1 improvements, low hardware uncertainty |
| F2 | feasible after runtime and telemetry contracts stabilize |
| F3 | feasible only after field validation or external dependency clarification |

## Feature Candidates

### FF-001

- Title: Runtime Safety Status Surface
- Category: safety enhancements, telemetry and fault analytics
- User/operator value:
  - Operators get a single safety view for stop-path health, GPS recovery state, charger state, watchdog state, and comms state.
- Architecture fit:
  - Fits the existing `Robot` telemetry path plus `WebSocketServer` and optional `MqttClient`.
  - Does not require a new runtime owner; it extends the current Pi-side orchestration model.
- Affected modules:
  - `core/Robot.cpp`
  - `core/WebSocketServer.cpp`
  - `core/MqttClient.cpp`
  - `webui-svelte`
- Hardware dependencies:
  - Depends only on already available runtime signals from Pi and STM32 telemetry.
  - No new hardware assumption required.
- Safety implications:
  - Positive if clearly marked as status visibility, not as a hard safety guarantee.
  - Must not imply a hard-stop capability that is currently `UNKNOWN`.
- Implementation risk:
  - Low
- Estimated complexity:
  - Medium
- Blockers:
  - Better health-telemetry model from improvement item `IB-007`.
- Recommended phase:
  - F1
- Confidence level:
  - High

### FF-002

- Title: Guided Fault Recovery Workflow
- Category: recovery automation, diagnostics and serviceability
- User/operator value:
  - When the robot enters `GpsWait`, `Dock`, `Charge`, or `Error`, the UI can present the current reason, recommended next checks, and allowed recovery actions.
- Architecture fit:
  - Fits current event and reason model in `Robot`, the existing UI notice path, and the WebSocket API.
  - Builds on existing op names and transition reasons rather than replacing them.
- Affected modules:
  - `core/Robot.cpp`
  - `core/EventMessages.cpp`
  - `core/WebSocketServer.cpp`
  - `webui-svelte`
- Hardware dependencies:
  - Uses existing telemetry and event reasons only.
- Safety implications:
  - Must avoid suggesting unsafe recovery actions while motion-related faults are active.
  - Should remain advisory unless the underlying command path is hardened first.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Command serialization and reason taxonomy need to stabilize first.
- Recommended phase:
  - F2
- Confidence level:
  - High

### FF-003

- Title: On-Device Incident Bundle Export
- Category: diagnostics and serviceability, telemetry and fault analytics
- User/operator value:
  - Support and field operators can export one bundle containing config snapshot, recent events, session history, runtime health, and hardware/runtime metadata.
- Architecture fit:
  - Fits the existing history database, event recorder, config path handling, and WebSocket HTTP backend.
  - Reuses the current backend as the export source of truth.
- Affected modules:
  - `core/storage/HistoryDatabase.cpp`
  - `core/Robot.cpp`
  - `core/WebSocketServer.cpp`
  - docs and operator flows
- Hardware dependencies:
  - None beyond already captured runtime telemetry.
- Safety implications:
  - Improves post-incident diagnosis.
  - Must not block the control loop or introduce heavy synchronous work in a fault path.
- Implementation risk:
  - Low
- Estimated complexity:
  - Medium
- Blockers:
  - Need a defined export schema and retention boundary.
- Recommended phase:
  - F1
- Confidence level:
  - High

### FF-004

- Title: Hardware Self-Test and Bring-Up Report
- Category: self-test and hardware validation, calibration tooling
- User/operator value:
  - Field technicians can run one guided validation flow for UART, I2C, LEDs, buzzer, IMU availability, charger signal plausibility, and board identity before operation.
- Architecture fit:
  - Fits current probe binaries under `tools/`, diagnostics callbacks, and startup/runtime logging.
  - Builds on existing hardware check scripts instead of inventing a separate subsystem.
- Affected modules:
  - `tools/*`
  - `scripts/check_alfred_hw.sh`
  - `scripts/check_rm18_uart.sh`
  - `core/WebSocketServer.cpp`
  - possibly `webui-svelte`
- Hardware dependencies:
  - Uses proven UART/I2C/port-expander paths.
  - Must keep unknown hard-stop and dock-contact assumptions marked as unknown.
- Safety implications:
  - Positive for serviceability and preflight validation.
  - Any actuator tests must remain low-risk and require explicit operator initiation.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Diagnostic stop-path hardening should land first.
- Recommended phase:
  - F1
- Confidence level:
  - High

### FF-005

- Title: Dock Geometry Validation Assistant
- Category: navigation and RTK resilience, calibration tooling
- User/operator value:
  - Map authors can validate dock corridor, final approach geometry, and likely failure zones before deployment.
- Architecture fit:
  - Fits current map, planner, docking metadata, and WebSocket map-management flow.
  - Uses existing dock route concepts rather than inventing a new planner.
- Affected modules:
  - `core/navigation/Map.cpp`
  - `core/navigation/Planner.cpp`
  - `core/WebSocketServer.cpp`
  - `webui-svelte`
- Hardware dependencies:
  - No new hardware needed.
  - Benefits from RTK-quality positioning but does not require new sensors.
- Safety implications:
  - Can reduce route-to-error escalations caused by poor dock geometry.
  - Must present validation as advisory unless backed by full planner checks.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Dock-route error handling and dock metadata conventions need to be stable enough to analyze.
- Recommended phase:
  - F2
- Confidence level:
  - High

### FF-006

- Title: RTK Degradation Analyzer
- Category: navigation and RTK resilience, telemetry and fault analytics
- User/operator value:
  - Operators can see whether failures came from no-signal, long float, stale fix age, dock-area multipath, or poor recovery hysteresis.
- Architecture fit:
  - Fits current GPS resilience logic, `GpsWait`, `StateEstimator`, and telemetry path.
  - Extends existing GPS state rather than replacing the navigation stack.
- Affected modules:
  - `core/Robot.cpp`
  - `core/navigation/StateEstimator.cpp`
  - `hal/GpsDriver/*`
  - `core/WebSocketServer.cpp`
  - `webui-svelte`
- Hardware dependencies:
  - Uses the existing GPS/RTK receiver path.
  - Exact production RTCM/NTRIP setup remains unknown.
- Safety implications:
  - Positive for diagnosing degraded navigation before unsafe operator decisions.
  - Must not encourage continued mowing beyond current safety policy.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Receiver configuration and production correction path are not fully documented.
- Recommended phase:
  - F2
- Confidence level:
  - Medium

### FF-007

- Title: Mission Resume With Safety Preconditions
- Category: recovery automation, remote management
- User/operator value:
  - After recoverable interruptions like `GpsWait` or obstacle escape, the system can offer a controlled resume proposal only when map, pose, and safety preconditions match.
- Architecture fit:
  - Fits current `nextOp` resume pattern, mission tracking, map fingerprint guard, and op model.
  - Extends existing recovery semantics already present in `Escape*` and `GpsWait`.
- Affected modules:
  - `core/Robot.cpp`
  - `core/op/*`
  - `core/navigation/Map.cpp`
  - `webui-svelte`
- Hardware dependencies:
  - Depends on existing GPS and odometry only.
- Safety implications:
  - High sensitivity: resume must remain blocked if map changed, pose confidence is weak, or safety unknowns are active.
- Implementation risk:
  - Medium to High
- Estimated complexity:
  - High
- Blockers:
  - Needs clearer resume gating and command determinism first.
- Recommended phase:
  - F2
- Confidence level:
  - Medium

### FF-008

- Title: Home Assistant Operational Workflow Pack
- Category: Home Assistant workflows, remote management
- User/operator value:
  - Provides stable HA entities and automations for mower status, docking, recovery state, battery alerts, and controlled operator actions.
- Architecture fit:
  - Fits the existing `MqttClient` discovery path and optional MQTT architecture.
  - Extends current MQTT/HA integration instead of bypassing it.
- Affected modules:
  - `core/MqttClient.cpp`
  - MQTT topic docs
  - Home Assistant docs/workflow examples
- Hardware dependencies:
  - None beyond current runtime telemetry.
- Safety implications:
  - Remote actions must respect the same command safety rules as local UI commands.
  - HA workflows must not retain or replay unsafe motion commands.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - MQTT command-channel hardening and topic-contract stabilization are required first.
- Recommended phase:
  - F2
- Confidence level:
  - Medium

### FF-009

- Title: Safe Update Assistant for Alfred
- Category: deployment robustness, remote management
- User/operator value:
  - Gives operators a guided update flow with preflight checks, service switchover steps, validation gates, and rollback instructions.
- Architecture fit:
  - Fits the current script-and-doc based deployment model in `scripts/` and `docs/`.
  - Does not require a new package manager or OTA layer.
- Affected modules:
  - `scripts/install_sunray.sh`
  - `scripts/flash_alfred.sh`
  - deployment docs
  - possibly a small backend version/status endpoint
- Hardware dependencies:
  - Uses current Pi and STM32 deployment paths.
  - No OTA hardware assumptions.
- Safety implications:
  - Positive if it enforces staged validation and rollback readiness.
  - Must not imply unattended updates unless supervision and rollback become proven.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Real productive service layout and rollback behavior are still partially unknown.
- Recommended phase:
  - F2
- Confidence level:
  - High
- Current status:
  - Pi-side update assistant is now functionally landed:
    - WebUI can check for updates
    - WebUI can trigger Pi-side update/install/restart
    - version file and reconnect flow are visible to the operator
  - Remaining gap is STM32 firmware OTA, which is tracked separately as `FF-014`.

### FF-014

- Title: STM32 Firmware OTA for Alfred
- Category: deployment robustness, remote management
- User/operator value:
  - Extends the now-working Pi-side update flow to the RM18 STM32 firmware, so Alfred can be updated without a separate wired flashing session.
- Architecture fit:
  - Must land on top of the existing Pi-side OTA/update assistant, not beside it.
  - Must integrate with the current firmware flashing path in `scripts/flash_alfred.sh` and the documented OpenOCD / `arduino-cli` workflow.
- Affected modules:
  - `scripts/flash_alfred.sh`
  - OTA/update backend surface
  - update docs and rollback notes
- Hardware dependencies:
  - Requires the existing STM32 flashing path on Alfred to be callable and recoverable remotely.
  - No repository evidence yet proves a safe unattended rollback path after a bad STM flash.
- Safety implications:
  - High: a failed or interrupted STM update can remove the low-level motion/safety runtime.
  - Must not be presented as safe unattended OTA until rollback/recovery is proven.
- Implementation risk:
  - High
- Estimated complexity:
  - Medium to High
- Blockers:
  - No proven automated rollback for failed STM flash
  - no verified production-safe recovery path after interrupted firmware update
  - no field proof yet that the current flash tooling can be safely driven through the WebUI
- Recommended phase:
  - F3
- Confidence level:
  - Medium

### FF-010

- Title: Long-Run Comms Health Analytics
- Category: telemetry and fault analytics, communication resilience
- User/operator value:
  - Exposes reconnect counts, broker session behavior, UART quality indicators, and recent comms degradations for long-duration troubleshooting.
- Architecture fit:
  - Fits current MQTT client, serial driver, runtime telemetry, and incident export path.
- Affected modules:
  - `core/MqttClient.cpp`
  - `hal/SerialRobotDriver/SerialRobotDriver.cpp`
  - `core/Robot.cpp`
  - telemetry/export surfaces
- Hardware dependencies:
  - None beyond current transports.
- Safety implications:
  - Improves detection of silent degradation.
  - Must remain observational, not a replacement for hard safety logic.
- Implementation risk:
  - Low to Medium
- Estimated complexity:
  - Medium
- Blockers:
  - More useful after MQTT and command-threading fixes land.
- Recommended phase:
  - F2
- Confidence level:
  - High

### FF-011

- Title: Operator Calibration Workspace
- Category: calibration tooling, diagnostics and serviceability
- User/operator value:
  - Provides one guided place for IMU calibration, wheel/tick validation, undock distance validation, and dock-approach calibration.
- Architecture fit:
  - Fits existing diagnostic motor/drive/turn actions, IMU calibration hook, and pose/tick reporting.
- Affected modules:
  - `core/Robot.cpp`
  - `core/WebSocketServer.cpp`
  - `hal/Imu/*`
  - `webui-svelte`
- Hardware dependencies:
  - Uses current IMU, odometry, and charger telemetry.
  - Cannot assume extra reference sensors.
- Safety implications:
  - Calibration motions must stay explicitly operator-triggered and bounded.
  - Should not run while autonomous mission modes are active.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium
- Blockers:
  - Diagnostic-stop safety and command-path determinism should be improved first.
- Recommended phase:
  - F1
- Confidence level:
  - High

### FF-012

- Title: Degraded-Mode Maintenance API
- Category: remote management, diagnostics and serviceability
- User/operator value:
  - Supports controlled low-authority maintenance actions when normal mowing is not allowed, such as collecting state, confirming hardware presence, or running bounded self-tests.
- Architecture fit:
  - Fits current WebSocket command and diagnostic model, plus history and telemetry export.
  - Extends the serviceability role of the backend without moving control out of `Robot`.
- Affected modules:
  - `core/WebSocketServer.cpp`
  - `core/Robot.cpp`
  - `webui-svelte`
  - optional MQTT status exposure
- Hardware dependencies:
  - None beyond current runtime capabilities.
- Safety implications:
  - Must remain clearly separated from normal mission control.
  - Any motion-capable maintenance actions must preserve bounded runtime and explicit operator initiation.
- Implementation risk:
  - Medium
- Estimated complexity:
  - Medium to High
- Blockers:
  - Needs clearer auth/authorization story and command serialization first.
- Recommended phase:
  - F3
- Confidence level:
  - Medium

### FF-013

- Title: Optical Flow Assisted GPS Coast
- Category: navigation and RTK resilience, telemetry and fault analytics
- User/operator value:
  - Reduces drift during GPS outages by adding a third motion source for short shadow and multipath intervals.
- Architecture fit:
  - Fits as an optional Pi-side visual odometry measurement path feeding the existing estimator instead of replacing the current navigation owner.
- Affected modules:
  - `core/navigation/StateEstimator.cpp`
  - optional camera/vision integration on Pi side
  - `webui-svelte` diagnostics/status views
- Hardware dependencies:
  - Raspberry Pi camera
  - optical-flow processing stack
  - ground-height / scale calibration
- Safety implications:
  - Could improve short GPS-outage resilience.
  - Must remain bounded and secondary to existing safety degradation logic, not a justification for unlimited GPS-free mowing.
- Implementation risk:
  - Medium to High
- Estimated complexity:
  - Medium to High
- Blockers:
  - Camera hardware path, calibration workflow, and estimator measurement extension are not yet present in the current runtime.
- Recommended phase:
  - F2
- Confidence level:
  - Medium

## Features Not Recommended Yet

- Autonomous OTA update system
  - Reason: no proven OTA path, incomplete rollback proof, and unclear production supervision.
- New hardware-driven dock sensor feature
  - Reason: current repo does not prove a dedicated dock-contact input exists.
- LoRa/CAN remote control extensions
  - Reason: no active runtime integration path is proven in the current repo snapshot.

## Summary

- F1 features build directly on existing diagnostics, telemetry, and runtime state.
- F2 features depend on hardening current correctness and contract gaps first.
- F3 features are plausible extensions, but they need operational or interface proof before implementation should start.
