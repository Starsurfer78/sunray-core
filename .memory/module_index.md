# Module Index — sunray-core

Last updated: 2026-03-22 (full repo scan)

---

## Implemented Modules

### platform/Serial.h + Serial.cpp

**Status:** ✅ Complete
**Purpose:** POSIX termios serial port wrapper — replaces LinuxSerial.cpp + Arduino shims
**Public API:** `read()`, `write()`, `writeStr()`, `available()`, `flush()`, `isOpen()`, `port()`
**Notes:** Raw 8N1, non-blocking (VMIN=0/VTIME=0). All c_iflag/c_lflag/c_oflag zeroed. Constructor throws on failure. Move-only. Maps baud rates 50…115200 to B-constants.

### platform/I2C.h + I2C.cpp

**Status:** ✅ Complete
**Purpose:** Linux i2cdev bus wrapper — replaces Arduino Wire.h
**Public API:** `write(addr, buf, len)`, `read(addr, buf, len)`, `writeRead(addr, tx, txLen, rx, rxLen)`, `isOpen()`, `busPath()`
**Notes:** `writeRead()` uses I2C_RDWR ioctl for atomic Repeated-START. Address cached to avoid redundant ioctls. Alfred device map: 0x20–0x22 PCA9555, 0x50 EEPROM, 0x68 MCP3421, 0x70 TCA9548A.

### platform/PortExpander.h + PortExpander.cpp

**Status:** ✅ Complete
**Purpose:** PCA9555 16-bit I/O port expander driver (LEDs, Buzzer, Fan, IMU power)
**Public API:** `setPin(port, pin, level)`, `getPin(port, pin)`, `setOutputPort(port, val)`, `setConfigPort(port, mask)`, `getInputPort(port, val&)`, `address()`
**Notes:** No internal state cache (read-modify-write per call). Alfred: EX1=0x21, EX2=0x20 (Buzzer), EX3=0x22 (LEDs).

### hal/HardwareInterface.h

**Status:** ✅ Complete (154 lines)
**Purpose:** Abstract base class — single boundary between Core and hardware
**Data structs:** `OdometryData` (leftTicks, rightTicks, mowTicks, mcuConnected), `SensorData` (bumperLeft/Right, lift, rain, stopButton, motorFault, nearObstacle), `BatteryData` (voltage, chargeVoltage, chargeCurrent, batteryTemp, chargerConnected), `LedId` (LED_1/2/3), `LedState` (OFF/GREEN/RED)
**Methods:** `init()`, `run()`, `setMotorPwm()`, `resetMotorFault()`, `readOdometry()`, `readSensors()`, `readBattery()`, `setBuzzer()`, `setLed()`, `keepPowerOn()`, `getCpuTemperature()`, `getRobotId()`, `getMcuFirmwareName()`, `getMcuFirmwareVersion()`

### hal/SerialRobotDriver/SerialRobotDriver.h + .cpp

**Status:** ✅ Complete
**Purpose:** Alfred (STM32) driver — flat implementation of HardwareInterface
**Depends on:** platform/Serial, platform/I2C, platform/PortExpander, core/Config
**Protocol:** AT+M@50Hz, AT+S@2Hz, AT+V once. CRC verify on RX (XOR), CRC append on TX.
**Bugs fixed:** BUG-05 (long cast for tick overflow), BUG-07 (PWM/encoder swap in send+parse), BUG-08 (no Pi-side mow clamp), IMP-01 (ovCheck in summaryFrame field 12)
**Notes:** Fan via CPU temp (>65°C on, <60°C off). WiFi LED via wpa_cli (7s interval). keepPowerOn(false) → 5s grace → shutdown now. Battery fallback 28V when MCU disconnected.

### hal/SimulationDriver/SimulationDriver.h + .cpp

**Status:** ✅ Complete
**Purpose:** Software-only HardwareInterface — no serial/I2C/hardware required
**Fault injection:** `setBumperLeft/Right()`, `setLift()`, `setGpsQuality()`, `addObstacle(Polygon)`, `clearObstacles()`, `setPose()`
**Kinematics:** Differential drive unicycle model. PWM→wheel speed→v/ω→dead-reckoning pose. Ticks from arc length.
**Thread safety:** `mutex_` guards all shared state.
**Notes:** `--sim` flag in main.cpp selects this driver. Polygon obstacle hit → bumperLeft+Right+nearObstacle. Point-in-polygon via ray casting.

### core/Config.h + Config.cpp

**Status:** ✅ Complete
**Purpose:** JSON-based runtime configuration, replaces config.h
**Key methods:** `get<T>(key, fallback)`, `set<T>(key, value)`, `save()`, `reload()`, `dump()`, `path()`
**Defaults:** 28 keys (see Config Keys Reference in docs/ARCHITECTURE.md)
**Notes:** Silent fallback on corrupt JSON. Forward-compatible (unknown keys accepted). Load order: defaults → file overrides per-key.

### core/Logger.h

