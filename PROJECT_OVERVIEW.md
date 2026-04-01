# PROJECT_OVERVIEW

Last updated: 2026-04-01

## Purpose

`sunray-core` is a Linux-first robot controller for an RTK-GPS lawn mower on Raspberry Pi.

The repository contains:

- a C++17 runtime for control loop, navigation, state machine, hardware access, HTTP/WebSocket API, and optional MQTT
- an active Svelte + TypeScript WebUI under `webui-svelte/`
- a simulation mode for development without physical hardware
- native Catch2 tests and frontend checks

## High-Level Architecture

### Runtime layers

- `platform/`
  Linux-near wrappers for serial, I2C, mux, and port expanders
- `hal/`
  hardware boundary via `HardwareInterface`, `SerialRobotDriver`, `SimulationDriver`, GPS, and IMU drivers
- `core/`
  `Robot`, config, op/state machine, navigation, planner/costmap/route handling, HTTP/WebSocket server, optional MQTT
- `webui-svelte/`
  active frontend with dashboard, map, mission, diagnostics, and shared stores/components

### Central control flow

`main.cpp`:

1. loads config
2. selects `SimulationDriver` or `SerialRobotDriver`
3. creates `Robot`
4. optionally attaches GPS
5. creates `WebSocketServer`
6. creates optional `MqttClient`
7. enters `Robot::loop()`

`core/Robot.cpp` runs the 50 Hz control loop and wires sensors, safety, state machine, navigation, telemetry, and mission state together.

## Entry Points

### Native executable

- `main.cpp`
- target: `sunray-core`
- modes:
  - `./build_pi/sunray-core --sim config.example.json`
  - `./build_pi/sunray-core /etc/sunray-core/config.json`

### Backend API surface

- `core/WebSocketServer.cpp`
- WebSocket:
  - `/ws/telemetry`
- HTTP:
  - `/`
  - `/assets/<path>`
  - `/api/version`
  - `/api/config`
  - `/api/map`
  - `/api/map/geojson`
  - `/api/missions`
  - `/api/schedule`
  - `/api/history/events`
  - `/api/history/sessions`
  - `/api/history/export`
  - `/api/statistics/summary`
  - `/api/sim/<action>`
  - `/api/diag/<action>`

### Frontend

- `webui-svelte/src/main.ts`
- `webui-svelte/src/App.svelte`
- shared API/stores:
  - `webui-svelte/src/lib/api/websocket.ts`
  - `webui-svelte/src/lib/api/types.ts`
  - `webui-svelte/src/lib/stores/telemetry.ts`
  - `webui-svelte/src/lib/stores/map.ts`
  - `webui-svelte/src/lib/stores/missions.ts`

### Tests

- native tests: `tests/CMakeLists.txt`
- frontend validation: `webui-svelte/package.json`

## Critical Files

### System bootstrap

- `CMakeLists.txt`
- `main.cpp`
- `config.example.json`

### Core runtime

- `core/Robot.h`
- `core/Robot.cpp`
- `core/WebSocketServer.h`
- `core/WebSocketServer.cpp`
- `core/MqttClient.cpp`

### Navigation and state machine

- `core/op/Op.h`
- `core/op/Op.cpp`
- `core/navigation/StateEstimator.cpp`
- `core/navigation/Map.cpp`
- `core/navigation/Planner.cpp`
- `core/navigation/Costmap.cpp`
- `core/navigation/GridMap.cpp`
- `core/navigation/LineTracker.cpp`
- `core/navigation/Route.h`

### Hardware boundary

- `hal/HardwareInterface.h`
- `hal/SerialRobotDriver/SerialRobotDriver.cpp`
- `hal/SimulationDriver/SimulationDriver.cpp`
- `hal/GpsDriver/UbloxGpsDriver.cpp`
- `hal/Imu/Mpu6050Driver.cpp`

## Current Project Reality

- Navigation architecture refactor is documented in [docs/NAVIGATION_UPGRADE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/NAVIGATION_UPGRADE.md).
- The biggest remaining gap is not architecture but real Linux/Pi/hardware validation.
- `TODO.md` is the only open backlog.
- `ALTE_DATEIEN/` is archival only.

## Key Risks

### Operational risks

- Real Alfred/Pi validation remains the biggest open gap.
- Safety-relevant behavior still depends on real UART, GPS, IMU, charger, and watchdog timing.
- Build claims are only trustworthy from a fresh Linux/Pi build directory.

### Architecture risks

- `Robot` is still the orchestration hub for many concerns.
- Backend and frontend remain tightly coupled via telemetry and mission/map payloads.
- Hardware behavior can still invalidate assumptions that are clean in simulation.

## Documentation Status

### Current / source of truth

- `README.md`
- `PROJECT_OVERVIEW.md`
- `PROJECT_MAP.md`
- `TODO.md`
- `docs/NAVIGATION_UPGRADE.md`
- `docs/OP_STATE_MACHINE.md`
- `docs/TELEMETRY_CONTRACT.md`
- `docs/ROBOT_RUN_BASELINE.md`
- `docs/ALFRED_TEST_RUN_GUIDE.md`

### Reference / operational docs

- `docs/ALFRED_FLASHING.md`
- `docs/ALFRED_PI_SWITCHOVER_GUIDE.md`
- `docs/ALFRED_HARDWARE_ACCEPTANCE.md`
- `docs/MQTT.md`
- `docs/ROBOT_BUTTON_BUZZER_ERROR.md`

### Archived / not source of truth

- `ALTE_DATEIEN/`
- archived Vue frontend under `ALTE_DATEIEN/webui-vue-reference/`

## Recommended Reading Order

1. `README.md`
2. `PROJECT_OVERVIEW.md`
3. `PROJECT_MAP.md`
4. `TODO.md`
5. `main.cpp`
6. `core/Robot.cpp`
7. `core/op/Op.h`
8. `core/navigation/Map.h`
9. `docs/NAVIGATION_UPGRADE.md`
