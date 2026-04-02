# Project Map

## Repository Shape

### FACT

Active top-level source areas from the repository tree and build files:

- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- [`core/`](/mnt/LappiDaten/Projekte/sunray-core/core)
- [`hal/`](/mnt/LappiDaten/Projekte/sunray-core/hal)
- [`platform/`](/mnt/LappiDaten/Projekte/sunray-core/platform)
- [`tests/`](/mnt/LappiDaten/Projekte/sunray-core/tests)
- [`tools/`](/mnt/LappiDaten/Projekte/sunray-core/tools)
- [`webui-svelte/`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte)
- [`docs/`](/mnt/LappiDaten/Projekte/sunray-core/docs)
- [`scripts/`](/mnt/LappiDaten/Projekte/sunray-core/scripts)

Generated or local-only areas visible in the workspace:

- [`build_verify/`](/mnt/LappiDaten/Projekte/sunray-core/build_verify)
- [`webui-svelte/dist/`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/dist)
- [`webui-svelte/node_modules/`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/node_modules)

These generated areas are not source-of-truth code.

## Entry Points

### FACT

- Backend executable entry point: [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- Backend runtime control object: [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)
- Backend operation dispatcher: `OpManager` in [`core/op/Op.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.cpp)
- Frontend entry point for build/dev tooling: [`webui-svelte/package.json`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/package.json)
- Test entry point: `sunray_tests` target from [`tests/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt)

## Startup Path

### FACT

Runtime startup path from [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp):

- config resolution
- logger creation
- backend driver selection
- `Robot` construction
- `robot.init()`
- optional GPS driver setup
- `WebSocketServer` construction and callback registration
- `MqttClient` construction and callback registration
- `robot.loop()`

## Build System

### FACT

- Root backend build file: [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt)
- Core module build file: [`core/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/core/CMakeLists.txt)
- HAL build file: [`hal/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/hal/CMakeLists.txt)
- Platform build file: [`platform/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/platform/CMakeLists.txt)
- Tests build file: [`tests/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt)
- Frontend build manifest: [`webui-svelte/package.json`](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/package.json)

### Build Paths

- Backend executable: `sunray-core`
- Test executable: `sunray_tests`
- Probe executables:
  - `rm18_serial_probe`
  - `ex3_led_probe`
  - `ex2_buzzer_probe`
  - `ex_led_buzzer_test`
  - `pca9555_probe`
- Frontend scripts:
  - `npm run dev`
  - `npm run build`
  - `npm run preview`
  - `npm run check`

## Platform Separation

### Core Runtime

### FACT

[`core/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/core/CMakeLists.txt) builds the platform-independent runtime library from:

- control logic
- state machine ops
- navigation
- config
- WebSocket server
- MQTT client
- history/event storage
- schedule logic

### HAL

### FACT

[`hal/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/hal/CMakeLists.txt) defines:

- interface-only `sunray_hal`
- `SerialRobotDriver`
- `SimulationDriver`
- `GpsDriver`
- `Imu`

### Platform

### FACT

[`platform/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/platform/CMakeLists.txt) contains Linux/Posix accessors:

- `Serial`
- `I2C`
- `I2cMux`
- `PortExpander`

### INFERENCE

- The intended separation is:
  - `platform`: Linux device access
  - `hal`: hardware backend contracts and drivers
  - `core`: runtime behavior independent of direct Linux device APIs

## Linux Versus STM32 Responsibilities

## Linux / Pi

### FACT

- Owns process startup and loop execution
- Hosts WebSocket and MQTT integrations
- Runs navigation, scheduling, and state machine logic
- Accesses serial and I2C through Linux-facing platform code

## STM32

### FACT

- The Alfred runtime path uses `SerialRobotDriver`, indicating an MCU-backed hardware peer
- Board-facing telemetry and actuator exchange are abstracted through that driver

### UNKNOWN

- Exact internal STM32 module ownership

## Safety-Relevant Areas

### FACT

High-sensitivity runtime areas visible from file structure and startup path:

- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [`core/op/`](/mnt/LappiDaten/Projekte/sunray-core/core/op)
- [`core/navigation/`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation)
- [`hal/SerialRobotDriver/`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver)
- [`platform/Serial.cpp`](/mnt/LappiDaten/Projekte/sunray-core/platform/Serial.cpp)
- [`platform/I2C.cpp`](/mnt/LappiDaten/Projekte/sunray-core/platform/I2C.cpp)

## High-Risk Files

### FACT

- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
  Reason: startup assembly, driver selection, external interface wiring
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
  Reason: loop ordering, safety guards, telemetry, shutdown behavior
- [`core/op/Op.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.cpp)
  Reason: operation switching and operator-command routing
- [`core/navigation/Map.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
  Reason: dock and route behavior, local replans, perimeter-related path decisions
- [`core/navigation/StateEstimator.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp)
  Reason: pose propagation and sensor-fusion behavior
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
  Reason: command and API surface
- [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)
  Reason: remote command and telemetry path
- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
  Reason: Alfred hardware protocol boundary

## Tests And Tools

### FACT

- [`tests/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt) builds a single Catch2-based test executable from multiple scenario and unit files.
- `tools/*.cpp` produce probe binaries for hardware-oriented checks.

## FACT / INFERENCE / UNKNOWN

### FACT

- The repository has clear backend module separation.
- The backend startup path is centralized in `main.cpp`.
- The frontend is a separate Svelte/Vite project.
- Tests are first-class and compiled through CMake.

### INFERENCE

- The repo is organized to keep direct Linux device access out of most runtime logic.
- `SerialRobotDriver` is the critical hardware boundary for real Alfred operation.

### UNKNOWN

- Production packaging and service orchestration
- Exact hardware revision spread in deployed Alfred units
- Whether any external repo or service owns part of runtime startup
