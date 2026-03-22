# sunray-core — Architecture Reference

Generated from full repo scan (2026-03-22). All claims sourced directly from code.

---

## Table of Contents

1. [Design Principles](#1-design-principles)
2. [Repository Structure](#2-repository-structure)
3. [Dependency Graph](#3-dependency-graph)
4. [Platform Layer (`platform/`)](#4-platform-layer)
5. [Hardware Abstraction Layer (`hal/`)](#5-hardware-abstraction-layer)
6. [Core — Config & Logger](#6-core--config--logger)
7. [Core — Robot](#7-core--robot)
8. [Core — Op State Machine](#8-core--op-state-machine)
9. [Core — Navigation](#9-core--navigation)
10. [Core — WebSocketServer](#10-core--websocketserver)
11. [Core — RobotConstants](#11-core--robotconstants)
12. [WebSocket API](#12-websocket-api)
13. [Config Keys Reference](#13-config-keys-reference)
14. [Build System](#14-build-system)
15. [Entry Point (`main.cpp`)](#15-entry-point-maincpp)

---

## 1. Design Principles

| Principle | Implementation |
|-----------|----------------|
| No Arduino includes anywhere | `#include <Arduino.h>` absent from all files |
| No global variables | All state owned by classes, passed via DI |
| Hardware behind `HardwareInterface` | Core never includes a driver header |
| No Singleton | `Config`, `Logger`, drivers passed as `shared_ptr`/`unique_ptr` |
| `config.json` replaces `config.h` | No recompile needed for parameter changes |
| Every module testable in isolation | MockHardware in tests, NullLogger, no hardware required |
| Pimpl for heavy dependencies | Crow headers confined to `WebSocketServer.cpp` |

---

## 2. Repository Structure

```
sunray-core/
  ├── core/                  platform-independent logic
  │   ├── Config.h/.cpp      JSON runtime config (nlohmann/json)
  │   ├── Logger.h           Logging interface + StdoutLogger + NullLogger
  │   ├── Robot.h/.cpp       Main class — control loop, DI root
  │   ├── RobotConstants.h   Compile-time architectural constants
  │   ├── WebSocketServer.h/.cpp   Crow WebSocket server
  │   ├── op/                Op state machine
  │   │   ├── Op.h           Base class, OpContext, OpManager, all Op declarations
  │   │   ├── Op.cpp         OpManager + base method bodies
  │   │   ├── IdleOp.cpp
  │   │   ├── MowOp.cpp
  │   │   ├── DockOp.cpp
  │   │   ├── ChargeOp.cpp
  │   │   ├── EscapeReverseOp.cpp  (also EscapeForwardOp)
  │   │   ├── GpsWaitFixOp.cpp
  │   │   └── ErrorOp.cpp
  │   └── navigation/
  │       ├── StateEstimator.h/.cpp   Odometry dead-reckoning + GPS stub
  │       ├── Map.h/.cpp              Waypoint/polygon management
  │       └── LineTracker.h/.cpp      Stanley path-tracking controller
  ├── hal/
  │   ├── HardwareInterface.h         Abstract base class
  │   ├── SerialRobotDriver/          Alfred (STM32) driver
  │   └── SimulationDriver/           Software-only driver for testing
  ├── platform/              Linux-specific POSIX wrappers
  │   ├── Serial.h/.cpp      termios serial port
  │   ├── I2C.h/.cpp         Linux i2cdev bus wrapper
  │   └── PortExpander.h/.cpp PCA9555 16-bit I/O expander
  ├── tests/                 Catch2 unit tests (no hardware required)
  ├── main.cpp               Entry point — DI wiring
  ├── config.example.json    All config keys with defaults (copy → /etc/sunray/)
  └── CMakeLists.txt
```

---

## 3. Dependency Graph

```
main.cpp
  └── Robot  (owns)
        ├── HardwareInterface  (unique_ptr — runtime polymorphism)
        │     ├── SerialRobotDriver   (Alfred/Pi)
        │     │     ├── platform::Serial
        │     │     ├── platform::I2C
        │     │     └── platform::PortExpander (×3)
        │     └── SimulationDriver    (--sim mode)
        ├── Config             (shared_ptr)
        ├── Logger             (shared_ptr)
        ├── OpManager          (value member — owns all Op instances)
        │     └── Op subclasses (×8 instances)
        ├── nav::StateEstimator (value member)
        ├── nav::Map            (value member)
        ├── nav::LineTracker    (value member)
        └── WebSocketServer*   (raw ptr, not owned — set via setter)

namespace dependencies (no cycles):
  platform  ←  hal  ←  core  ←  main
```

---

## 4. Platform Layer

### `platform::Serial`

**File:** `platform/Serial.h/.cpp`
**Purpose:** POSIX termios serial port wrapper, replaces Arduino `HardwareSerial` and `LinuxSerial.cpp`.

**Constructor:**
```cpp
Serial(const std::string& port, unsigned int baud)
// throws std::runtime_error if port cannot be opened
```

**Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `read` | `int read(uint8_t* buf, size_t maxLen)` | Non-blocking read. Returns bytes read (0 = no data, -1 = error) |
| `write` | `bool write(const uint8_t* buf, size_t len)` | Write bytes, retries on EAGAIN |
| `writeStr` | `bool writeStr(const char* str)` | Write null-terminated string |
| `available` | `int available()` | Bytes in kernel receive buffer (FIONREAD ioctl) |
| `flush` | `void flush()` | Discard all unread input and unsent output (TCIOFLUSH) |
| `isOpen` | `bool isOpen() const` | True if fd ≥ 0 |
| `port` | `const std::string& port() const` | Device path from constructor |

**Configuration:** Raw 8N1, non-blocking (VMIN=0, VTIME=0). Saves and restores original termios on destruction.

**Fixes vs LinuxSerial.cpp:** All `c_iflag/c_lflag/c_oflag` explicitly zeroed; `tcflush()` before and after `tcsetattr`; constructor throws instead of silently failing.

---

### `platform::I2C`

**File:** `platform/I2C.h/.cpp`
**Purpose:** Linux i2cdev bus wrapper, replaces Arduino `Wire.h`.

**Constructor:**
```cpp
explicit I2C(const std::string& bus)
// throws std::runtime_error if bus cannot be opened
```

**Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `write` | `bool write(uint8_t addr, const uint8_t* buf, size_t len)` | Write to 7-bit address |
| `read` | `bool read(uint8_t addr, uint8_t* buf, size_t len)` | Read from 7-bit address |
| `writeRead` | `bool writeRead(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx, size_t rxLen)` | Atomic write+read (I2C_RDWR ioctl, Repeated START) |
| `isOpen` | `bool isOpen() const` | True if fd open |
| `busPath` | `const std::string& busPath() const` | Bus device path |

**Alfred I2C Device Map (bus `/dev/i2c-1`):**

| Address | Device | Purpose |
|---------|--------|---------|
| `0x20` | PCA9555 EX2 | Buzzer (IO1.1), SWD-CS6 (IO0.6) |
| `0x21` | PCA9555 EX1 | IMU power (IO1.6), Fan (IO1.7), ADC mux (IO1.0-3) |
| `0x22` | PCA9555 EX3 | LED1 green/red (IO0.0/1), LED2 (IO0.2/3), LED3 (IO0.4/5) |
| `0x50` | BL24C256A EEPROM | Persistent storage |
| `0x68` | MCP3421 ADC | Battery voltage measurement |
| `0x70` | TCA9548A mux | Selects IMU/EEPROM/ADC sub-bus |

---

### `platform::PortExpander`

**File:** `platform/PortExpander.h/.cpp`
**Purpose:** PCA9555 16-bit I/O port expander driver.

**Constructor:**
```cpp
PortExpander(I2C& bus, uint8_t addr)
// Does NOT talk to hardware until first setPin/getPin call
```

**Methods:**

| Method | Signature | Description |
|--------|-----------|-------------|
| `setPin` | `bool setPin(uint8_t port, uint8_t pin, bool level)` | Configure as output, set level. Read-modify-write. |
| `getPin` | `bool getPin(uint8_t port, uint8_t pin)` | Configure as input, return level |
| `setOutputPort` | `bool setOutputPort(uint8_t port, uint8_t value)` | Write full 8-bit output latch (all 8 pins at once) |
| `setConfigPort` | `bool setConfigPort(uint8_t port, uint8_t dirMask)` | Set direction register (0=output, 1=input) |
| `getInputPort` | `bool getInputPort(uint8_t port, uint8_t& value)` | Read full 8-bit input register |
| `address` | `uint8_t address() const` | I2C address |

**PCA9555 Register Map:**

| Register | Address | Description |
|----------|---------|-------------|
| Input Port 0/1 | `0x00/0x01` | Read-only, actual pin state |
| Output Port 0/1 | `0x02/0x03` | Output latch (driven value) |
| Config Port 0/1 | `0x06/0x07` | Direction (0=output, 1=input; reset=0xFF) |

---

## 5. Hardware Abstraction Layer

### `HardwareInterface` (abstract)

**File:** `hal/HardwareInterface.h`

#### Data Structures

**`OdometryData`**

| Field | Type | Description |
|-------|------|-------------|
| `leftTicks` | `int` | Left wheel encoder delta since last `readOdometry()` |
| `rightTicks` | `int` | Right wheel encoder delta |
| `mowTicks` | `int` | Mow motor encoder delta |
| `mcuConnected` | `bool` | False = no valid data this cycle (ticks = 0) |

**`SensorData`**

| Field | Type | Description |
|-------|------|-------------|
| `bumperLeft` | `bool` | Left bumper contact |
| `bumperRight` | `bool` | Right bumper contact |
| `lift` | `bool` | Lift sensor (robot lifted off ground) |
| `rain` | `bool` | Rain sensor |
| `stopButton` | `bool` | Physical stop button |
| `motorFault` | `bool` | Motor fault (overload or IC fault) |
| `nearObstacle` | `bool` | Sonar/ToF proximity — always `false` on Alfred |

**`BatteryData`**

| Field | Type | Description |
|-------|------|-------------|
| `voltage` | `float` | Battery voltage (V) |
| `chargeVoltage` | `float` | Charger output voltage (V) |
| `chargeCurrent` | `float` | Charging current (A), 0 when not charging |
| `batteryTemp` | `float` | Battery temperature (°C), -9999 if unavailable |
| `chargerConnected` | `bool` | True when `chargeVoltage > threshold` |

**`LedId`** enum: `LED_1` (bottom, WiFi), `LED_2` (top, status), `LED_3` (middle, GPS)
**`LedState`** enum: `OFF`, `GREEN`, `RED`

#### Interface Methods

| Method | Description |
|--------|-------------|
| `bool init()` | Open hardware, configure. Returns false on failure — Core aborts. |
| `void run()` | Periodic tick — call every control loop iteration (non-blocking) |
| `void setMotorPwm(int left, int right, int mow)` | PWM range: -255…+255 |
| `void resetMotorFault()` | Clear latched motor fault |
| `OdometryData readOdometry()` | Encoder deltas since last call |
| `SensorData readSensors()` | Snapshot of all sensor states |
| `BatteryData readBattery()` | Power/charging snapshot |
| `void setBuzzer(bool on)` | Activate/deactivate buzzer |
| `void setLed(LedId, LedState)` | Set panel LED |
| `void keepPowerOn(bool)` | false = trigger platform shutdown sequence |
| `float getCpuTemperature()` | CPU temp (°C), -9999 if unavailable |
| `std::string getRobotId()` | Unique identifier (eth0 MAC on Alfred) |
| `std::string getMcuFirmwareName()` | AT+V firmware name |
| `std::string getMcuFirmwareVersion()` | AT+V firmware version |

---

### `SerialRobotDriver`

**File:** `hal/SerialRobotDriver/SerialRobotDriver.h/.cpp`
**Implements:** `HardwareInterface`
**Purpose:** Alfred (STM32) driver — communicates via UART AT-frames.

**AT Protocol:**

| Frame | Rate | Direction | Content |
|-------|------|-----------|---------|
| `AT+M` | 50 Hz | Pi → STM32 → Pi | Motor PWM command + encoder/sensor response |
| `AT+S` | 2 Hz | Pi → STM32 → Pi | Summary request — battery, bumpers, rain, currents |
| `AT+V` | once | Pi → STM32 → Pi | Firmware name/version handshake |

**Frame format:** CSV fields + `*XX` CRC suffix (XOR over all bytes before `*`).

**Bug fixes applied (from firmware analysis):**
- `BUG-05`: Unsigned tick overflow — delta computed via `long` cast
- `BUG-07`: Left/right PWM and encoder swap — compensated internally (Alfred PCB cross-wiring)
- `BUG-08`: Pi-side mow motor clamp removed — STM32 ramp handles it

**Internal behaviors:**
- Fan: on when CPU temp > 65°C, off when < 60°C, checked every ~60 s
- WiFi LED: `wpa_cli status` polled every 7 s → LED_1
- Battery fallback: if MCU disconnected, voltage returns 28 V (Pi standalone safe)
- Shutdown: `keepPowerOn(false)` → 5 s grace → fan off → `shutdown now`

---

### `SimulationDriver`

**File:** `hal/SimulationDriver/SimulationDriver.h/.cpp`
**Implements:** `HardwareInterface`
**Purpose:** Software-only driver — no hardware required.

**Kinematic model:** Differential-drive unicycle. PWM → wheel speed (m/s) → dead-reckoning pose (x, y, heading). Ticks accumulated from arc length.

**Additional methods (not in HardwareInterface):**

| Method | Description |
|--------|-------------|
| `setBumperLeft(bool)` | Inject left bumper contact |
| `setBumperRight(bool)` | Inject right bumper contact |
| `setLift(bool)` | Inject lift sensor |
| `setGpsQuality(GpsQuality)` | `FIX` / `FLOAT` / `NO_FIX` |
| `addObstacle(Polygon)` | Polygon — robot entering it triggers bumper |
| `clearObstacles()` | Remove all polygons |
| `getPose()` | Current `SimPose` {x, y, heading} |
| `setPose(SimPose)` | Override pose directly |
| `getGpsQuality()` | Current GPS quality |
| `isBuzzerOn()` | Buzzer state |

**Thread safety:** All shared state guarded by `mutex_`.

**Activation:** `main.cpp --sim` flag selects `SimulationDriver` instead of `SerialRobotDriver`.

---

## 6. Core — Config & Logger

### `Config`

**File:** `core/Config.h/.cpp`
**Purpose:** JSON runtime configuration. Replaces Arduino `config.h` macros.

**Constructor:**
```cpp
explicit Config(std::filesystem::path path)
// Loads file; falls back to built-in defaults silently if file absent/corrupt
```

**Methods:**

| Method | Description |
|--------|-------------|
| `T get<T>(key, fallback)` | Read value; returns fallback on missing key or type error |
| `void set<T>(key, value)` | Write to in-memory document (not to disk) |
| `void save()` | Persist in-memory document to file (pretty-printed, 4 spaces). Throws on write error |
| `void reload()` | Discard unsaved changes, re-read from disk |
| `std::string dump()` | Pretty-printed JSON string of current document |
| `const path& path()` | Path from constructor |

**Load order:** Built-in defaults → file values override on a per-key basis. Unknown file keys are accepted (forward compatibility).

---

### `Logger`

**File:** `core/Logger.h` (header-only)

**Types:**

| Type | Description |
|------|-------------|
| `LogLevel` enum | `DEBUG`, `INFO`, `WARN`, `ERROR` |
| `Logger` (abstract) | `info(tag, msg)`, `warn(tag, msg)`, `error(tag, msg)`, `debug(tag, msg)` |
| `StdoutLogger` | Prints to stdout with `[LEVEL][tag] msg` format |
| `NullLogger` | No output (used in unit tests) |

---

## 7. Core — Robot

**File:** `core/Robot.h/.cpp`
**Purpose:** Main robot class — DI root, 50 Hz control loop, state machine orchestrator.

**Constructor:**
```cpp
Robot(std::unique_ptr<HardwareInterface> hw,
      std::shared_ptr<Config>            config,
      std::shared_ptr<Logger>            logger)
// throws std::invalid_argument if any arg is nullptr
```

**Lifecycle methods:**

| Method | Description |
|--------|-------------|
| `bool init()` | Open hardware, reset LEDs. Returns false on hardware failure. |
| `void loop()` | Run control loop until `stop()`. Sleeps to maintain 50 Hz. |
| `void run()` | Single control loop iteration (exposed for testing). |
| `void stop()` | Request graceful shutdown (thread-safe). |

**Command methods:**

| Method | Description |
|--------|-------------|
| `void startMowing()` | Dispatch operator command "Mow" to OpManager |
| `void startDocking()` | Dispatch operator command "Dock" |
| `void emergencyStop()` | Stop motors + dispatch "Idle" |
| `bool loadMap(path)` | Load map JSON from file. Call before `startMowing()`. |
| `void setPose(x, y, heading)` | Override StateEstimator pose |

**Accessors:**

| Method | Description |
|--------|-------------|
| `activeOpName()` | Active Op name string ("Idle", "Mow", etc.) |
| `lastOdometry()` | Last `OdometryData` snapshot |
| `lastSensors()` | Last `SensorData` snapshot |
| `lastBattery()` | Last `BatteryData` snapshot |
| `poseX() / poseY() / poseHeading()` | Current pose from StateEstimator |
| `isRunning()` | True while `loop()` is running |
| `controlLoops()` | Total number of `run()` calls |
| `opManager()` | Direct access to `OpManager` |

**Optional integration:**
```cpp
void setWebSocketServer(WebSocketServer* ws)
// Attach WS server — pushTelemetry() called every run()
```

**Control loop sequence (one `run()` call):**

```
1.  hw_->run()                      — driver tick (AT frames, LEDs, fan, WiFi)
2.  readOdometry/Sensors/Battery    — sensor snapshot
3.  stateEst_.update(odo, dt_ms)    — odometry dead-reckoning
4.  Build OpContext                 — populate all fields from current state
5.  checkBattery()                  — low voltage → dock/shutdown events
6.  opMgr_.tick(ctx)                — Op state machine step
7.  Safety stop                     — bumper/lift/motorFault → setMotorPwm(0,0,0)
8.  updateStatusLeds()              — LED_2 (status), LED_3 (GPS)
9.  ws_->pushTelemetry()            — WebSocket telemetry (if WS attached)
10. ++controlLoops_
```

**Battery guard thresholds (from Config):**

| Key | Default | Action |
|-----|---------|--------|
| `battery_low_v` | 22.0 V | → `onBatteryLowShouldDock` |
| `battery_critical_v` | 20.0 V | Motors off + `keepPowerOn(false)` + shutdown |

---

## 8. Core — Op State Machine

### Overview

All Op instances are owned by `OpManager` (no global singletons). `OpManager::tick()` is called every control loop iteration.

**Transition mechanism:**
1. An Op calls `changeOp(ctx, target)` → `requestOp(ctx, target, PRIO_NORMAL)`
2. `requestOp` calls `opMgr.setPending(target, priority, returnBackOnExit, caller)`
3. At the start of the next `tick()`: `activeOp->end()` → `target->begin()` → `target->run()`

**Priority levels:**

| Level | Value | Applied to |
|-------|-------|-----------|
| `PRIO_LOW` | 10 | — |
| `PRIO_NORMAL` | 50 | `changeOp()` default |
| `PRIO_HIGH` | 80 | `error_`, `charge_` minimum; operator commands for Mow/Dock/Charge |
| `PRIO_CRITICAL` | 100 | Operator stop/error; `onBatteryUndervoltage` |

**Return-back mechanism:** `changeOp(ctx, target, true)` sets `target.nextOp = caller`. When `target` completes, it calls `changeOp(ctx, *nextOp)` to return to the caller Op.

---

### State Transition Diagram

```
                    ┌─────────┐
         charger    │         │
         connected  │  Idle   │◄── operator "stop" (PRIO_CRITICAL)
       ─────────────┤         │◄── battery undervoltage (PRIO_CRITICAL)
       ↓            └────┬────┘
  ┌─────────┐            │ charger connected (>2s)
  │  Charge │◄───────────┘
  │         │
  │         │ timetable start + battery OK
  │         ├──────────────────────────────────────────────────► MowOp
  └────┬────┘ operator "Mow"
       │
       │ charger disconnected
       ↓
  ┌─────────┐                    ┌──────────────────┐
  │  Idle   │                    │  EscapeReverseOp │
  └─────────┘                    │  (3s reverse)    │
                                 └────────┬─────────┘
  ┌─────────┐ obstacle           ↑ onObstacle      │ done (returnBack)
  │  Mow    │────────────────────┘                 ↓
  │         │                              ┌──────────────┐
  │         │ GPS lost                     │  (caller Op) │
  │         ├─────────────────────────────►│  GpsWaitFix  │
  │         │ (returnBack)                 │  (2min max)  │
  │         │                              └──────┬───────┘
  │         │ GPS fix timeout                     │ GPS acquired
  │         │──────────────────►ErrorOp           └──► (returnBack to caller)
  │         │
  │         │ rain / battery low / timetable stop / no more waypoints
  │         ├─────────────────────────────────────────────────────► DockOp
  └─────────┘ operator "Dock"
                                           ┌─────────┐
  ┌─────────┐ charger connected            │  Error  │
  │  Dock   │───────────────────────────►  │         │ motors off, buzzer 500ms/5s
  │         │                         Charge│         │ LED_2 RED
  │         │ obstacle                      └─────────┘ exit: operator "Idle" only
  │         ├──► EscapeReverse (returnBack)
  │         │
  │         │ routing fails × 5
  │         ├──────────────────────────────────────────────────────► ErrorOp
  └─────────┘
```

---

### Op Details

**`IdleOp`** (`name() = "Idle"`)
- `begin()`: stop all motors
- `run()`: if charger connected for > 2 s → `ChargeOp`
- Transition in: operator stop (`PRIO_CRITICAL`), battery undervoltage, charger disconnect from Charge

**`MowOp`** (`name() = "Mow"`)
- `begin()`: start mow blade (PWM 200), `map->startMowing(x, y)`, `lineTracker->reset()`
- `run()`: `lineTracker->track(ctx, map, stateEst)`
- `end()`: stop all motors + mow blade
- Events: `onObstacle` → EscapeReverse (returnBack), `onGpsNoSignal` → GpsWait (returnBack), `onGpsFixTimeout` → ErrorOp, `onMotorError` → ErrorOp, `onRainTriggered/BatteryLow/TimetableStop/NoFurtherWaypoints` → DockOp, `onKidnapped` → GpsWait (returnBack), `onImuTilt/Error` → ErrorOp

**`DockOp`** (`name() = "Dock"`)
- `begin()`: `map->startDocking(x, y)`, `lineTracker->reset()`
- `run()`: if charger connected → `onChargerConnected`; otherwise `lineTracker->track()`
- Events: `onObstacle` → EscapeReverse (returnBack), `onNoFurtherWaypoints` → retry up to 5×, then ErrorOp; `onGpsNoSignal` → GpsWait (returnBack); `onGpsFixTimeout` → ErrorOp; `onChargerConnected` → ChargeOp; `onKidnapped` → GpsWait (returnBack)

**`ChargeOp`** (`name() = "Charge"`)
- `begin()`: stop motors, reset retry state
- `run()`: charger disconnect after 3 s → `onChargerDisconnected`; log V/I every 30 s; full charge (≥28.5 V + current < 0.1 A for ≥60 s) → `onChargingCompleted`
- Events: `onChargerDisconnected` → IdleOp; `onBadChargingContact` → creep forward 0.02 m/s for 500 ms; `onBatteryUndervoltage` → ErrorOp; `onRainTriggered` → stay; `onTimetableStartMowing` + battery ≥ `battery_low_v` → MowOp

**`EscapeReverseOp`** (`name() = "EscapeReverse"`)
- `begin()`: record which bumper was hit, set stop time = now + 3 s
- `run()`: drive -0.1 m/s with steering bias away from hit side; if outside perimeter → dock; after 3 s → check lift (→ ErrorOp) → check perimeter (→ DockOp) → `changeOp(*nextOp)` or IdleOp
- `EscapeForwardOp` (`name() = "EscapeForward"`): drive +0.1 m/s for 2 s, no steering

**`GpsWaitFixOp`** (`name() = "GpsWait"`)
- `begin()`: stop motors, record `waitStartTime_ms`
- `run()`: if `gpsHasFloat || gpsHasFix` → `changeOp(*nextOp)` or IdleOp; if > 2 min without GPS → ErrorOp

**`ErrorOp`** (`name() = "Error"`)
- `begin()`: stop motors, LED_2 RED, schedule first buzz at now+1 s
- `run()`: keep motors stopped; buzzer 500 ms on / every 5 s
- `end()`: LED_2 GREEN, buzzer off
- **No autonomous exit.** Operator must send "Idle" command explicitly.

---

### `OpContext`

Passed to every Op method each loop iteration:

| Field | Type | Source |
|-------|------|--------|
| `hw` | `HardwareInterface&` | Robot constructor |
| `config` | `Config&` | Robot constructor |
| `logger` | `Logger&` | Robot constructor |
| `opMgr` | `OpManager&` | Robot member |
| `sensors` | `SensorData` | `hw.readSensors()` |
| `battery` | `BatteryData` | `hw.readBattery()` |
| `odometry` | `OdometryData` | `hw.readOdometry()` |
| `x`, `y`, `heading` | `float` | StateEstimator |
| `insidePerimeter` | `bool` | `map.isInsideAllowedArea(x,y)` |
| `isDockingRoute` | `bool` | `map.isDocking()` |
| `gpsHasFix`, `gpsHasFloat` | `bool` | StateEstimator |
| `gpsFixAge_ms` | `unsigned long` | Phase 2 TODO (9999999 in Phase 1) |
| `now_ms` | `unsigned long` | Monotonic ms since Robot start |
| `stateEst*` | `nav::StateEstimator*` | Robot member |
| `map*` | `nav::Map*` | Robot member |
| `lineTracker*` | `nav::LineTracker*` | Robot member |

**Helper methods on `OpContext`:**

| Method | Description |
|--------|-------------|
| `stopMotors()` | `hw.setMotorPwm(0, 0, 0)` |
| `setMowMotor(on)` | `hw.setMotorPwm(0, 0, on ? 200 : 0)` |
| `setLinearAngularSpeed(v, ω)` | Unicycle → PWM conversion using `motor_max_speed_ms`, `wheel_base_m` |

---

## 9. Core — Navigation

### `nav::StateEstimator`

**File:** `core/navigation/StateEstimator.h/.cpp`
**Purpose:** Robot pose estimation from wheel encoder ticks.

**Phase 1:** Odometry dead-reckoning only.
**Phase 2:** `updateGps()` to be called from `Robot::run()` after GPS read.

**Methods:**

| Method | Description |
|--------|-------------|
| `update(OdometryData, dt_ms)` | Integrate encoder deltas into pose (x, y, heading) |
| `updateGps(posE, posN, isFix, isFloat)` | GPS position override stub (Phase 2) |
| `x()`, `y()`, `heading()` | Current pose (local metres, radians) |
| `groundSpeed()` | m/s, low-pass filtered from encoder deltas |
| `gpsHasFix()`, `gpsHasFloat()` | GPS quality flags |
| `setPose(x, y, heading)` | Override pose directly |
| `reset()` | Reset to origin (0, 0, 0) |

**Sanity guard:** Tick delta implying > 0.5 m in 20 ms is rejected (MCU reconnect artifact).
**Safety clamp:** If odometry drifts beyond ±10 km, pose is reset to origin.
**Speed LP filter:** `groundSpeed = 0.9 * groundSpeed + 0.1 * newSpeed`

**Config keys used:** `ticks_per_meter` (default 120), `wheel_base_m` (default 0.285)

---

### `nav::Map`

**File:** `core/navigation/Map.h/.cpp`
**Purpose:** Waypoint management, perimeter polygon, obstacle tracking.

**Coordinate system:** Local metres, east = +x, north = +y. Origin set by `AT+P` / Mission Service.

**Data types:**
`Point` struct: `{float x, float y}` + `distanceTo(Point)`
`PolygonPoints` = `std::vector<Point>`
`WayType` enum: `PERIMETER`, `EXCLUSION`, `DOCK`, `MOW`, `FREE`

**Key methods:**

| Method | Description |
|--------|-------------|
| `load(path)` | Load JSON map file from Mission Service |
| `save(path)` | Save map to JSON |
| `startMowing(x, y)` | Set first target to nearest mow point |
| `startDocking(x, y)` | Set path toward dock |
| `retryDocking(x, y)` | Retry after missed contact |
| `nextPoint(sim, x, y)` | Advance to next waypoint. Returns false when done. |
| `nextPointIsStraight()` | True if no sharp turn at next waypoint |
| `isInsideAllowedArea(x, y)` | Inside perimeter AND outside all exclusions |
| `addObstacle(x, y)` | Mark virtual obstacle (avoidance input to A*) |
| `getDockingPos(x, y, δ, idx)` | Docking approach position and heading |
| `mowingCompleted()` | True when all mow points visited |

**Public state:**

| Field | Type | Description |
|-------|------|-------------|
| `targetPoint` | `Point` | Current navigation target |
| `lastTargetPoint` | `Point` | Previous target (defines tracking line) |
| `trackReverse` | `bool` | True = drive in reverse to target |
| `trackSlow` | `bool` | True = reduced speed (docking approach) |
| `wayMode` | `WayType` | Current navigation mode |
| `percentCompleted` | `int` | Mowing progress 0–100% |

---

### `nav::LineTracker`

**File:** `core/navigation/LineTracker.h/.cpp`
**Purpose:** Stanley path-tracking controller.

**Stanley formula:**
```
angular = p * headingError + atan2(k * lateralError, 0.001 + |groundSpeed|)
```

**Methods:**

| Method | Description |
|--------|-------------|
| `reset()` | Clear rotation state — call in Op::begin() |
| `track(ctx, map, estimator)` | One control iteration: compute steering, fire events, advance waypoints |
| `lateralError()` | Current cross-track error (m) |
| `targetDist()` | Distance to current target (m) |
| `angleToTargetFits()` | True if heading error < threshold (→ Stanley phase active) |

**Op events fired by `track()`:**

| Event | Condition |
|-------|-----------|
| `onTargetReached` | Distance to target < `TARGET_REACHED_TOLERANCE` (0.2 m) |
| `onNoFurtherWaypoints` | `map.nextPoint()` returned false |
| `onKidnapped(true)` | Distance from planned line > `KIDNAP_TOLERANCE` (3.0 m) |
| `onKidnapped(false)` | Robot back within 3 m of planned line |

**Constants (in `LineTracker.h`):**

| Constant | Value | Description |
|----------|-------|-------------|
| `TARGET_REACHED_TOLERANCE` | 0.2 m | Distance at which target is considered reached |
| `KIDNAP_TOLERANCE` | 3.0 m | Distance off planned line → kidnap event |
| `ROTATE_SPEED_RADPS` | 29°/s → rad/s | Angular speed during rotation phase |

**Config keys used:**

| Key | Default | Description |
|-----|---------|-------------|
| `motor_set_speed_ms` | 0.3 | Forward speed during tracking (m/s) |
| `stanley_k_normal` | 1.0 | Cross-track gain (normal) |
| `stanley_p_normal` | 2.0 | Heading gain (normal) |
| `stanley_k_slow` | 0.2 | Cross-track gain (slow/docking) |
| `stanley_p_slow` | 0.5 | Heading gain (slow/docking) |
| `dock_linear_speed_ms` | 0.1 | Forward speed during docking approach |

---

## 10. Core — WebSocketServer

**File:** `core/WebSocketServer.h/.cpp`
**Library:** Crow v1.2.0 (via FetchContent) + standalone Asio 1.30.2
**Pimpl:** Crow headers confined to `.cpp` — not in `.h`

### Constructor

```cpp
WebSocketServer(std::shared_ptr<Config> config,
                std::shared_ptr<Logger> logger)
```

### Lifecycle

| Method | Description |
|--------|-------------|
| `start()` | Configure Crow route, start Crow async, start push thread. Returns immediately. |
| `stop()` | Set `running_ = false`, join push thread, call `app.stop()` |
| `isRunning()` | True between `start()` and `stop()` |

Port from config key `ws_port` (default 8765).

### Data feed

```cpp
void pushTelemetry(const TelemetryData& data)
// Thread-safe. Stores latest snapshot; push thread broadcasts every 100 ms.
```

### Command reception

```cpp
void onCommand(CommandCallback cb)
// cb(std::string cmd, nlohmann::json params)
// Called from Crow I/O thread — must be thread-safe
```

### `TelemetryData` struct

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `op` | `string` | `"Idle"` | Active Op name |
| `x` | `float` | 0.0 | Local east (m) |
| `y` | `float` | 0.0 | Local north (m) |
| `heading` | `float` | 0.0 | Heading (rad, 0=east) |
| `battery_v` | `float` | 0.0 | Battery voltage (V) |
| `charge_v` | `float` | 0.0 | Charger output voltage (V) |
| `gps_sol` | `int` | 0 | NMEA quality (0=none, 4=RTK fix, 5=RTK float) |
| `gps_text` | `string` | `"---"` | Human-readable GPS quality |
| `gps_lat` | `double` | 0.0 | WGS-84 latitude (Phase 2) |
| `gps_lon` | `double` | 0.0 | WGS-84 longitude (Phase 2) |
| `bumper_l` | `bool` | false | Left bumper |
| `bumper_r` | `bool` | false | Right bumper |
| `motor_err` | `bool` | false | Motor fault |
| `uptime_s` | `ulong` | 0 | Seconds since Robot start |

### Threading model

```
Thread A (Robot::run() @ 50 Hz)  →  pushTelemetry()  →  mutex → latestTelemetry_
Thread B (Crow I/O pool)          →  onmessage() → commandCallback_
Thread C (serverThread_ push loop)→  every 100ms: read latestTelemetry_ → broadcast
```

---

## 11. Core — RobotConstants

**File:** `core/RobotConstants.h`
**Purpose:** Compile-time architectural constants (NOT runtime-tunable).

| Constant | Value | Description |
|----------|-------|-------------|
| `OVERALL_MOTION_TIMEOUT_MS` | 10 000 ms | Max time in motion-requested state before fault |
| `MOTION_LP_DECAY_MS` | 2 000 ms | Ground-speed LP filter time constant |
| `GPS_NO_MOTION_THRESHOLD_M` | 0.05 m | Below this displacement → robot considered stationary |
| `OBSTACLE_ROTATION_TIMEOUT_MS` | 15 000 ms | Max rotation time after obstacle before abort |
| `OBSTACLE_ROTATION_SPEED_DEG_S` | 10.0 °/s | Angular speed during obstacle-avoidance rotation |

---

## 12. WebSocket API

### Endpoint

```
ws://host:8765/ws/telemetry
```

Port configurable via `ws_port` in `config.json`.

### Server → Client: Telemetry (10 Hz)

```json
{
  "type":      "state",
  "op":        "Mow",
  "x":         1.5000,
  "y":         2.5000,
  "heading":   0.7854,
  "battery_v": 25.40,
  "charge_v":  0.00,
  "gps_sol":   4,
  "gps_text":  "RTK",
  "gps_lat":   51.12345678,
  "gps_lon":   7.12345678,
  "bumper_l":  false,
  "bumper_r":  false,
  "motor_err": false,
  "uptime_s":  123
}
```

**Op name values (frozen — must not change):**
`"Idle"`, `"Mow"`, `"Dock"`, `"Charge"`, `"EscapeReverse"`, `"GpsWait"`, `"Error"`

**GPS quality `gps_sol` values:**

| Value | Meaning |
|-------|---------|
| 0 | No fix (Phase 1 default) |
| 4 | RTK fixed |
| 5 | RTK float |

### Server → Client: Keepalive

Sent when no new telemetry within 100 ms:

```json
{"type": "ping"}
```

### Client → Server: Commands

```json
{"cmd": "start"}
{"cmd": "stop"}
{"cmd": "dock"}
{"cmd": "charge"}
{"cmd": "setpos", "lat": 51.12345, "lon": 7.12345}
```

| Command | Robot action |
|---------|-------------|
| `start` | `robot.startMowing()` |
| `stop` | `robot.emergencyStop()` |
| `dock` | `robot.startDocking()` |
| `charge` | `robot.startDocking()` (alias) |
| `setpos` | `robot.setPose(lon, lat, 0)` |

**Format compatibility:** Identical to `sunray/mission_api.cpp:254-274` and Python `SunrayClient`. Any change to field names or Op name strings will break the Mission Service frontend.

---

## 13. Config Keys Reference

All keys have built-in defaults (defined in `Config::defaults()` in `Config.cpp`).
Production config: `/etc/sunray/config.json` (see `config.example.json` for template).

### Hardware / Driver

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `driver` | string | `"serial"` | `"serial"` = Alfred, `"sim"` = SimulationDriver |
| `driver_port` | string | `"/dev/ttyS0"` | UART device for STM32 |
| `driver_baud` | int | `115200` | Must match rm18.ino |
| `i2c_bus` | string | `"/dev/i2c-1"` | Linux I2C bus for PortExpander + ADC |
| `port_expander_addr` | string | `"0x20"` | I2C address of main PortExpander |
| `ws_port` | int | `8765` | WebSocket server port |

### Navigation

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ticks_per_meter` | float | `120` | Encoder ticks per metre of travel |
| `wheel_base_m` | float | `0.285` | Distance between wheel centres (m) |
| `stanley_k` | float | `0.5` | Stanley cross-track gain (legacy key) |
| `stanley_k_normal` | float | `1.0` | Cross-track gain (normal mowing) |
| `stanley_p_normal` | float | `2.0` | Heading gain (normal mowing) |
| `stanley_k_slow` | float | `0.2` | Cross-track gain (docking) |
| `stanley_p_slow` | float | `0.5` | Heading gain (docking) |
| `motor_set_speed_ms` | float | `0.3` | Forward speed during mowing (m/s) |
| `dock_linear_speed_ms` | float | `0.1` | Forward speed during docking (m/s) |
| `motor_max_speed_ms` | float | `0.5` | PWM=255 maps to this speed (m/s) |
| `gps_no_motion_threshold_m` | float | `0.05` | GPS displacement below this = stationary |

### Energy

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `battery_low_v` | float | `22.0` | Return-to-dock threshold (V) |
| `battery_critical_v` | float | `20.0` | Emergency shutdown threshold (V) |
| `battery_full_v` | float | `29.4` | Stop-charging threshold (V) |

### Peripherals

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `buzzer_enabled` | bool | `true` | Enable/disable buzzer output |

### Undocking

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `undock_speed_ms` | float | `0.1` | Reverse speed during undock (m/s) |
| `undock_distance_m` | float | `0.5` | Minimum exit distance from dock point (m) |
| `undock_charger_timeout_ms` | int | `3000` | Max wait for charger disconnect (ms) |
| `undock_position_timeout_ms` | int | `10000` | Overall undock timeout (ms) |
| `undock_heading_tolerance_rad` | float | `0.175` | IMU heading check tolerance (≈ ±10°) |
| `undock_encoder_check_ms` | int | `1000` | Encoder cross-check window (ms) |
| `undock_encoder_min_ticks` | int | `5` | Min ticks to confirm movement |

### Recovery

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `stuck_detect_timeout_ms` | int | `5000` | No movement for this long = stuck (ms) |
| `stuck_recovery_max_attempts` | int | `3` | Max stuck-recovery attempts before ErrorOp |
| `dock_retry_max_attempts` | int | `3` | Max docking retries after missed contact |
| `obstacle_loop_max_count` | int | `5` | Obstacles in window → stuck recovery |
| `obstacle_loop_window_ms` | int | `30000` | Time window for obstacle loop detection (ms) |

### Scheduling

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `rain_delay_min` | int | `60` | Minutes to wait after rain before restart |

---

## 14. Build System

**Root:** `CMakeLists.txt` — CMake 3.20+, C++17.

**FetchContent dependencies:**

| Library | Tag | Use |
|---------|-----|-----|
| nlohmann/json | v3.11.3 | Config, WebSocketServer JSON |
| Catch2 | v3.6.0 | Unit tests |
| asio (standalone) | asio-1-30-2 | Required by Crow (header-only) |
| Crow | v1.2.0 | WebSocket server |

**Static libraries:**

| Target | Sources |
|--------|---------|
| `sunray_platform` | Serial.cpp, I2C.cpp, PortExpander.cpp |
| `sunray_hal` | HardwareInterface.h (interface only) |
| `sunray_serial_driver` | SerialRobotDriver.cpp |
| `sunray_simulation_driver` | SimulationDriver.cpp |
| `sunray_core` | Config.cpp, Robot.cpp, WebSocketServer.cpp, all Op/*.cpp, all navigation/*.cpp |

**Executable:** `sunray-core` — links all static libraries + Crow::Crow.

**Tests:** `sunray_tests` — links `sunray_core` + `sunray_simulation_driver` + Catch2. No real hardware.

**Platform detection:** Warning issued when building on non-Linux (Serial/I2C use POSIX APIs).

---

## 15. Entry Point (`main.cpp`)

**Usage:**
```bash
./sunray-core                          # Alfred hardware, config from /etc/sunray/config.json
./sunray-core /path/to/config.json     # custom config file
./sunray-core --sim                    # SimulationDriver (no hardware)
./sunray-core --sim /path/to/config   # both flags
```

**DI wiring sequence:**

```cpp
1. Config   = std::make_shared<Config>(configPath)
2. Logger   = std::make_shared<StdoutLogger>(INFO)
3. HW       = std::make_unique<SerialRobotDriver>(config)   // or SimulationDriver
4. Robot    = Robot(std::move(hw), config, logger)
5. WsServer = std::make_unique<WebSocketServer>(config, logger)
6. wsServer->onCommand(lambda)   // routes cmd → robot.startMowing/Docking/emergencyStop/setPose
7. wsServer->start()
8. robot.setWebSocketServer(wsServer.get())
9. robot.init()
10. robot.loop()        // blocks until SIGINT/SIGTERM
11. wsServer->stop()
```

**Signal handling:** `SIGINT` / `SIGTERM` → `robot.stop()` (via `g_robot` pointer, only calls atomic flag write — safe from signal handler).
