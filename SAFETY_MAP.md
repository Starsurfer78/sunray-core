# Safety Map

## Immediate Stop Paths

| Trigger | Current Path | Expected Effect | Evidence |
| --- | --- | --- | --- |
| Bumper hit | `Robot::tickSafetyStop()` then `onObstacle()` | Motors zeroed immediately, recovery op requested | `core/Robot.cpp`, scenario tests |
| Lift sensor | `Robot::tickSafetyStop()` then `onLiftTriggered()` | Motors zeroed, transition toward `Error` | `core/Robot.cpp`, tests |
| Motor fault | `Robot::tickSafetyStop()` then `onMotorError()` | Motors zeroed, transition toward `Error` | `core/Robot.cpp`, tests |
| Unhandled runtime exception | `Robot::run()` catch blocks | Zero motor command, loop stop | `core/Robot.cpp` |
| Critical battery | `checkBattery()` low-critical branch | `keepPowerOn(false)`, stop running, `Error` | tests and runtime |

## Controlled Stop Paths

| Trigger | Path | Result |
| --- | --- | --- |
| Battery low | `onBatteryLowShouldDock()` | Transition to `Dock` |
| GPS prolonged degradation | GPS guard to `GpsWait` or `Dock` depending on condition | Mission pauses or returns home |
| Rain | active op to `WaitRain` | Controlled mission interruption |
| Perimeter violation | op-specific `onPerimeterViolated()` | Usually stop and dock, or error if docking cannot be resumed safely |
| Operator emergency stop | `emergencyStop()` | Immediate motor stop, return to `Idle` |

## Recovery Paths

| Situation | Recovery State | Notes |
| --- | --- | --- |
| Temporary obstacle | `EscapeReverse` or `EscapeForward` | Returns to previous op when safe |
| GPS transient loss | `GpsWait` | Resume target logic matters |
| Dock contact weak | retry logic in docking/charge flows | Must avoid endless retries |
| Map changed mid-mission | block resume and move to safer state | Prevents blind continuation |

## Critical Questions

- Is there an independent hardware watchdog outside Pi runtime?
- Does STM32 enforce a stop if serial commands stall?
- Is there a physical hard e-stop or relay gate not modeled here?
- What is the authoritative charger contact threshold on real Alfred hardware?
- Which dock failures should remain recoverable versus latched as `Error`?

## Confirmed Safeguards

- Motor zeroing is called on several fault paths
- Dock watchdog exists and is covered by tests
- Battery critical shutdown path is explicit
- Perimeter and map-change safety are modeled before normal state progression
- Error state and safety transitions have dedicated scenario coverage

## Suspected Weak Points

- Hardware ownership split between Pi and STM32 is not fully documented
- Some safety assumptions still rely on inferred MCU behavior
- Field deployment order and service supervision are not yet documented as evidence-backed facts

## Safety Verification Checklist

- Confirm `Error` always results in zero motion commands
- Confirm watchdog timeout is active even when op start time begins at zero
- Confirm battery critical path cuts power hold signal as intended
- Confirm obstacle recovery does not resume into an invalid route
- Confirm docking from outside perimeter is handled deterministically
- Confirm GPS failover and recovery latches behave without flapping
- Confirm auth on remote control surfaces matches deployment expectations
