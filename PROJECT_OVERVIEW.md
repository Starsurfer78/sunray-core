# PROJECT_OVERVIEW

Last updated: 2026-03-28

## Purpose

`sunray-core` is a Linux-first robot controller for an RTK-GPS lawn mower running on Raspberry Pi. The repository contains:

- a C++17 runtime for robot control, navigation, mission/state logic, hardware access, HTTP/WebSocket API, and optional MQTT
- a Vue 3 + TypeScript WebUI served as static files by the C++ backend
- a simulation mode for development without physical hardware
- native and frontend tests

The codebase is organized around one central runtime object: `sunray::Robot`, which orchestrates the hardware abstraction layer, navigation, state machine, and telemetry.

## High-Level Architecture

### Runtime layers

- `platform/`
  Linux-near wrappers for serial, I2C, and port expanders.
- `hal/`
  The hardware boundary. `HardwareInterface` defines the contract; concrete implementations include:
  - `SerialRobotDriver` for Alfred STM32 hardware over UART/I2C
  - `SimulationDriver` for software-only execution
  - GPS and IMU drivers
- `core/`
  Platform-independent robot logic:
  - `Robot`
  - config and logging
  - op/state machine
  - navigation and map handling
  - Crow-based HTTP/WebSocket server
  - optional MQTT client
- `webui/`
  Vue frontend with dashboard, map editor, scheduler, diagnostics, simulator, history, and settings

### Central control flow

[`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp) is the native executable entry point. It:

1. loads config
2. selects `SimulationDriver` or `SerialRobotDriver`
3. creates `Robot`
4. optionally attaches `UbloxGpsDriver`
5. starts `WebSocketServer`
6. starts `MqttClient`
7. enters `Robot::loop()`

[`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) runs the 50 Hz control loop:

1. tick hardware
2. read odometry, sensors, battery, IMU
3. update state estimation
4. check schedule, battery, GPS resilience, obstacle cleanup
5. tick the `OpManager` state machine
6. apply manual-drive and safety behavior
7. publish telemetry to WebSocket and MQTT

## Entry Points

### Native executable

- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- executable target: `sunray-core`
- modes:
  - `./build_linux/sunray-core --sim config.example.json`
  - `./build_linux/sunray-core /etc/sunray/config.json`

### Backend API surface

- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- WebSocket: `/ws/telemetry`
- HTTP:
  - `/`
  - `/assets/<path>`
  - `/api/version`
  - `/api/config`
  - `/api/map`
  - `/api/map/geojson`
  - `/api/schedule`
  - `/api/sim/<action>`
  - `/api/diag/<action>`

### Frontend

- [`webui/src/main.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/main.ts)
- [`webui/src/router/index.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/router/index.ts)
- views:
  - Dashboard
  - MapEditor
  - Scheduler
  - History
  - Statistics
  - Diagnostics
  - Simulator
  - Settings

### Tests

- native tests: [`tests/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt)
- frontend tests: [`webui/src/composables/useMowPath.test.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useMowPath.test.ts)

Verification observed during this scan:

- `ctest --test-dir build_linux --output-on-failure`: 175/175 passed
- `npm test` in `webui/`: 6/6 passed

## Build Process

### Native build

Top-level CMake is defined in [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt).

- language: C++17
- build system: CMake
- external dependencies fetched with `FetchContent`:
  - `nlohmann_json`
  - `Catch2`
  - `asio`
  - `Crow`
- optional system dependency:
  - `libmosquitto` via `pkg-config`

Typical commands:

```bash
cmake -S . -B build_linux
cmake --build build_linux -j2
ctest --test-dir build_linux --output-on-failure
```

Hinweis:

- Build-Verzeichnisse sind lokale, generierte Artefakte und nicht die Source of Truth.
- Für Builds auf einem Zielsystem wie dem Raspberry Pi sollte ein frisches Build-Verzeichnis verwendet werden, z. B. `build_pi/`.
- Bereits vorhandene Build-Ordner aus anderen Maschinen oder Toolchains sollten nicht als Grundlage für einen sauberen Build dienen.

