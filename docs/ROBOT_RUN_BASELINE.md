# ROBOT_RUN_BASELINE

Last updated: 2026-03-28

## Purpose

This document captures the current pre-refactor shape of `Robot::run()` so that
Commit 1 can be checked against a concrete baseline instead of a vague
"behavior should stay the same" promise.

## Current Size

Primary implementation:

- [`core/Robot.cpp:87`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L87)
- [`core/Robot.cpp:299`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L299)

Observed shape:

- one control-loop function of roughly 200 lines
- one large `try` block with twelve inline phases
- several phases mix state mutation, hardware side effects, event dispatch, and telemetry preparation

## Current Responsibilities Inside `Robot::run()`

Observed execution order:

1. Monotonic timing:
   - updates `now_ms_`
   - derives `dt_ms` from previous loop
2. Hardware tick:
   - calls `hw_->run()`
3. Sensor acquisition:
   - reads odometry, sensors, battery, IMU
   - pushes valid IMU data into `stateEst_`
4. Schedule handling:
   - once per minute checks `schedule_.isActiveNow()`
   - emits timetable start/stop events to the active Op
5. Obstacle cleanup:
   - once per second calls `map_.cleanupExpiredObstacles(now_ms_)`
6. State estimation and GPS ingestion:
   - updates odometry-based state estimate
   - polls GPS, updates EKF inputs and `gpsLastFixTime_ms_`
   - broadcasts changed NMEA GGA lines to WebSocket
7. `OpContext` assembly:
   - copies current sensors, battery, odometry, pose, perimeter, docking, GPS, resume, and navigation references
8. Safety and resilience guards before state-machine tick:
   - `checkBattery(ctx)`
   - `monitorGpsResilience(ctx)`
9. State machine:
   - `opMgr_.tick(ctx)`
10. Diagnostic flow:
   - runs scheduled diagnostic motor requests
   - can command motors directly
   - can early-return and skip the normal remainder of the loop
11. Manual drive:
   - applies joystick PWM only while idle and within watchdog freshness
12. Safety stop and event edge detection:
   - immediate motor stop on bumper, lift, or motor fault
   - dispatches edge-triggered obstacle, lift, motor-fault, and rain events
13. UI/status output:
   - updates LEDs
14. Telemetry fan-out:
   - builds `TelemetryData`
   - pushes to WebSocket and MQTT
15. Fault containment:
   - catches exceptions
   - stops motors
   - marks the robot as not running

## Refactor Sensitivities

The following parts are especially easy to change accidentally during a
"structure only" refactor:

- timing-sensitive fields derived from `now_ms_` and `dt_ms`
- GPS side effects:
  - `gpsLastFixTime_ms_`
  - NMEA broadcast on change only
- the fact that diagnostic mode returns before manual drive, safety handling,
  LED updates, telemetry push, and `controlLoops_++`
- edge-triggered event dispatch via `previousSensors_`
- the current ordering of:
  - `checkBattery(ctx)`
  - `monitorGpsResilience(ctx)`
  - `opMgr_.tick(ctx)`
  - diag handling
  - manual drive
  - safety stop
  - telemetry push

## Test Gaps In `tests/test_robot.cpp`

Current `Robot` tests cover construction, init, basic loop execution,
selected safety events, some state transitions, and battery handling.

Before Commit 1, these gaps remain important:

- no smoke test that freezes the current `Robot` telemetry output contract
- no test that checks `state_phase`, `resume_target`, `event_reason`, or `error_code`
- no test that confirms the current ordering-sensitive diag early-return path
- no test for NMEA forwarding only on changed GGA lines
- no test for once-per-minute schedule transition dispatch from `run()`
- no test for manual-drive watchdog behavior in `Idle`
- no targeted test that proves `controlLoops_` is skipped on the diag early-return path
- no regression test that compares representative telemetry before and after a structural extraction

## Minimum Guard Rails Before Commit 1

- keep the telemetry contract snapshot in [`docs/TELEMETRY_CONTRACT.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/TELEMETRY_CONTRACT.md)
- do not rename payload keys or business enums in Commit 1
- add at least one smoke test that exercises current telemetry semantics before moving helper code out of `run()`
