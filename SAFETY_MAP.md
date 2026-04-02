# Safety Map

## Scope

This map identifies safety-critical paths from active repository evidence only.

Primary evidence:

- `core/Robot.cpp`
- `core/op/*`
- `core/navigation/LineTracker.cpp`
- `hal/SerialRobotDriver/SerialRobotDriver.cpp`
- `alfred/firmware/rm18.ino`
- scenario and unit tests under `tests/`

Each item is tagged as `FACT`, `INFERENCE`, or `UNKNOWN`.

## Safety-Critical Areas In Scope

- motor enable and motion suppression
- emergency stop
- communication timeout
- watchdog reset
- GPS invalid and degraded navigation
- docking mismatch and charger-contact handling
- battery critical handling
- recovery behavior after faults or degraded state

## Runtime Order

### FACT

Per `Robot::run()` the control order is:

1. `tickHardware()`
2. `tickSchedule()`
3. `tickObstacleCleanup()`
4. `tickStateEstimation()`
5. `assembleOpContext()`
6. `tickSafetyGuards()`
7. `tickStateMachine()`
8. `tickDiag()` special-case
9. `tickButtonControl()`
10. `tickUserFeedback()`
11. `tickManualDrive()`
12. `tickSafetyStop()`
13. session tracking, LED update, telemetry

This means some guards run before op execution, while bumper/lift/motor-fault stop handling runs after manual drive and after the op tick.

## Motor Enable and Motion Suppression

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| Pi motion commands are applied by `setMotorPwm()` and forwarded over `AT+M` at 50 Hz | `SerialRobotDriver::run()` | `FACT` | High |
| `ctx.stopMotors()` and explicit `setMotorPwm(0,0,0)` are the main software stop primitives | `Robot.cpp`, ops | `FACT` | High |
| `ErrorOp`, `WaitRainOp`, `DockOp::begin`, `UndockOp::begin`, `NavToStartOp::begin`, and battery-critical branch all stop motors | active op code + `Robot::checkBattery()` | `FACT` | High |
| STM32 firmware zeros setpoints if `motorTimeout` expires after 3 s without new `AT+M` | `rm18.ino` sets `motorTimeout = millis()+3000` and later zeros all speed sets | `FACT` | High |
| There is a distinct hardware motor-enable line independently gated from PWM/brake logic | no direct evidence in Pi runtime or active firmware path | `UNKNOWN` | Critical unknown |

### Assessment

- `FACT`: Motion stop exists in many Pi-side logic branches.
- `FACT`: The only explicit comms-loss fallback proven in firmware is the STM32 `motorTimeout` at about 3 s.
- `INFERENCE`: If Pi control freezes but STM32 loop stays alive, motion can continue until that timeout expires.
- Residual risk: fail-late rather than instant fail-safe on pure communication loss.

## Emergency Stop

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| STM32 stop button uses interrupt on `pinStopButton` | `rm18.ino` `attachInterrupt(... stopButtonISR ...)` | `FACT` | Critical |
| Firmware comments state two normally-closed e-stop switches in series and HIGH means stop | `rm18.ino` comment in `setup()` | `FACT` | Critical |
| Stop button state is returned to Pi in `AT+M` field 7 | firmware `cmdMotor()`, Pi `parseMotorFrame()` | `FACT` | Critical |
| Pi short-press outside Idle/Charge triggers `emergencyStop()` immediately | `Robot::tickButtonControl()` | `FACT` | High |
| `emergencyStop()` resets drive controller, zeros motors, moves op state to `Idle` | `Robot::emergencyStop()` | `FACT` | High |
| Physical e-stop electrically cuts motor power independently of firmware and Pi | not proven from current repo evidence | `UNKNOWN` | Critical unknown |

### Assessment

- `FACT`: There is an application-level emergency stop path on Pi and an interrupt-fed stop signal on STM32.
- `UNKNOWN`: Whether the stop circuit is hard-wired to power or only software-observed.
- Residual risk: hard-stop guarantee is not proven from the repo alone.

