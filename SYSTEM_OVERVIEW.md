# System Overview

## Runtime Topology

### FACT

- The executable entry point is [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp).
- `main.cpp` wires together `Config`, logger, a hardware driver, `Robot`, `WebSocketServer`, and `MqttClient`.
- The runtime can start in two modes:
  - Alfred hardware mode using `SerialRobotDriver`
  - simulation mode using `SimulationDriver` via `--sim`
- The long-running control loop is `Robot::loop()` in [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp).

### INFERENCE

- The Pi-side process is the orchestration layer for mission state, APIs, telemetry, and higher-level safety policy.
- The repo is structured so that Linux-specific I/O, backend runtime logic, and board-facing driver logic are intentionally separated.

### UNKNOWN

- Exact process supervision and production boot orchestration on Alfred hardware
- Whether additional services outside this repo participate in runtime startup

## Startup Path

### FACT

From [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp), the startup path is:

1. Parse arguments and resolve config path.
2. Construct `Config`.
3. Construct logger fanout.
4. Select hardware backend:
   - `SimulationDriver` when `--sim`
   - `SerialRobotDriver` otherwise
5. Construct `Robot`.
6. Install `SIGINT` and `SIGTERM` handlers that call `Robot::stop()`.
7. Call `robot.init()`.
8. In non-sim mode, construct and optionally attach `UbloxGpsDriver`.
9. Construct and configure `WebSocketServer`, including:
   - static web root
   - map path
   - mission path
   - command callbacks
   - diagnostics callbacks
   - schedule callbacks
   - history/statistics callbacks
   - simulator callbacks in sim mode
10. Start `WebSocketServer` and attach it to `Robot`.
11. Construct `MqttClient`, register command callback, start it, and attach it to `Robot`.
12. Enter `robot.loop()`.
13. On shutdown, stop MQTT, stop WebSocket server, and exit cleanly.

## Pi Responsibilities

### FACT

Based on [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp), [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp), and the `core/` module build list:

- Own process startup and dependency wiring
- Run the main control loop
- Hold mission and operation state through `OpManager` and `core/op/*`
- Run navigation and planning components under `core/navigation/*`
- Manage telemetry and control surfaces through `WebSocketServer`
- Optionally run MQTT integration through `MqttClient`
- Maintain schedule and history/statistics functionality
- Access Linux serial and I2C facilities through `platform/*`

## STM32 Responsibilities

### FACT

- The default non-sim hardware backend is `SerialRobotDriver`.
- The serial driver is described in code comments and naming as the Alfred/STM32 implementation.
- Runtime sensor and actuator abstractions imply board-side exchange of:
  - odometry
  - battery data
  - bumper and lift data
  - motor-fault-related status

### INFERENCE

- The STM32 is the immediate board-facing control peer for Alfred hardware.
- Low-level actuator and sensor exchange likely remain STM32-backed while the Pi decides high-level behavior.

### UNKNOWN

- Exact STM32 firmware-side task breakdown
- Which safety behaviors are enforced independently on the MCU
- Exact pin-level ownership split between Pi and STM32

## Pi To STM32 Communication

### FACT

- Default runtime transport is serial via configured `driver_port`.
- The active Alfred backend is `hal/SerialRobotDriver`.
- `Robot` interacts with hardware through `HardwareInterface`, not directly with Linux or STM32 details.

### INFERENCE

- The serial link is the primary operational contract between Pi-side logic and Alfred board firmware.

## Runtime Modes

### FACT

The active operation set is visible in the `sunray_core` build sources under [`core/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/core/CMakeLists.txt):

- `Idle`
- `Charge`
- `Undock`
- `NavToStart`
- `WaitRain`
- `Mow`
- `Dock`
- `EscapeReverse`
- `GpsWaitFix`
- `Error`

Also present in the runtime is `EscapeForward`, implemented in [`core/op/EscapeReverseOp.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op/EscapeReverseOp.cpp) and managed by `OpManager`.

### INFERENCE

- `GpsWaitFix` maps to the user-facing degraded GPS wait/recovery mode.
- `EscapeForward` and `EscapeReverse` are recovery substates rather than operator-facing mission states.

## Linux Versus STM32 Responsibilities

## Linux / Pi Side

### FACT

- Process and server startup
- Serial, I2C, mux, and port-expander access in `platform/*`
- Runtime control loop and state machine in `core/*`
- API serving and MQTT integration
- Local simulation mode

## STM32 / Board Side

### FACT

- Board interaction is abstracted through the Alfred serial driver
- Runtime expects MCU-provided board telemetry

### UNKNOWN

- Exact firmware scheduling and watchdog layering

## Build Paths

### FACT

- Backend build system is CMake at [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt).
- Backend modules are divided into:
  - `platform`
  - `hal`
  - `core`
- Main executable target is `sunray-core`.
- Test executable target is `sunray_tests`.
- Additional probe executables are built from `tools/*.cpp`.
- Frontend build path is [`webui-svelte/package.json`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/package.json) using Vite scripts.

### FACT

Root CMake declares external dependencies via `FetchContent`:

- `nlohmann_json`
- `Catch2`
- `asio`
- `Crow`

`core/CMakeLists.txt` conditionally enables:

- `libmosquitto`
- `sqlite3`

## FACT / INFERENCE / UNKNOWN Summary

### FACT

- `sunray-core` is a Linux-hosted backend with explicit simulation and Alfred serial backends.
- `Robot::loop()` is the runtime heartbeat.
- `main.cpp` wires WebSocket and MQTT around `Robot`.
- The repo has explicit module separation across `platform`, `hal`, and `core`.

### INFERENCE

- The Pi is the system coordinator; the STM32 is the board-facing execution peer.
- The repo is designed to keep hardware-specific behavior behind HAL and platform boundaries.

### UNKNOWN

- Production process supervision
- Full MCU-side safety implementation
- Exact deployed boot sequence on Alfred units