**Status:** ✅ Complete (header-only)
**Purpose:** Minimal logging interface
**Types:** `LogLevel` (DEBUG/INFO/WARN/ERROR), `Logger` (abstract), `StdoutLogger`, `NullLogger`
**Notes:** NullLogger for tests. No iostream in interface (uses printf). `StdoutLogger(minLevel)` constructor.

### core/Robot.h + Robot.cpp

**Status:** ✅ Complete
**Purpose:** Main robot class — DI constructor, 50 Hz control loop, state machine orchestrator
**Key methods:** `init()`, `loop()`, `run()`, `stop()`, `startMowing()`, `startDocking()`, `emergencyStop()`, `loadMap()`, `setPose()`, `setWebSocketServer()`
**Loop sequence:** hw->run() → readSensors → stateEst.update → OpContext → checkBattery → opMgr.tick → safety stop → updateLEDs → pushTelemetry → ++loops
**Notes:** `setWebSocketServer(ws*)` optional setter — not in constructor, keeps test mock clean. `buildTelemetry()` private helper assembles TelemetryData from current state.

### core/RobotConstants.h

**Status:** ✅ Complete (A.7)
**Purpose:** Compile-time architectural constants — NOT runtime tuning params
**Constants:** `OVERALL_MOTION_TIMEOUT_MS` (10s), `MOTION_LP_DECAY_MS` (2s), `GPS_NO_MOTION_THRESHOLD_M` (0.05m), `OBSTACLE_ROTATION_TIMEOUT_MS` (15s), `OBSTACLE_ROTATION_SPEED_DEG_S` (10°/s)
**Notes:** All `inline constexpr`. Tuning params remain in Config/config.json.

### core/op/ (Op State Machine)

**Status:** ✅ Complete (A.4)
**Files:** Op.h, Op.cpp, IdleOp.cpp, MowOp.cpp, DockOp.cpp, ChargeOp.cpp, EscapeReverseOp.cpp (incl. EscapeForwardOp), GpsWaitFixOp.cpp, ErrorOp.cpp
**Op instances:** Idle, Mow, Dock, Charge, EscapeReverse, EscapeForward, GpsWait, Error — owned by OpManager
**Op names (frozen):** `"Idle"`, `"Mow"`, `"Dock"`, `"Charge"`, `"EscapeReverse"`, `"GpsWait"`, `"Error"` — must match Mission Service
**Priority levels:** PRIO_LOW=10, PRIO_NORMAL=50, PRIO_HIGH=80, PRIO_CRITICAL=100
**Key transitions:**
- IdleOp: charger connected >2s → ChargeOp
- MowOp: obstacle → EscapeReverse(returnBack), GPS lost → GpsWait(returnBack), GPS timeout → ErrorOp, rain/battery/timetable/noWaypoints → DockOp
- DockOp: charger connected → ChargeOp, routing fails ×5 → ErrorOp
- ChargeOp: disconnected after 3s → IdleOp, timetable start + battery OK → MowOp
- GpsWaitFixOp: GPS acquired → nextOp/IdleOp, >2 min → ErrorOp
- ErrorOp: no autonomous exit — operator "Idle" only

### core/navigation/StateEstimator.h + .cpp

**Status:** ✅ Complete (A.5)
**Purpose:** Odometry dead-reckoning + GPS fusion stub
**Phase 1:** odometry-only. `updateGps()` stub for Phase 2.
**Config:** `ticks_per_meter` (def 120), `wheel_base_m` (def 0.285)
**Notes:** Sanity guard: >0.5 m/frame → skip. Safety clamp: >±10 km → reset to origin. LP filter α=0.1 for groundSpeed.

### core/navigation/Map.h + .cpp

**Status:** ✅ Complete (A.5)
**Purpose:** Waypoint/polygon management, perimeter enforcement, obstacle tracking
**Key types:** `Point`, `PolygonPoints`, `WayType` (PERIMETER/EXCLUSION/DOCK/MOW/FREE)
**Key methods:** `load()`, `save()`, `startMowing()`, `startDocking()`, `nextPoint()`, `isInsideAllowedArea()`, `addObstacle()`, `getDockingPos()`
**Public state:** `targetPoint`, `lastTargetPoint`, `trackReverse`, `trackSlow`, `wayMode`, `percentCompleted`
**Notes:** Phase 1 uses simplified A* pathfinding. JSON map format set by Mission Service.

### core/navigation/LineTracker.h + .cpp

**Status:** ✅ Complete (A.5)
**Purpose:** Stanley path-tracking controller
**Formula:** `angular = p*headingError + atan2(k*lateralError, 0.001+|speed|)`
**Constants:** TARGET_REACHED_TOLERANCE=0.2m, KIDNAP_TOLERANCE=3.0m, ROTATE_SPEED_RADPS=29°/s
**Events fired:** onTargetReached, onNoFurtherWaypoints, onKidnapped(true/false)
**Config:** `stanley_k/p_normal`, `stanley_k/p_slow`, `motor_set_speed_ms`, `dock_linear_speed_ms`