### Frontend build

Frontend build is defined in [`webui/package.json`](/mnt/LappiDaten/Projekte/sunray-core/webui/package.json) and [`webui/vite.config.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/vite.config.ts).

- stack: Vue 3, Vue Router, TypeScript, Vite, Tailwind CSS, Vitest
- dev proxy:
  - `/api` -> `http://localhost:8765`
  - `/ws` -> `ws://localhost:8765`
- production output: `webui/dist`

Typical commands:

```bash
cd webui
npm install
npm run build
npm test
```

## Critical Files

### System bootstrap

- [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt)
- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- [`config.example.json`](/mnt/LappiDaten/Projekte/sunray-core/config.example.json)

### Core runtime

- [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [`core/WebSocketServer.h`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.h)
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`core/MqttClient.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/MqttClient.cpp)

### Hardware boundary

- [`hal/HardwareInterface.h`](/mnt/LappiDaten/Projekte/sunray-core/hal/HardwareInterface.h)
- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [`hal/SimulationDriver/SimulationDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SimulationDriver/SimulationDriver.cpp)
- [`hal/GpsDriver/UbloxGpsDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/GpsDriver/UbloxGpsDriver.cpp)
- [`hal/Imu/Mpu6050Driver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/Imu/Mpu6050Driver.cpp)

### State machine and navigation

- [`core/op/Op.h`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h)
- [`core/op/*.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op)
- [`core/navigation/StateEstimator.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp)
- [`core/navigation/Map.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- [`core/navigation/GridMap.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp)
- [`core/navigation/LineTracker.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

### Frontend integration

- [`webui/src/composables/useTelemetry.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useTelemetry.ts)
- [`webui/src/views/Dashboard.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Dashboard.vue)
- [`webui/src/views/MapEditor.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/MapEditor.vue)
- [`webui/src/views/Settings.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Settings.vue)

## Key Risks

### Operational risks

- Hardware validation is still incomplete on real Raspberry Pi / Alfred hardware. `TODO.md` still marks Pi build and real-drive validation as open.
- Safety-critical behavior depends on real sensor timing, GPS quality, charger contact behavior, and watchdog coordination that are only partly covered by tests.
- `SerialRobotDriver` performs platform shutdown with `std::system("shutdown now")`, which is correct for deployment but high-impact if triggered unexpectedly on real hardware.

### Repository hygiene risks

- Generated build trees are present in the repo: `build/`, `build_gcc/`, `build_linux/`, `build_clang_ninja/`.
- `webui/node_modules/` and `webui/dist/` are also present, which increases repo size and makes source-vs-generated boundaries easier to misunderstand.
- `ALTE_DATEIEN/` and older workflow material remain in-tree. Useful as reference, but not all of it matches current code.

Operational convention going forward:

- local build directories should be treated as disposable
- frontend dependency and dist folders should be reproducible locally
- the authoritative project state is the source tree plus documentation, not generated output

### Architecture risks

- The `Robot` class is the main orchestration hub and carries a lot of responsibility: control loop, telemetry assembly, schedule handling, manual drive, diagnostic flow, GPS resilience, and state-machine wiring.
- Backend and frontend are tightly coupled through a frozen telemetry format and specific REST/WS payloads. Changes in one side can easily break the other.
- Map handling, routing, and recovery logic span multiple modules (`Robot`, `Map`, `GridMap`, `LineTracker`, `Op` classes), so navigation changes have wide impact.

## Recommended Reading Order

1. [`README.md`](/mnt/LappiDaten/Projekte/sunray-core/README.md)
2. [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
3. [`hal/HardwareInterface.h`](/mnt/LappiDaten/Projekte/sunray-core/hal/HardwareInterface.h)
4. [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)
5. [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
6. [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
7. [`docs/OP_STATE_MACHINE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md)
8. [`webui/README.md`](/mnt/LappiDaten/Projekte/sunray-core/webui/README.md)
9. [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
