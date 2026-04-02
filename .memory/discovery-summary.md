# discovery-summary.md

## Runtime

- `FACT`: `main.cpp` bootstraps config, hardware driver selection, `Robot`, optional GPS, WebSocket, optional MQTT, then enters `robot.loop()`.
- `FACT`: Pi-side runtime ownership sits primarily in `Robot`, `WebSocketServer`, `MqttClient`, `StateEstimator`, `Map`, and the op state machine.
- `FACT`: STM32 owns PWM, brake/low-level motor behavior, ADC reads, encoder counting, and watchdog feeding.
- `FACT`: Pi owns high-level mission logic, safety policy, telemetry, history, UI/API exposure, and Linux-side shutdown decisions.
- `FACT`: Watchdog chain is layered: STM32 hardware watchdog plus Pi-side op/time safety guards.

## Build

- `FACT`: Backend build is CMake.
- `FACT`: Frontend build is npm + Vite/Svelte.
- `FACT`: Pi deployment path is documented through `scripts/install_sunray.sh` and generated `sunray-core.service`.
- `FACT`: STM32 flash path is `arduino-cli` + OpenOCD via `scripts/flash_alfred.sh`.
- `FACT`: A read-only deployment verifier now exists at `scripts/check_deploy_state.sh` for artefacts, runtime files, and rollback-anchor visibility.
- `FACT`: `config.example.json` now covers the active Alfred runtime baseline for key hardware, docking, planner, and path defaults.
- `FACT`: WebUI now includes a `Verlauf` page backed by `/api/history/events`, `/api/history/sessions`, `/api/statistics/summary`, and `/api/history/export`.
- `UNKNOWN`: OTA/update rollback beyond the documented manual paths.

## Hardware

- `FACT`: Pi talks to STM32 on `/dev/ttyS0`.
- `FACT`: Pi uses `/dev/i2c-1` with mux/expanders including `0x70`, `0x21`, `0x20`, `0x22`.
- `FACT`: EX1/EX2/EX3 cover IMU/fan, buzzer, and panel LEDs.
- `FACT`: Dock truth on Linux is still inferred from charger voltage threshold, but the signal is now debounced before it becomes `battery_.chargerConnected`.
- `UNKNOWN`: Separate hard dock-contact line, proven hard motor-disable relay, active CAN usage.

## Safety

- `FACT`: Stop paths include operator emergency stop, bumper/lift/motor-fault safety transitions, battery critical shutdown, GPS degrade handling, op watchdogs, and now Pi-side MCU-comms-loss faulting.
- `FACT`: Active diagnostics now abort immediately on stop-button press.
- `FACT`: External WebSocket/MQTT commands are serialized onto the `Robot` control-loop thread.
- `FACT`: MCU comm loss after at least one confirmed connection now yields local motor stop plus `ERR_MCU_COMMS`.
- `FACT`: MQTT command subscription is retried after reconnect or subscribe failure.
- `FACT`: Debounced dock/charge truth is now observable as `charger_connected` in Web telemetry.
- `FACT`: Compact runtime health telemetry now exposes comms, GPS degrade, battery guard, recovery, and watchdog-notice state for operators and future diagnostics.
- `FACT`: Frontend command sends now produce a visible transient error notice if the WebSocket send path is unavailable.
- `FACT`: Frontend keeps the active mission card visible during transient recovery ops while telemetry still points at an active mission.
- `FACT`: Dock-critical GPS recovery now waits for RTK Fix before leaving `GpsWait`; non-dock recovery still accepts RTK Float.
- `FACT`: Dock retries now escalate early after repeated non-progressive attempts instead of replaying the same stalled approach until the retry budget is exhausted.
- `FACT`: `Mow` can now bridge short GPS outages through a bounded `mow_gps_coast_ms` window when degraded fusion remains operational after prior GPS signal.
- `FACT`: Later dock retries may now vary their approach target laterally via `dock_retry_lateral_offset_m`, with fallback to the original retry path if offset routing fails.
- `FACT`: Encoder-visible stuck conditions now raise a dedicated runtime guard: `Robot` compares commanded linear motion with fused ground speed and dispatches op-specific `onStuck()` recovery.
- `FACT`: Repeated unresolved stuck recovery now escalates deterministically to `Error` via `ERR_STUCK` / `stuck_recovery_exhausted` after `stuck_recovery_max_attempts`.
- `FACT`: Alfred runtime GPS-loss thresholds are now tuned more conservatively by default: `gps_no_signal_ms=15000`, `gps_recover_hysteresis_ms=3000`, `ekf_gps_failover_ms=20000`.
- `FACT`: Alfred runtime now also carries `mow_gps_coast_ms=20000`.
- `FACT`: Alfred runtime stuck-detection defaults are now `stuck_detect_timeout_ms=3000` and `stuck_detect_min_speed_ms=0.03`.
- `FACT`: Alfred runtime dock-retry geometry now includes `dock_retry_lateral_offset_m=0.10`.
- `FACT`: Automated regression coverage now includes repeated charger-contact flap damping, Charge grace-expiry handling, and degraded MQTT enabled lifecycle behavior.
- `FACT`: History summary now exposes grouped incident counters by reason, type, and level for field diagnostics and optimization statistics.

## Bugs

### Top Confirmed

- `BUG-MED-003`: dock/contact is threshold-derived from charger voltage
- `BUG-MED-004`: critical battery path can mis-handle unstable dock signal
- `BUG-MED-001`: UART gap framing uses a stale timestamp through a burst
- `BUG-LOW-001`: `ErrorOp` depends on software stop, not a proven hard disable path
- `BUG-LOW-002`: zero battery telemetry is treated as missing data
- `BUG-LOW-003`: `UndockOp` success depends on pose delta plus charger-drop only

### Top Plausible

- `BUG-PLAUS-001`: long-run MQTT disconnect symptom still needs soak confirmation
- `BUG-PLAUS-002`: I2C mux/channel switching may cause intermittent side effects
- docking/path-to-error brittleness after degraded conditions remains operationally fragile

## Improvements

- `IB-005`: charger-contact debounce and dock-state observability
- `IB-008`: deployment and rollback procedure hardening
- `IB-009`: configuration ownership cleanup
- `IB-007`: centralized runtime health telemetry (done)
- `IB-010`: startup self-report for hardware/safety assumptions
- `IB-011`: break out high-risk safety seams from `Robot.cpp`
- `IB-012`: require RTK Fix before resuming dock-critical approach (done)
- `IB-013`: detect non-progressive dock retries and escalate earlier (done)
- `IB-014`: surface failed WebSocket command sends to the operator UI (done)
- `IB-015`: keep the active mission card visible through transient recovery ops (done)
- `IB-016`: tune GPS-loss thresholds for safer EKF coast through short shadow zones (done)
- `IB-018`: add encoder-based stuck detection and recovery dispatch (done)
- `IB-017`: add bounded mowing GPS coast on degraded EKF fusion (done)
- `IB-019`: add lateral-offset dock retry geometry for repeated contact misses (done)
- `IB-020`: persist field-usable incident counters in the history summary (done)

## Future

- `FF-001`: runtime safety status surface
- `FF-003`: incident bundle export
- `FF-004`: hardware self-test and bring-up report
- `FF-002`: guided fault recovery workflow
- `FF-005`: dock geometry validation assistant
- `FF-006`: RTK degradation analyzer
- `FF-007`: mission resume with safety preconditions
- `FF-010`: long-run comms health analytics
- `FF-008`: Home Assistant workflow pack
- `FF-011`: operator calibration workspace
