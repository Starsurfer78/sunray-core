# PROJECT_MAP

Last updated: 2026-03-28

## Top-Level Layout

```text
sunray-core/
├── main.cpp                 Native runtime entry point
├── CMakeLists.txt           Top-level native build
├── config.example.json      Deployment config template
├── core/                    Platform-independent runtime
├── hal/                     Hardware abstraction and concrete drivers
├── platform/                Linux/POSIX adapters
├── tests/                   Catch2 native tests
├── webui/                   Vue 3 + TypeScript frontend
├── docs/                    Active project docs
├── alfred/                  MCU/board-specific firmware assets
├── scripts/                 Workflow and hardware support scripts/docs
├── .memory/                 Project memory/index material
├── ALTE_DATEIEN/            Archived / legacy docs
├── build*/                  Generated native build trees
└── webui/dist,node_modules  Generated frontend artifacts
```

## Active Source Tree

### Native runtime

#### [`core/`](/mnt/LappiDaten/Projekte/sunray-core/core)

- `Config.*`
  JSON-backed runtime config loading and saving.
- `Logger.h`
  Minimal logging abstraction.
- `Robot.*`
  Main control-loop orchestrator.
- `WebSocketServer.*`
  Crow-based HTTP/WebSocket server and static-file serving.
- `MqttClient.*`
  Optional MQTT adapter.
- `Schedule.*`
  Weekly mowing schedule persistence and checks.
- `RobotConstants.h`
  Compile-time constants.

#### [`core/op/`](/mnt/LappiDaten/Projekte/sunray-core/core/op)

- `Op.h`, `Op.cpp`
  State-machine framework and `OpManager`.
- `IdleOp.cpp`
- `UndockOp.cpp`
- `NavToStartOp.cpp`
- `MowOp.cpp`
- `DockOp.cpp`
- `ChargeOp.cpp`
- `WaitRainOp.cpp`
- `GpsWaitFixOp.cpp`
- `EscapeReverseOp.cpp`
- `ErrorOp.cpp`

Reference doc:

