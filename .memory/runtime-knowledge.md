# Runtime Knowledge

## Purpose

Short, evidence-based runtime notes for `sunray-core`.

## Service Shape

- `FACT`: `main.cpp` bootstraps the backend runtime.
- `FACT`: `Robot` is the central runtime object.
- `FACT`: `SerialRobotDriver` is the Alfred hardware path when not in simulation.
- `FACT`: Pi-side runtime publishes telemetry through WebSocket and optional MQTT.

## Startup Order

- `FACT`: config path resolution happens before any runtime object creation.
- `FACT`: `robot.init()` happens before GPS, WebSocket, and MQTT setup.
- `FACT`: GPS init failure is tolerated; runtime continues without GPS.
- `FACT`: WebSocket server starts before MQTT.
- `FACT`: MQTT start is optional and can degrade to disabled/no-op depending on config and build dependencies.
- `FACT`: long-running runtime starts at `robot.loop()`.

## Build / Deploy Snapshot

- `FACT`: backend build system is CMake.
- `FACT`: frontend build system is npm plus Vite/Svelte.
- `FACT`: installer script builds backend into `build_linux` by default and can generate `sunray-core.service`.
- `FACT`: Alfred manual test docs use `build_pi/sunray-core` and foreground execution.
- `FACT`: runtime config path is `/etc/sunray-core/config.json` unless CLI arg or `CONFIG_PATH` overrides it.
- `FACT`: STM32 firmware flashing uses `arduino-cli` plus OpenOCD through `scripts/flash_alfred.sh`.
- `FACT`: no active `platformio.ini` was found in the repo snapshot.
- `FACT`: repo now contains a read-only deploy verifier at `scripts/check_deploy_state.sh` that checks build artefacts, runtime files, `sunray-core.service`, and rollback-anchor visibility.
- `FACT`: `config.example.json` now mirrors the active Alfred runtime baseline for key UART, I2C, docking, planner, and path defaults instead of leaving them only in `Config.cpp`.

## Active Hardware Chain

- `FACT`: Pi opens `/dev/ttyS0` to talk to the STM32 over AT-style frames.
- `FACT`: Pi opens `/dev/i2c-1` for mux, port expanders, and IMU.
- `FACT`: STM32 firmware owns local PWM, brakes, ADC reads, odometry ISRs, and stop-button ISR.

## Telemetry Path

- `FACT`: `AT+M` carries odometry, charge voltage, bumper, lift, stop button, and motor fault.
- `FACT`: `AT+S` carries battery voltage, charge voltage/current, rain, battery temp, and fault-related bits.
- `FACT`: Pi derives `chargerConnected` from charge voltage threshold rather than a separate dock switch.
- `FACT`: External WebSocket and MQTT operator commands are now serialized into `Robot::run()` via a thread-safe external-command queue before any non-thread-safe robot state mutation occurs.
- `FACT`: `MqttClient` now keeps separate MQTT socket-connection and command-subscription state and retries the `{prefix}/cmd` subscription after reconnect or subscribe failure.
- `FACT`: `SerialRobotDriver` now debounces the threshold-derived charger-contact signal over consecutive samples before updating `battery_.chargerConnected`.
- `FACT`: Web telemetry now exposes the debounced dock/charge truth as `charger_connected`.
- `FACT`: Web telemetry now also exposes a compact runtime-health surface: `runtime_health`, `mcu_connected`, `mcu_comm_loss`, `gps_signal_lost`, `gps_fix_timeout`, `battery_low`, `battery_critical`, `recovery_active`, and `watchdog_event_active`.
- `FACT`: Frontend `sendCmd()` now raises a shared transient error notice when the WebSocket is not open or the send fails, and `BottomPanel` renders that notice across the main operator pages.
- `FACT`: Frontend `MissionWidget` now keeps the active mission card visible during normal recovery ops (`GpsWait`, `EscapeReverse`, `EscapeForward`, `WaitRain`) as long as telemetry still carries the active `mission_id`.
- `FACT`: `GpsWaitFixOp` now distinguishes dock-critical recovery from normal mission recovery: dock resumes require RTK Fix, while non-dock resume paths may still continue on RTK Float.
- `FACT`: `DockOp` now tracks retry-start pose and escalates after repeated non-progressive dock retries instead of exhausting the full retry budget on the same stalled contact approach.
- `FACT`: `Robot` now detects persistent commanded-motion stalls from commanded linear speed versus fused ground speed and dispatches `onStuck()`; `Mow` and `Dock` recover via `EscapeReverse`, while `Undock` escalates to `Error`.
- `FACT`: `Robot::monitorGpsResilience()` now allows bounded mowing continuation during short GPS outages via `mow_gps_coast_ms` when degraded fusion remains operational and GPS had been available earlier in the mission.
- `FACT`: Later dock retries can now vary approach geometry laterally through `dock_retry_lateral_offset_m`; if offset routing fails, the runtime falls back to the original docking retry path.
- `FACT`: Regression coverage now explicitly exercises repeated charger-contact flap damping, ChargeOp short-flap versus grace-expiry behavior, and degraded MQTT enabled start/stop lifecycle without a reachable broker.
- `UNKNOWN`: True injected UART transport-fault regression coverage still lacks a dedicated test seam in the current repo.

## GPS Loss Runtime Behavior

- `FACT`: `StateEstimator::update()` continues running its predict step every update cycle while odometry is available, even after GPS quality flags time out.
- `FACT`: Wheel odometry remains active as long as MCU odometry is available.
- `FACT`: IMU heading integration remains active through `updateImu()`.
- `FACT`: `fusionMode()` distinguishes `"EKF+GPS"`, `"EKF+IMU"`, and `"Odo"`.
- `FIELD`: Under oak canopy / house-edge shadowing, observed safe bridging behavior was reported as:
  - `10-30 s`: low drift, suitable for normal mowing corridors
  - `30-60 s`: acceptable only at controlled low speed
  - `>60 s`: uncertainty rises significantly