## Communication Timeout

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| Pi detects missing MCU responses when `motorTxCount_ > 0 && motorRxCount_ == 0` | `SerialRobotDriver::run()` | `FACT` | Critical |
| On detection, Pi sets `mcuConnected_ = false` and resets odometry deltas | same | `FACT` | Critical |
| `StateEstimator::update()` aborts motion integration when `odo.mcuConnected == false` | `StateEstimator.cpp` | `FACT` | High |
| `DifferentialDriveController` suppresses normal closed-loop behavior when odometry is disconnected | search result shows explicit check in controller | `FACT` | High |
| `Robot` raises an immediate safety event solely because `mcuConnected` became false | no such direct guard found in `Robot.cpp` | `UNKNOWN` | Critical weakness |
| STM32 stops motors after 3 s of no fresh `AT+M` | `rm18.ino` `motorTimeout` logic | `FACT` | Critical |

### Assessment

- `FACT`: Pi notices communication loss.
- `FACT`: Pi does not, from the evidence reviewed, immediately force an error or stop state based only on `mcuConnected == false`.
- `FACT`: The hard fallback currently evidenced is the STM32-side 3 s motor command timeout.
- Residual risk: communication loss handling appears fail-safe eventually, but not immediately.

## Watchdog Reset Chain

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| STM32 local watchdog starts at 2 s and is reloaded each loop | `IWatchdog.begin(2000000)` and `IWatchdog.reload()` | `FACT` | Critical |
| Firmware exposes a developer trigger `AT+Y` that intentionally hangs and should trip the watchdog | `cmdTriggerWatchdog()` | `FACT` | Medium |
| Pi-side op watchdog checks `ctx.now_ms - op->startTime_ms > timeoutMs` | `Robot::monitorOpWatchdog()` | `FACT` | High |
| `DockOp` defines watchdog timeout from `dock_max_duration_ms` | `DockOp::watchdogTimeoutMs()` | `FACT` | High |
| Watchdog timeout escalates docking to `Error` and stops motors | `DockOp::onWatchdogTimeout()` | `FACT` | High |
| External Linux service watchdog or supervisor restart chain exists | not proven in repo | `UNKNOWN` | High unknown |

### Assessment

- `FACT`: There is a local MCU watchdog and a runtime op watchdog.
- `FACT`: Dock watchdog is explicit and scenario-tested.
- `UNKNOWN`: There is no proven external supervisor path for a wedged Pi process.

## GPS Invalid and Degraded Navigation

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| GPS resilience logic runs before op execution | `Robot::tickSafetyGuards()` -> `monitorGpsResilience()` | `FACT` | Critical |
| Short outage latches `gpsNoSignalLatched_` and calls `op->onGpsNoSignal(ctx)` | `Robot::monitorGpsResilience()` | `FACT` | High |
| Prolonged outage latches `gpsFixTimeoutLatched_` and calls `op->onGpsFixTimeout(ctx)` | same | `FACT` | High |
| Latches clear only after stable hysteresis recovery | same | `FACT` | Medium |
| `Mow` and `NavToStart` switch to `GpsWait` on no signal and to `Dock` on fix timeout | `MowOp.cpp`, `NavToStartOp.cpp` | `FACT` | High |
| `Dock` switches to `GpsWait` on no signal but to `Error` on GPS fix timeout | `DockOp.cpp` | `FACT` | High |
| `GpsWait` stops all motors and resumes previous op only after GPS returns | `GpsWaitFixOp.cpp` | `FACT` | High |
| While GPS is degraded but not yet lost, `LineTracker` only scales speed down instead of stopping | `LineTracker.cpp` | `FACT` | Medium |

### Assessment

- `FACT`: GPS loss handling is explicit and latched.
- `FACT`: Behavior differs by operation, especially during docking.
- `INFERENCE`: During Float or stale-fix periods, the system deliberately keeps moving at reduced speed rather than forcing a halt.
- Residual risk: degraded navigation is tolerated by design; that is safer than full speed, but not a full stop.

## Docking Mismatch and Charger-Contact Safety

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| Dock success is defined by `battery.chargerConnected` becoming true | `DockOp::run()`, `DockOp::onChargerConnected()` | `FACT` | Critical |
| `chargerConnected` is derived from charger voltage threshold, not a dedicated dock switch | Serial driver + config | `FACT` | Critical |
| `DockOp::begin()` errors immediately if route creation fails | `DockOp::begin()` | `FACT` | High |
| If dock path ends without charger contact, docking retries, then escalates to `Error` after retry budget | `DockOp::onNoFurtherWaypoints()` | `FACT` | High |
| During `Charge`, weak contact first triggers a slow creep retry, then `Dock`, then eventually `Error` on repeated failure | `ChargeOp.cpp` | `FACT` | High |
| `Undock` errors if charger never drops within timeout or if position progress times out | `UndockOp.cpp` | `FACT` | High |
| A separate physical dock-detect input confirms mechanical dock alignment | no evidence found | `UNKNOWN` | Critical unknown |

