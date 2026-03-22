# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

**sunray-core** is a clean C++17 rewrite of the Sunray robot mower firmware core.
Target hardware: Raspberry Pi 4B (mission planning, navigation, WebSocket server).
Companion MCU: STM32F103 (Alfred) or RP2040 Pico (Phase 2) — communicated via UART AT-frames.

**Design principles:**

- No Arduino includes or shims
- No global objects — Dependency Injection throughout
- Hardware behind `HardwareInterface` — Core never touches hardware directly
- `config.json` replaces `config.h` — no recompile for parameter changes
- Every module testable in isolation (Catch2)

---

## Build

**First time:**
```bash
mkdir -p sunray-core/build
cd sunray-core/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
```

**Subsequent builds:**
```bash
cd sunray-core/build && make -j4
```

**Run all tests:**
```bash
./tests/sunray_tests
```

**Run a single test tag:**
```bash
./tests/sunray_tests "[robot]"        # all tests tagged [robot]
./tests/sunray_tests "[battery]"      # specific sub-group
./tests/sunray_tests -l               # list all test names
```

**Run the binary:**
```bash
./sunray-core                          # Alfred hardware (SerialRobotDriver)
./sunray-core --sim                    # SimulationDriver (no hardware needed)
./sunray-core --sim /path/to/cfg.json  # custom config in sim mode
```

**WebUI (Vue 3 + Vite + Tailwind):**
```bash
cd sunray-core/webui
npm install        # first time
npm run dev        # dev server
npm run build      # produces webui/dist/ — served by Crow at /
```

CMake exports `compile_commands.json` automatically (for clangd/IDEs).

---

## Architecture

### HardwareInterface ([hal/HardwareInterface.h](hal/HardwareInterface.h))

The single boundary between Core and hardware. All drivers implement this.

```
Core → Hardware: setMotorPwm(), setBuzzer(), setLed(), keepPowerOn(), resetMotorFault()
Hardware → Core: readOdometry(), readSensors(), readBattery(), getCpuTemperature()
Lifecycle:       init() → loop: run() → readXxx()
```

Two implementations: `SerialRobotDriver` (Alfred/STM32, UART AT-frames) and `SimulationDriver` (kinematic model, no hardware).

### Dependency Injection

```cpp
Robot robot(
  std::make_unique<SerialRobotDriver>(config),  // or SimulationDriver
  config,
  logger
);
robot.setWebSocketServer(wsServer.get());
```

### Op State Machine ([core/op/Op.h](core/op/Op.h))

```
IdleOp → MowOp → DockOp → ChargeOp
              ↑ bumper/GPS loss → EscapeReverseOp / GpsWaitFixOp / ErrorOp
```

Each Op receives `OpContext&` per tick — no global state. `OpManager::tick()` calls `checkStop()` then `run()`. Transitions via `requestOp()` / `changeOp()`.

### Navigation ([core/navigation/](core/navigation/))

- **StateEstimator** — odometry dead-reckoning; GPS-update is a stub (Phase 2)
- **LineTracker** — Stanley controller (heading + lateral error)
- **Map** — polygon + waypoint storage; A\* path planning deferred to Phase 2

### Config ([core/Config.h](core/Config.h))

```cpp
auto cfg = std::make_shared<Config>("/etc/sunray/config.json");
double k = cfg->get<double>("stanley_k", 0.5);
cfg->set("stanley_k", 0.6);
cfg->save();
```

Copy `config.example.json` to `/etc/sunray/config.json` for deployment.

### WebSocket + HTTP Server ([core/WebSocketServer.h](core/WebSocketServer.h))

Crow + standalone Asio (both via FetchContent). Endpoints:
- `GET /ws/telemetry` — WebSocket, 10 Hz JSON push
- `GET|PUT /api/config` — live config read/write
- `GET|POST /api/map` — map file read/write
- `POST /api/sim/*` — bumper/gps/lift injection (sim mode only)
- `GET /` + `/assets/*` — serves `webui/dist/`

---

## Frozen Constraints (do not change without frontend update)

**Telemetry JSON format** — the WebSocket telemetry payload and op-name strings are frozen. The Python Mission Service and frontend parse these directly:
- Required fields: `op`, `x`, `y`, `battery_v`, `gps_text`, `bumper_l`, `bumper_r`, `motor_err`
- Op strings: `"Mow"`, `"Idle"`, `"Dock"`, `"Charge"`, `"Error"`, `"GpsWait"`, `"EscapeReverse"`
- WebSocket path: `/ws/telemetry`, keepalive: `{"type":"ping"}`

**WebUI design** — color scheme is locked to the "Dark Blue" reference in `webui/design/dashboard_reference.html`. Do not modify that file.

---

## Memory System

**Session start (mandatory):**

1. `.memory/module_index.md`
2. `.memory/decisions.md`
3. `TODO.md` — check current status before any work

**After each completed task:**

1. Mark `TODO.md` as `[x]`
2. Update `.memory/module_index.md` if new module added
3. Update `.memory/decisions.md` if architectural decision made
4. Commit with descriptive message

---

## Key Decisions (details in [.memory/decisions.md](.memory/decisions.md))

- Flat HardwareInterface — no artificial sub-driver split
- BUG-07 (PWM/encoder swap) stays in `SerialRobotDriver` — not in interface
- `nearObstacle = false` by default in `SensorData` (Alfred has no sonar)
- No Singleton pattern — everything via DI
- AT-frame protocol unchanged for Phase 1
- No-Go zones only handled by the Python path planner (Shapely) — Core ignores them at runtime

---

## Active Phase

**Phase 1 — A.1–A.7 complete.** A.8 (Alfred hardware build test) is on hold pending Pi access.
**Phase 2** (Pico-Driver) starts only after A.8 passes.
**Do not modify TODO.md section C** (Mission Service) — separate project.

---

## Testing Rules

- Every new module gets `tests/test_<module>.cpp`
- Tests must compile and pass before marking TODO as `[x]`
- Use MockHardware (implements `HardwareInterface`) for hardware-dependent tests — see [tests/test_robot.cpp](tests/test_robot.cpp) for the pattern
- Run `make -j4 && ./tests/sunray_tests` to verify

---

## What NOT to do

- No `#include <Arduino.h>` anywhere in `core/` or `hal/`
- No global variables (except the `g_robot` signal-handler pointer in `main.cpp`)
- No raw pointers for ownership — use `std::unique_ptr` / `std::shared_ptr`
- Do not rename telemetry JSON fields or op-name strings without a simultaneous frontend update
