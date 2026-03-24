# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project

C++17 rewrite of Sunray robot mower firmware.

- **Pi 4B:** Mission planning, navigation, WebSocket server (`sunray-core`)
- **STM32F103 Alfred:** Safety-critical, motor control, sensors — via UART AT-frames
- Workdir: `E:\TRAE\sunray-core\` | Read-only ref: `E:\TRAE\Sunray\` (never commit there)

Design rules: No Arduino. No globals. DI everywhere. Hardware behind `HardwareInterface`. `config.json` not `config.h`. Every module testable (Catch2).

---

## Build

```bash
# First time
mkdir -p build_gcc && cd build_gcc
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j4

# Subsequent (active build dir is build_gcc/)
cd build_gcc && make -j4

# Tests (C++ — Catch2)
./tests/sunray_tests           # all
./tests/sunray_tests "[robot]" # by tag
./tests/sunray_tests -l        # list test names

# Run
./sunray-core          # hardware (SerialRobotDriver)
./sunray-core --sim    # SimulationDriver, no hardware needed

# WebUI (Vue 3 + Vite + Tailwind)
cd webui && npm install && npm run dev   # dev server
npm run build                            # → webui/dist/, served by Crow at /
npm run test                             # Vitest unit tests (useMowPath.test.ts)
```

CMake exports `compile_commands.json` (for clangd).

---

## Architecture

**Layer order (no cycles):** `platform ← hal ← core ← main.cpp`

**HardwareInterface** (`hal/HardwareInterface.h`) — single Core↔Hardware boundary.

- Core→HW: `setMotorPwm()`, `setBuzzer()`, `setLed()`, `keepPowerOn()`, `resetMotorFault()`
- HW→Core: `readOdometry()`, `readSensors()`, `readBattery()`, `getCpuTemperature()`
- Implementations: `SerialRobotDriver` (UART AT-frames) | `SimulationDriver` (kinematic, no HW)

**Op State Machine** (`core/op/Op.h`):
`IdleOp → MowOp → DockOp → ChargeOp` + `EscapeReverseOp / GpsWaitFixOp / ErrorOp`
Each Op gets `OpContext&` per tick. Transitions via `requestOp()` / `changeOp()`.

**GPS:** `UbloxGpsDriver` (ZED-F9P), background reader thread, UBX binary.
Integrated via `Robot::setGpsDriver()` setter (not in constructor).
Config keys: `gps_port`, `gps_baud`, `gps_configure`.

**MQTT:** `MqttClient` (`core/MqttClient.h/.cpp`) — optional telemetry push + remote commands.
Publishes op/state/gps under `{prefix}/*`, subscribes on `{prefix}/cmd`.
Enabled via config key `mqtt_enabled` (default: false).

**Navigation:** `StateEstimator` (odometry dead-reckoning + GPS stub) | `LineTracker` (Stanley controller) | `Map` (polygon + waypoints + zones)

**WebSocket/HTTP (Crow + Asio via FetchContent):**

- `GET /ws/telemetry` — 10 Hz JSON push
- `GET|PUT /api/config` | `GET|POST /api/map` | `GET|POST /api/map/geojson`
- `POST /api/sim/bumper|gps|lift|motor|imu_calib` — sim/test mode only
- `GET /` — serves `webui/dist/`

Full API and all config keys documented in `docs/ARCHITECTURE.md`.
WebUI view/composable/design details in `docs/WEBUI_ARCHITECTURE.md`.

---

## Frozen — do not change without frontend update

**Telemetry fields (15):** `type, op, x, y, heading, battery_v, charge_v, gps_sol, gps_text, gps_lat, gps_lon, bumper_l, bumper_r, motor_err, uptime_s`
**Op strings:** `"Mow"`, `"Idle"`, `"Dock"`, `"Charge"`, `"Error"`, `"GpsWait"`, `"EscapeReverse"`
**WS path:** `/ws/telemetry` — keepalive: `{"type":"ping"}`
**Map JSON format:** `perimeter/mow/dock/dockPath/exclusions/obstacles/zones/origin` — parsed by Mission Service.
**MowPt fields:** `{p: [x,y], rev?: bool, slow?: bool}` — K-Turn encoding, must match C++ `MowPoint` struct.
**WebUI color scheme:** locked to `webui/design/dashboard_reference.html` — never modify.

---

## Session Start — mandatory, in order

1. `docs/STATUS.md` — scan open bugs relevant to your task
2. `.memory/decisions.md` — always required
3. `TODO.md` — find next open `[ ]` in Section A
4. `.memory/module_index.md` — only if touching a module new to this session

---

## Coding Rules

**Before:** List files you will modify. Read them first. Files >200 lines: interface + relevant functions only.
**During:** One TODO item per task. C++17 only. `#pragma once` on every header. `std::unique_ptr` for ownership, `std::shared_ptr` for shared. Every public method: brief comment.
**After:**

1. Verify compile (check includes, types, signatures)
2. Write/update `tests/test_<module>.cpp` — tests must pass before marking done
3. Mark `TODO.md` `[x]`
4. Update `.memory/module_index.md` if new module added
5. Update `.memory/decisions.md` if architectural decision made
6. Commit: `feat(module): description` | `fix(module): description` | `test(module): description`

---

## Subagents — use sparingly

**Use ONLY when ALL conditions true:**

- File >500 lines AND deep understanding required
- Task requires reading >4 files simultaneously
- Porting a large module (Map.cpp, StateEstimator.cpp)

**Never use for:** Files <500 lines, simple reads, new modules <150 lines, config/platform/test work.
**When used:** One specific question, max 3 files, no global context.

---

## Constraints (non-negotiable)

- No `#include <Arduino.h>` in `core/` or `hal/`
- No global variables (except `g_robot` signal-handler pointer in `main.cpp`)
- No raw owning pointers
- `Config` always as `std::shared_ptr<Config>` — never global
- `nearObstacle = false` default — Alfred has no sonar
- BUG-07 (PWM/encoder swap) in `SerialRobotDriver` only — never in interface
- AT-frame protocol unchanged in Phase 1
- No-Go zones handled by Python path planner only — Core ignores at runtime

---

## Phase Discipline

- **Phase 1:** Section A of TODO.md only. A.9 (Alfred hardware test) on hold pending Pi access.
- **Phase 2:** Pico-Driver. Starts only after A.9 passes.
- **Section C (Mission Service):** Separate project — never touch.

---

## Decision Protocol

When facing a design choice: state options, pick simpler consistent option.
Write to `.memory/decisions.md`:

```
## [date] Decision: <title>
**Choice:** <what>  **Reason:** <one sentence>  **Rejected:** <alternative + why not>
```

---

## Error Handling

- Missing file → create it, don't assume
- Compile error likely → state and fix before continuing
- Blocked TODO → note blocker in TODO.md, skip to next
- Never leave broken code — write stub with TODO comment if unsure
