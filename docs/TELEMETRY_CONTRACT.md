# TELEMETRY_CONTRACT

Last updated: 2026-03-28

## Purpose

This document freezes the telemetry contract currently emitted by
`Robot::buildTelemetry()` and serialized by `WebSocketServer::buildTelemetryJson()`
before `Robot::run()` is refactored.

Primary sources:

- [`core/Robot.cpp:484`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L484)
- [`core/WebSocketServer.h:48`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.h#L48)
- [`tests/test_websocket_server.cpp:17`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_websocket_server.cpp#L17)

## Stability Rule

For Commit 1:

- existing keys are frozen
- existing enum-like string values are frozen
- field meanings and precedence are frozen
- only structural refactoring is allowed

Any intentional contract change belongs in Commit 2 or later and must update:

- this document
- [`tests/test_websocket_server.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_websocket_server.cpp)
- affected frontend consumers

## Payload Shape

Serialized payload type:

- `type = "state"`

Current telemetry keys:

- `op`
- `x`
- `y`
- `heading`
- `battery_v`
- `charge_v`
- `gps_sol`
- `gps_text`
- `gps_acc`
- `gps_lat`
- `gps_lon`
- `bumper_l`
- `bumper_r`
- `lift`
- `motor_err`
- `uptime_s`
- `mcu_v`
- `pi_v`
- `imu_h`
- `imu_r`
- `imu_p`
- `diag_active`
- `diag_ticks`
- `ekf_health`
- `ts_ms`
- `state_since_ms`
- `state_phase`
- `resume_target`
- `event_reason`
- `error_code`

Special note:

- `pi_v` is currently serialized as a compile-time constant from the backend serializer
  and is not injected through `TelemetryData`
- that means tests can freeze its presence, but not choose its value through the
  `TelemetryData` struct

Serializer coverage already exists in:

- [`tests/test_websocket_server.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_websocket_server.cpp)

Known gap:

- serializer tests freeze the JSON shape, but not the higher-level business
  meaning of all values produced by `Robot::buildTelemetry()`

## Frozen Op Names

The current runtime uses these op names in telemetry:

- `Idle`
- `Undock`
- `NavToStart`
- `Mow`
- `Dock`
- `Charge`
- `WaitRain`
- `GpsWait`
- `EscapeReverse`
- `EscapeForward`
- `Error`

## Frozen `state_phase` Mapping

Derived in `Robot::currentStatePhase()`:

- `Idle` -> `idle`
- `Undock` -> `undocking`
- `NavToStart` -> `navigating_to_start`
- `Mow` -> `mowing`
- `Dock` -> `docking`
- `Charge` -> `charging`
- `WaitRain` -> `waiting_for_rain`
- `GpsWait` -> `gps_recovery`
- `EscapeReverse` -> `obstacle_recovery`
- `EscapeForward` -> `obstacle_recovery`
- `Error` -> `fault`
- unknown fallback -> `unknown`

## Frozen `resume_target` Meaning

- empty string when no `nextOp` is present
- otherwise the `name()` of the active op's `nextOp`

## Frozen `event_reason` Precedence

`Robot::buildTelemetry()` currently resolves exactly one dominant
`event_reason` in this precedence order:

1. `resume_blocked_map_changed`
2. `battery_critical`
3. `battery_low`
4. `rain_detected`
5. `lift_triggered`
6. `motor_fault`
7. `bumper_triggered`
8. `charger_connected`
9. `kidnap_detected`
10. `gps_fix_timeout`
11. `gps_signal_lost`
12. `undocking`
13. `navigating_to_start`
14. `mission_active`
15. `docking`
16. `charging`
17. `waiting_for_rain`
18. `gps_recovery_wait`
19. `obstacle_recovery`
20. `none`

Refactor implication:

- changing the order changes UI semantics even if the key names stay the same

## Frozen `error_code` Semantics

`error_code` is only populated when `op == "Error"`.

Current values:

- `ERR_BATTERY_CRITICAL`
- `ERR_LIFT`
- `ERR_MOTOR_FAULT`
- `ERR_GPS_TIMEOUT`
- `ERR_GENERIC`

Current precedence:

1. critical battery
2. lift
3. motor fault
4. GPS timeout
5. generic fallback

## Value Notes

- `heading` is emitted in radians
- `imu_h`, `imu_r`, and `imu_p` are emitted in degrees
- `gps_sol` currently maps to:
  - `4` for RTK/fix
  - `5` for float
  - `0` for no fix
- `gps_text` currently maps to:
  - `RTK`
  - `Float`
  - `---`
- `ts_ms` is current robot uptime in milliseconds
- `state_since_ms` is the active op start timestamp in milliseconds since robot start

## Required Pre-Refactor Test Follow-Up

Before or together with Commit 1, add at least one focused `Robot` test that
checks representative telemetry semantics, not just JSON serialization.