- [`docs/OP_STATE_MACHINE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md)

#### [`core/navigation/`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation)

- `StateEstimator.*`
  EKF-like pose estimation from odometry, GPS, and IMU.
- `Map.*`
  Perimeter, zones, obstacles, mowing route, docking route.
- `LineTracker.*`
  Path tracking / waypoint following.
- `GridMap.*`
  Local occupancy grid and A* replanning.

### Hardware and platform

#### [`hal/`](/mnt/LappiDaten/Projekte/sunray-core/hal)

- `HardwareInterface.h`
  Core-to-hardware contract.
- `SerialRobotDriver/`
  Alfred hardware driver over UART and I2C.
- `SimulationDriver/`
  Software-only hardware implementation for local dev and tests.
- `GpsDriver/`
  GPS abstractions and u-blox concrete driver.
- `Imu/`
  MPU6050 IMU support.

#### [`platform/`](/mnt/LappiDaten/Projekte/sunray-core/platform)

- `Serial.*`
  POSIX serial wrapper.
- `I2C.*`
  Linux `i2c-dev` wrapper.
- `PortExpander.*`
  PCA9555 helpers for LEDs, buzzer, fan, IMU power.

### Frontend

#### [`webui/src/`](/mnt/LappiDaten/Projekte/sunray-core/webui/src)

- `main.ts`
  Vue bootstrap and automatic bearer-token injection for `/api/*`.
- `App.vue`
  App shell.
- `router/index.ts`
  Route table.
- `style.css`
  Global styles.

#### Views

- [`webui/src/views/Dashboard.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Dashboard.vue)
- [`webui/src/views/MapEditor.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/MapEditor.vue)
- [`webui/src/views/Scheduler.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Scheduler.vue)
- [`webui/src/views/History.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/History.vue)
- [`webui/src/views/Statistics.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Statistics.vue)
- [`webui/src/views/Diagnostics.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Diagnostics.vue)
- [`webui/src/views/Simulator.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Simulator.vue)
- [`webui/src/views/Settings.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Settings.vue)

#### Frontend shared logic

- [`webui/src/composables/useTelemetry.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useTelemetry.ts)
- [`webui/src/composables/useSessionTracker.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useSessionTracker.ts)
- [`webui/src/composables/useMowPath.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useMowPath.ts)

#### Frontend components

- `RobotMap.vue`
- `RobotSidebar.vue`
- `LogPanel.vue`
- `VirtualJoystick.vue`

## Build and Tooling Files

### Native

- [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt)
- [`core/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/core/CMakeLists.txt)
- [`hal/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/hal/CMakeLists.txt)
- [`platform/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/platform/CMakeLists.txt)
- [`tests/CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/tests/CMakeLists.txt)

### Frontend

- [`webui/package.json`](/mnt/LappiDaten/Projekte/sunray-core/webui/package.json)
- [`webui/package-lock.json`](/mnt/LappiDaten/Projekte/sunray-core/webui/package-lock.json)
- [`webui/vite.config.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/vite.config.ts)
- [`webui/tsconfig.node.json`](/mnt/LappiDaten/Projekte/sunray-core/webui/tsconfig.node.json)

## Documentation and Status Files

### Current/useful

- [`README.md`](/mnt/LappiDaten/Projekte/sunray-core/README.md)
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
- [`docs/OP_STATE_MACHINE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md)
- [`docs/USER_EXPERIENCE_IMPROVEMENTS.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/USER_EXPERIENCE_IMPROVEMENTS.md)
- [`docs/ALFRED_FLASHING.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_FLASHING.md)
- [`webui/README.md`](/mnt/LappiDaten/Projekte/sunray-core/webui/README.md)

### Project-assistant material

- [`AGENTS.md`](/mnt/LappiDaten/Projekte/sunray-core/AGENTS.md)
- [`CLAUDE.md`](/mnt/LappiDaten/Projekte/sunray-core/CLAUDE.md)
- [`WORKING_RULES.md`](/mnt/LappiDaten/Projekte/sunray-core/WORKING_RULES.md)
- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md)
- [`scripts/workflow.md`](/mnt/LappiDaten/Projekte/sunray-core/scripts/workflow.md)
- [`.memory/module_index.md`](/mnt/LappiDaten/Projekte/sunray-core/.memory/module_index.md)

### Historical / archival

- [`ALTE_DATEIEN/`](/mnt/LappiDaten/Projekte/sunray-core/ALTE_DATEIEN)

Treat this folder as reference material, not as the source of truth for current runtime behavior.

## Generated / Non-Authoritative Areas

These directories are important for local work, but they are not primary source:

- [`build/`](/mnt/LappiDaten/Projekte/sunray-core/build)
- [`build_gcc/`](/mnt/LappiDaten/Projekte/sunray-core/build_gcc)
- [`build_linux/`](/mnt/LappiDaten/Projekte/sunray-core/build_linux)
- [`build_clang_ninja/`](/mnt/LappiDaten/Projekte/sunray-core/build_clang_ninja)
- [`webui/dist/`](/mnt/LappiDaten/Projekte/sunray-core/webui/dist)
- [`webui/node_modules/`](/mnt/LappiDaten/Projekte/sunray-core/webui/node_modules)

Working rule:

- create a fresh build directory per machine or toolchain when possible, for example `build_pi/` on Raspberry Pi
- do not treat existing build outputs as portable project inputs
- when in doubt, rebuild from source instead of reusing generated artifacts

## Where To Look For Common Questions

### "How does the robot start?"

- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

### "How do hardware calls cross into the core?"

- [`hal/HardwareInterface.h`](/mnt/LappiDaten/Projekte/sunray-core/hal/HardwareInterface.h)
- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [`hal/SimulationDriver/SimulationDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SimulationDriver/SimulationDriver.cpp)

### "How do state transitions work?"

- [`core/op/Op.h`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h)
- [`core/op/Op.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.cpp)
- [`docs/OP_STATE_MACHINE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md)

### "How does navigation work?"

- [`core/navigation/StateEstimator.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp)
- [`core/navigation/Map.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- [`core/navigation/GridMap.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp)
- [`core/navigation/LineTracker.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

### "How does the UI talk to the backend?"

- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`webui/src/composables/useTelemetry.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useTelemetry.ts)
- [`webui/vite.config.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/vite.config.ts)

### "Where is the current backlog?"

- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)

## Suggested Mental Model

Use this dependency path when navigating:

```text
main.cpp
  -> Config / Logger
  -> HardwareInterface implementation
  -> Robot
     -> OpManager + Ops
     -> StateEstimator / Map / LineTracker / GridMap
     -> WebSocketServer / MqttClient
  -> webui/dist served by backend
```
