# Project Map

## Core Runtime

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| Main loop | Process entry and runtime assembly | `main.cpp`, `core/Robot.cpp`, `core/Robot.h` | Fault propagation, hidden startup assumptions | What services or env vars are required in production? |
| Config and logging | Runtime parameters and structured logs | `core/Config.*`, `core/Logger.h` | Drift between defaults and real hardware | Which defaults are field-authoritative? |
| State machine | High-level robot operations | `core/op/*` | Incorrect transitions can create unsafe motion | Which transitions are contractual for the UI and HA? |
| Telemetry and APIs | WebSocket and HTTP surface | `core/WebSocketServer.*` | Command auth, stale state, serialization drift | What public API is considered stable? |
| MQTT | Remote telemetry and command path | `core/MqttClient.*` | Disconnect handling, retain misuse, stale command replay | Which topics are production-critical today? |

## Hardware Layer

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| HAL contracts | Shared hardware interface | `hal/HardwareInterface.h` | Interface changes ripple widely | Which HAL methods are hard real-time? |
| Alfred serial driver | Pi to MCU integration | `hal/SerialRobotDriver/*` | Safety-critical parsing and command mapping | Which fields are MCU truth vs Pi-derived? |
| Simulation driver | Test and software-only mode | `hal/SimulationDriver/*` | Simulation drift from hardware behavior | Which failure modes are still missing? |
| GPS driver | RTK/GNSS ingest | `hal/GpsDriver/*` | Fix handling, age calculation, failover | Which receiver configurations are deployed? |
| IMU driver | Orientation feed | `hal/Imu/*` | Calibration and drift assumptions | Which IMU board variant is final? |

## Motion And Navigation

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| State estimation | Odometry plus GPS plus IMU fusion | `core/navigation/StateEstimator.cpp` | Pose drift, wrong defaults, failover behavior | Which geometry values are field-calibrated? |
| Map and planner | Mission routing and local replans | `core/navigation/Map.*`, `Planner.*`, `GridMap.*`, `Costmap.*` | Boundary safety, dock approach failures | What dock corridor data is mandatory? |
| Tracking and control | Route following and drive commands | `LineTracker.*`, controllers under `core/control/*` | Overshoot, incorrect reverse behavior | What tuning is Alfred-specific vs generic? |

## Safety

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| Safety guards | Battery, GPS, perimeter, watchdog | `core/Robot.cpp` | Ordering bugs can mask stop requests | Is any guard intentionally delegated to STM32? |
| Error handling | Terminal safe state | `core/op/ErrorOp.cpp`, `Robot.cpp` | Partial stop without full fault latching | What should auto-recover and what must remain latched? |
| Obstacle recovery | Escape behavior | `core/op/EscapeReverseOp.cpp` | Re-entry into unsafe route | Are recovery timers field-validated? |

## Communication

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| Web UI backend | Local UI telemetry and commands | `core/WebSocketServer.*` | Auth and schema drift | Which endpoints are required for production UI? |
| MQTT bridge | Remote state integration | `core/MqttClient.*` | Heartbeat and reconnect correctness | What HA discovery model is intended? |
| Frontend | Browser UI | `webui-svelte/*` | UI assumptions drifting from backend semantics | Which workflows are already operator-facing? |

## Build And Config

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| Backend build | CMake and dependencies | root `CMakeLists.txt`, subdir `CMakeLists.txt` | Hidden network dependency via `FetchContent` | Should dependencies be vendored for field builds? |
| Frontend build | Node and Vite | `webui-svelte/package.json` | Node version drift | Is frontend build part of deployment pipeline? |
| Runtime config | JSON plus defaults | `config.example.json`, `core/Config.cpp` | Default mismatch vs deployed robots | Where do production configs live? |

## Tests And Tools

| Area | Role | Typical Files | Risks | Open Questions |
| --- | --- | --- | --- | --- |
| Unit and scenario tests | Runtime verification | `tests/*.cpp` | Tests can drift from real hardware semantics | Which scenarios still need hardware-backed tests? |
| Probes | Hardware bring-up helpers | `tools/*.cpp` | Operators may run probes on live hardware without procedure | Which probes are safe in deployed systems? |

## Entry Points

- Backend: `main.cpp`
- Frontend: `webui-svelte`
- Primary control object: `Robot`
- Primary operation dispatcher: `OpManager`

## High-Risk Files

- `core/Robot.cpp`
- `core/op/Op.cpp`
- `core/navigation/Map.cpp`
- `core/navigation/StateEstimator.cpp`
- `hal/SerialRobotDriver/SerialRobotDriver.cpp`
- `core/WebSocketServer.cpp`

## Unknowns

- Production systemd units and service order
- Exact Alfred hardware revision matrix
- Full mapping of GPIO and expanders in the field
