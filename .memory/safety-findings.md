# Safety Findings

## Purpose

Short evidence-backed safety notes for `sunray-core`.

## Confirmed Safeguards

- `FACT`: `Robot::checkBattery()` is a strong critical-battery stop path: motors zeroed, buzzer on, power hold dropped, loop stopped.
- `FACT`: `Robot::monitorOpWatchdog()` enforces op timeouts; docking escalates timeout to `Error`.
- `FACT`: `Robot::monitorGpsResilience()` latches no-signal and fix-timeout events and clears them only after hysteresis.
- `FACT`: `tickSafetyStop()` zeros motors on bumper, lift, and motor-fault conditions and dispatches op-specific handlers.
- `FACT`: `ErrorOp`, `WaitRainOp`, `DockOp::begin`, `UndockOp::begin`, and multiple transition handlers explicitly stop motors.
- `FACT`: STM32 firmware has a local 2 s watchdog and a 3 s motor-command timeout.

## Confirmed Weaknesses

- `FACT`: Pi-side communication loss detection sets `mcuConnected = false` but no direct `Robot` safety transition based solely on that flag was proven.
- `FACT`: Dock-state truth in runtime depends on charge-voltage-derived `chargerConnected`, not on a separately proven dock-contact sensor.
- `FACT`: GPS degraded mode can continue movement at reduced speed before no-signal or fix-timeout thresholds trigger.

## Critical Unknowns

- `UNKNOWN`: physical hard-cut path for emergency stop
- `UNKNOWN`: dedicated hardware motor-enable or safety relay path
- `UNKNOWN`: external Linux supervisor or watchdog for the Pi process

## Follow-Up Audit Targets

- verify whether comms loss should trigger immediate `Error` or hard stop in Pi runtime
- verify real Alfred dock-contact behavior on hardware
- verify whether e-stop cuts power independently of firmware and UART
- verify timeout interaction between Pi-side stop requests and STM32 `motorTimeout`