### core/WebSocketServer.h + .cpp

**Status:** ✅ Complete (A.6)
**Purpose:** Crow-based WebSocket server — 10 Hz telemetry push + command reception
**Endpoint:** `/ws/telemetry` — port from config `ws_port` (default 8765)
**Push:** 100 ms interval; `{"type":"ping"}` keepalive if no new data
**Telemetry format:** frozen, identical to `sunray/mission_api.cpp:254-274` — 15 keys: type/op/x/y/heading/battery_v/charge_v/gps_sol/gps_text/gps_lat/gps_lon/bumper_l/bumper_r/motor_err/uptime_s
**Commands:** `{"cmd":"start|stop|dock|charge|setpos"}` — routed to Robot methods
**Threading:** Crow I/O in own thread pool; push loop in serverThread_; telemetry shared via mutex
**Pimpl:** Crow headers only in WebSocketServer.cpp — not in .h
**Robot integration:** `Robot::setWebSocketServer(ws*)` setter; pushed in Robot::run() step 9

### config.example.json

**Status:** ✅ Complete (A.7)
**Purpose:** All 28 Config defaults documented — copy to /etc/sunray/config.json for deployment
**Notes:** Contains `_comment` key (ignored by nlohmann/json parser).

### docs/ARCHITECTURE.md

**Status:** ✅ Complete (2026-03-22 full scan)
**Purpose:** Complete technical documentation — all modules, HardwareInterface, WebSocket API, Config keys, Op State Machine

---

## Tests

| Test file | Module | Tests |
|-----------|--------|-------|
| `test_config.cpp` | Config | 8 tests: missing file, overrides, unknown key, type mismatch, save+reload, reload discards, invalid JSON, dump |
| `test_serial.cpp` | Serial | 4 tests: nonexistent port throws, error message, move semantics, non-copyable |
| `test_i2c.cpp` | I2C | 4 sw-tests + 3 hardware tests (tagged [.][hardware], skipped by default) |
| `test_port_expander.cpp` | PortExpander | I2C mock tests |
| `test_serial_driver.cpp` | SerialRobotDriver | Mock-based AT protocol tests |
| `test_robot.cpp` | Robot | 21 tests: null-ptr guards, init, run, state transitions, battery, loop exit |
| `test_simulation_driver.cpp` | SimulationDriver | 22 tests: init, kinematics, odometry, sensors, battery, obstacles, gps, thread |
| `test_websocket_server.cpp` | WebSocketServer | 8 tests: JSON format, all 15 keys, Op names, API surface |

---

## Pending Modules

### Phase 2 (after A.8 Alfred Build-Test)

- `hal/PicoRobotDriver/` — RP2040 Pico direct PWM/Hall driver
- GPS fusion in StateEstimator (complementary filter, fusionPI logic)
- Real gps_lat/gps_lon in telemetry
- A* pathfinding in Map (simplified placeholder currently)

### webui/src/views/Settings.vue

**Status:** ✅ Complete (2026-03-22)
**Purpose:** Einstellungen-Screen — 3 Sektionen: System, Roboter-Konfiguration, Diagnose
**Sektion 1 (System):** Mission Service Version (/api/version), Socket-Status aus useTelemetry, Uptime (h:mm), GPS-Text, Map-Pfad mit Link zu /map, Links /docs + /redoc
**Sektion 2 (Config):** 6 Felder (strip_width_default, speed_default, battery_low_v, battery_critical_v, gps_no_motion_threshold_m, rain_delay_min). GET /api/config beim Mount. PUT /api/config mit nur geänderten Keys. Speichern-Button nur aktiv wenn dirty. Toast-Feedback.
**Sektion 3 (Diagnose):** Motor-Tests (3 Buttons → POST /api/sim/motor), IMU-Kalibrierung (POST /api/sim/imu_calib, States: idle/running/done), Log-Stream aus useTelemetry logs (type:"log" WS-Nachrichten, max 200, neueste oben, Clear-Button)
**Design:** Dark Blue — exakt #-Werte aus dashboard_reference.html, scoped CSS mit !important

### webui/src/composables/useTelemetry.ts

**Status:** ✅ Updated (2026-03-22) — Log-Stream hinzugefügt
**New exports:** `logs: readonly string[]` (max 200, neueste zuerst), `clearLogs()`
**Log source:** WebSocket-Nachrichten mit `type:"log"` und `text` Feld

### C — Mission Service WebUI

- Zonen-Einstellungen Modal
- Pfad-Vorschau
- GeoJSON-Import
- Coverage-Overlay
- MissionRunner Waypoint-Sending

---

## Build Status

- CMakeLists.txt: ✅ Complete (root + all subdirs)
- FetchContent: nlohmann/json ✅, Catch2 ✅, Asio standalone ✅, Crow v1.2.0 ✅
- Target platform: Linux / Raspberry Pi 4B
- A.8 (Alfred Build-Test auf Pi): ⏸ zurückgestellt — erfordert Hardware-Zugang
