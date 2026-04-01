# PROJECT_MAP

Last updated: 2026-04-01

## Top-Level Layout

```text
sunray-core/
├── main.cpp                 native runtime entry point
├── CMakeLists.txt           top-level native build
├── config.example.json      deployment config template
├── core/                    platform-independent runtime
├── hal/                     hardware abstraction and concrete drivers
├── platform/                Linux/POSIX adapters
├── tests/                   Catch2 native tests
├── webui-svelte/            active Svelte frontend
├── docs/                    active project docs
├── alfred/                  MCU/board-specific firmware assets
├── scripts/                 workflow and hardware support scripts/docs
├── tools/                   helper binaries
├── ALTE_DATEIEN/            archived docs and legacy Vue frontend
├── build*/                  generated native build trees
└── webui-svelte/dist,node_modules  generated frontend artifacts
```

## Active Source Tree

### Native runtime

#### `core/`

- `Config.*`
  JSON-backed runtime config loading and saving
- `Robot.*`
  main control-loop orchestrator
- `WebSocketServer.*`
  HTTP/WebSocket server and static-file serving
- `MqttClient.*`
  optional MQTT adapter
- `Schedule.*`
  weekly mowing schedule persistence and checks
- `storage/`
  event/session recording and SQLite-backed history
- `control/`
  drive-related control logic

#### `core/op/`

- `Op.h`, `Op.cpp`
  state-machine framework and `OpManager`
- state ops:
  `IdleOp.cpp`, `UndockOp.cpp`, `NavToStartOp.cpp`, `MowOp.cpp`, `DockOp.cpp`,
  `ChargeOp.cpp`, `WaitRainOp.cpp`, `GpsWaitFixOp.cpp`, `EscapeReverseOp.cpp`,
  `EscapeForwardOp.cpp`, `ErrorOp.cpp`

Reference doc:

- `docs/OP_STATE_MACHINE.md`

#### `core/navigation/`

- `StateEstimator.*`
  pose estimation from odometry, GPS, and IMU
- `Map.*`
  perimeter, exclusions, zones, docking route, mission routing, and obstacle state
- `Route.h`
  shared route/segment data types
- `PlannerContext.h`, `Planner.*`
  planner façade and planning inputs
- `Costmap.*`
  local planning costmap with separated layers
- `GridMap.*`
  search/raster planning implementation
- `LineTracker.*`
  path/segment following

### Hardware and platform

#### `hal/`

- `HardwareInterface.h`
  core-to-hardware contract
- `SerialRobotDriver/`
  Alfred hardware driver over UART and I2C
- `SimulationDriver/`
  software-only implementation
- `GpsDriver/`
  GPS abstractions and u-blox driver
- `Imu/`
  MPU6050 support

#### `platform/`

- `Serial.*`
- `I2C.*`
- mux / port-expander helpers

### Frontend

#### `webui-svelte/src/`

- `main.ts`
- `App.svelte`
- `lib/pages/`
  `Dashboard.svelte`, `Map.svelte`, `Mission.svelte`, `Diagnostics.svelte`
- `lib/api/`
  REST and WebSocket integration
- `lib/stores/`
  telemetry, map, mission, and connection state
- `lib/components/`
  dashboard, diagnostics, map, and mission UI

## Build And Tooling Files

### Native

- `CMakeLists.txt`
- `core/CMakeLists.txt`
- `hal/CMakeLists.txt`
- `platform/CMakeLists.txt`
- `tests/CMakeLists.txt`

### Frontend

- `webui-svelte/package.json`
- `webui-svelte/vite.config.ts`
- `webui-svelte/tsconfig.json`
- `webui-svelte/README.md`

## Documentation Map

### Source of truth

- `README.md`
- `PROJECT_OVERVIEW.md`
- `PROJECT_MAP.md`
- `TODO.md`

### Core fachliche Doku

- `docs/NAVIGATION_UPGRADE.md`
- `docs/OP_STATE_MACHINE.md`
- `docs/TELEMETRY_CONTRACT.md`
- `docs/ROBOT_RUN_BASELINE.md`

### Betrieb / Hardware

- `docs/ALFRED_TEST_RUN_GUIDE.md`
- `docs/ALFRED_FLASHING.md`
- `docs/ALFRED_PI_SWITCHOVER_GUIDE.md`
- `docs/ALFRED_HARDWARE_ACCEPTANCE.md`
- `docs/ROBOT_BUTTON_BUZZER_ERROR.md`

### Additional reference docs

- `docs/MQTT.md`
- `docs/HISTORY_STATS_ARCHITECTURE.md`
- `docs/USER_EXPERIENCE_IMPROVEMENTS.md`

### Archived

- `ALTE_DATEIEN/`

## Generated / Non-Authoritative Areas

These directories matter for local work, but they are not primary source:

- `build/`
- `build_gcc/`
- `build_linux/`
- `build_clang_ninja/`
- `build_scan/`
- `webui-svelte/dist/`
- `webui-svelte/node_modules/`

Working rule:

- prefer a fresh build directory per machine or toolchain
- do not treat checked-in build outputs as portable inputs

## Where To Look For Common Questions

### "How does the robot start?"

- `main.cpp`
- `core/Robot.cpp`

### "How do state transitions work?"

- `core/op/Op.h`
- `core/op/Op.cpp`
- `docs/OP_STATE_MACHINE.md`

### "How does navigation work?"

- `core/navigation/StateEstimator.cpp`
- `core/navigation/Map.cpp`
- `core/navigation/Planner.cpp`
- `core/navigation/Costmap.cpp`
- `core/navigation/GridMap.cpp`
- `core/navigation/LineTracker.cpp`
- `docs/NAVIGATION_UPGRADE.md`

### "How does the UI talk to the backend?"

- `core/WebSocketServer.cpp`
- `webui-svelte/src/lib/api/websocket.ts`
- `webui-svelte/src/lib/api/rest.ts`
- `webui-svelte/src/lib/api/types.ts`

### "Where is the current backlog?"

- `TODO.md`