- `FACT`: The configured Alfred runtime baseline now uses `gps_no_signal_ms=15000`, `gps_recover_hysteresis_ms=3000`, and `ekf_gps_failover_ms=20000`.
- `FACT`: The configured Alfred runtime baseline now uses `mow_gps_coast_ms=20000`.
- `FACT`: The configured Alfred runtime baseline now uses `stuck_detect_timeout_ms=3000` and `stuck_detect_min_speed_ms=0.03`.
- `FACT`: Repeated unresolved stuck recovery now honors `stuck_recovery_max_attempts` and escalates to `Error` with `ERR_STUCK` / `stuck_recovery_exhausted`.
- `FACT`: The configured Alfred runtime baseline now uses `dock_retry_lateral_offset_m=0.10`.
- `FIELD`: Current mowing operation logic does not fully exploit this runtime capability and still degrades early into `GpsWait`.

## Runtime Statistics

- `FACT`: Runtime events are persisted individually through the history backend.
- `FACT`: Runtime events now persist both relative uptime (`ts_ms`) and wall-clock time (`wall_ts_ms`); the WebUI event stream should use wall-clock time for operator-facing chronology.
- `FACT`: History summary now also exposes grouped counters as `event_reason_counts`, `event_type_counts`, and `event_level_counts`.
- `FACT`: History summary now also exposes `last_event_wall_ts_ms`, so the `Verlauf` landing cards can show real date/time instead of formatting uptime as if it were epoch time.
- `FACT`: This grouped summary is suitable for field statistics such as repeated `lift_triggered`, `battery_critical`, `gps_signal_lost`, `mcu_comm_lost`, or `stuck_recovery_exhausted` patterns without re-reading the full event list.

## Watchdog Chain

- `FACT`: STM32 firmware starts an `IWatchdog` with a 2 s timeout and reloads it in `loop()`.
- `FACT`: Pi-side `SerialRobotDriver` treats missing UART responses as MCU disconnect.
- `FACT`: After at least one confirmed MCU-connected cycle, Pi-side `Robot` now treats a later `mcuConnected=false` edge as a local safety fault and requests the existing `Error` op with `ERR_MCU_COMMS`.
- `FACT`: Pi-side `Robot` enforces op-specific watchdog timeouts.
- `FACT`: `main.cpp` now blocks `SIGINT` and `SIGTERM` process-wide and handles them through a dedicated `sigwait()` thread that calls `Robot::stop()`; this avoids relying on fragile asynchronous `std::signal` handling inside the multithreaded runtime.
- `FACT`: generated `sunray-core.service` uses `Restart=on-failure`, not a `systemd` watchdog.
- `UNKNOWN`: external supervisor or process restarter beyond generated `systemd` behavior.

## Safety-Relevant Hardware Notes

- `FACT`: stop button is represented in Pi sensor state and influences runtime behavior.
- `FACT`: while a diagnostic motor action is active, a pressed stop button now aborts the diagnostic immediately, zeros motor PWM, and returns control to the normal button-handling path in the same control-loop cycle.
- `FACT`: battery critical handling can stop motors and request power-off through `keepPowerOn(false)`.
- `FACT`: motor fault may come from STM32 overload logic and over-voltage reporting.
- `FACT`: The top system-status LED now blinks red when no MCU communication is available in the current control cycle, instead of remaining green.
- `FACT`: On runtime loop shutdown, the system-status LED is forced to solid red while the Alfred power-off grace path is armed.

## Known Unknowns

- deployed service manager details on productive target beyond generated installer unit
- exact power-cut circuitry behind emergency stop and shutdown
- exact dock-contact hardware, if any, beyond charge-voltage inference
- exact field rollback path for failed STM32 firmware flash
- exact productive state of `sunray-core.service` and `sunray` remains machine-specific and must be checked locally with `scripts/check_deploy_state.sh`

## Future Constraints

- `FACT`: Future operator-facing features have clear landing zones only in the existing backend surfaces: `Robot`, `WebSocketServer`, `MqttClient`, diagnostics, and history/event storage.
- `FACT`: Current repo evidence does not justify parallel control paths outside those backend surfaces.
- `FACT`: Future remote-command features must inherit the same safety and determinism constraints as local UI commands.
- `FACT`: Pi-side OTA is now functionally proven for `sunray-core` userspace: check, update trigger, version write, service restart, and reconnect flow.
- `FACT`: The STM32 SWD probe now distinguishes a missing `openocd` tool from a real SWD probe failure; Alfred's installer also installs `openocd` as part of the standard hardware-side dependency set.
- `FACT`: Alfred's STM32 SWD probe now targets Raspberry Pi 4 via OpenOCD `bcm2835gpio`; the older `sysfsgpio` backend was removed because it failed before the real probe on current Pi OS releases.
- `FACT`: Field validation on Alfred now proves the SWD probe end-to-end: OpenOCD detects `SWD DPIDR 0x1ba01477`, identifies `stm32f1x.cpu` as Cortex-M3, completes target examination, and halts the MCU successfully.
- `FACT`: `scripts/flash_alfred.sh probe` now ends with `reset run`, so a successful SWD probe should no longer leave the STM32 halted and invisible to the Pi-side UART runtime.
- `UNKNOWN`: STM32 firmware OTA, dedicated dock-contact hardware, and hard electrical emergency-stop proof remain blocked by missing evidence.