### Assessment

- `FACT`: Docking safety hinges on interpreted charge voltage, route availability, and timeout budgets.
- `INFERENCE`: False-positive or false-negative charger voltage could misclassify dock state.
- Residual risk: docking truth source is electrical charging behavior, not a dedicated contact sensor proven in repo.

## Battery Critical

| Path | Evidence | Classification | Risk |
| --- | --- | --- | --- |
| Battery guard runs before op tick every loop | `Robot::tickSafetyGuards()` -> `checkBattery()` | `FACT` | Critical |
| If charging, low and critical latches are cleared and no shutdown is triggered | `Robot::checkBattery()` | `FACT` | Medium |
| If below `battery_critical_v`, Pi logs event, zeros motors, turns buzzer on, notifies current op, drops power hold, and stops loop | `Robot::checkBattery()` | `FACT` | Critical |
| If below `battery_low_v`, current op is told to dock | same | `FACT` | High |
| Critical path depends on battery telemetry received from STM32 | telemetry path via `AT+S` and `AT+M` | `FACT` | Critical |

### Assessment

- `FACT`: Battery critical handling is one of the strongest fail-safe paths in the repo.
- `INFERENCE`: If MCU telemetry is stale or lost, Pi can no longer make fresh battery-based decisions and must rely on the MCU and existing state.

## Recovery Behavior

| Situation | Recovery path | Classification | Risk |
| --- | --- | --- | --- |
| Bumper during Mow/NavToStart/Dock | `EscapeReverse` then optional resume via `nextOp` | `FACT` | High |
| Escape outside perimeter | stop and `Dock` | `FACT` | High |
| Lift during escape | stop and `Error` | `FACT` | High |
| GPS kidnap during Mow/NavToStart/Dock/Escape | stop and `GpsWait` | `FACT` | High |
| Map changed during mission | usually stop and `Idle` or block resume | `FACT` | Medium |
| Rain away from dock | `WaitRain` then request `Dock` | `FACT` | Medium |
| Unexpected charger disconnect during `Charge` | retry contact, then `Dock`, then `Error` after repeated failure | `FACT` | High |

### Assessment

- `FACT`: Recovery paths are explicit and operation-specific.
- `INFERENCE`: Obstacle recovery is designed to resume work if the planner can recover a valid route.
- Residual risk: safe resume depends on map state and planner correctness, not purely on stop behavior.

## Confirmed Safeguards

- `FACT`: Many critical paths explicitly zero motor outputs.
- `FACT`: Battery critical path both stops motion and drops `keepPowerOn(false)`.
- `FACT`: Dock watchdog timeout escalates to `Error`.
- `FACT`: GPS no-signal and fix-timeout are latched with hysteresis.
- `FACT`: Escape and docking flows have explicit timeout or retry boundaries.
- `FACT`: Unhandled exceptions in `Robot::run()` zero motors and stop the loop.

## Weak Points

- `FACT`: Communication loss is detected, but no direct Pi safety state transition based only on `mcuConnected == false` was proven.
- `UNKNOWN`: Hard electrical motor-disable path is not proven from the repo.
- `UNKNOWN`: Physical e-stop power-cut topology is not proven.
- `FACT`: Dock truth depends on charger-voltage interpretation rather than a separately proven dock sensor.
- `FACT`: GPS degraded mode allows reduced-speed motion before the no-signal or timeout thresholds fire.

## Unknowns

- `UNKNOWN`: external Linux watchdog or service supervision
- `UNKNOWN`: whether STM32 emergency-stop input also hard-disables drive electronics outside the normal loop
- `UNKNOWN`: whether deployed hardware revisions add independent dock-contact or safety-relay signals

## High-Risk Files

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [core/op/DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp)
- [core/op/UndockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/UndockOp.cpp)
- [core/op/MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp)
- [core/op/NavToStartOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/NavToStartOp.cpp)
- [core/op/GpsWaitFixOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/GpsWaitFixOp.cpp)
- [core/navigation/LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)
- [hal/SerialRobotDriver/SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [alfred/firmware/rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino)

## Status

- Scope completed as mapping only
- No production code changed
- Risk classification is evidence-based and intentionally conservative
