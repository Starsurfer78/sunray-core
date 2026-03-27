# CLAUDE.md

C++17 rewrite of Sunray robot mower firmware.

- **Pi 4B:** Mission planning, navigation, WebSocket server (`sunray-core`)
- **STM32F103 Alfred:** Safety-critical, motor control, sensors — via UART AT-frames
- Workdir: `E:\TRAE\sunray-core\` | Read-only ref: `E:\TRAE\Sunray\` (never commit there)

**Design rules:** No Arduino. No globals. DI everywhere. Hardware behind `HardwareInterface`. `config.json` not `config.h`. Every module testable (Catch2).

---

## Build

```bash
cd build_gcc && make -j4                          # subsequent builds
cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j4     # first time (from build_gcc/)

./tests/sunray_tests                              # all tests
./sunray-core --sim                               # run without hardware

cd webui && npm run dev                           # WebUI dev server
```

---

## Architecture

**Layer order (no cycles):** `platform ← hal ← core ← main.cpp`

**HardwareInterface** (`hal/HardwareInterface.h`) — single Core↔Hardware boundary.
Implementations: `SerialRobotDriver` (UART AT-frames) | `SimulationDriver` (kinematic, no HW)

**Op State Machine:** `IdleOp → MowOp → DockOp → ChargeOp` + `EscapeReverseOp / GpsWaitFixOp / ErrorOp`

**Navigation:** `StateEstimator` | `LineTracker` (Stanley) | `Map` (polygon + waypoints + zones)

**WebSocket/HTTP (Crow):** `GET /ws/telemetry` 10 Hz | `GET|PUT /api/config` | `GET|POST /api/map`

Full reference: `docs/ARCHITECTURE.md` | WebUI: `docs/WEBUI_ARCHITECTURE.md`

---

## Frozen — never change without frontend update

- **Telemetry fields (15):** `type, op, x, y, heading, battery_v, charge_v, gps_sol, gps_text, gps_lat, gps_lon, bumper_l, bumper_r, motor_err, uptime_s`
- **Op strings:** `"Mow"`, `"Idle"`, `"Dock"`, `"Charge"`, `"Error"`, `"GpsWait"`, `"EscapeReverse"`
- **WS path:** `/ws/telemetry` — keepalive: `{"type":"ping"}`
- **Map JSON:** `perimeter/mow/dock/dockPath/exclusions/obstacles/zones/origin`
- **WebUI color scheme:** locked to `webui/design/dashboard_reference.html`

---

## Session Start — genau diese Reihenfolge, nichts anderes

1. Lies `TODO.md` — finde den Task der dir gegeben wurde
2. Lies das `<!-- ctx: -->` Profil direkt unter diesem Task
3. Lade **nur** die dort genannten `module:` Dateien aus `.memory/modules/`
4. Lies **nur** die dort genannten `files:` — nicht mehr
5. Lies `.memory/decisions.md` nur wenn das ctx-Profil `decisions` enthält
6. Implementiere **genau diesen einen Task** — dann stoppe

---

## One Task — Hard Stop

**Führe immer nur den explizit genannten Task aus. Nie mehr.**

- Task abgeschlossen → `[x]` setzen → Session beenden
- Nicht den nächsten Task antizipieren
- Nicht "solange ich schon dabei bin" weiterarbeiten
- Nicht mehrere Tasks in einer Session zusammenfassen
- Der Nutzer entscheidet wann der nächste Task startet

---

## Coding Rules

**Before:** List files you will modify. Read them first.
**During:** One TODO item per task. C++17. `#pragma once`. `std::unique_ptr` ownership, `std::shared_ptr` shared. Every public method: brief comment.
**After:**
1. Verify compile
2. Write/update `tests/test_<module>.cpp` — tests must pass
3. Mark `TODO.md` item `[x]`
4. Update `.memory/modules/<module>.md` if module changed
5. Update `.memory/decisions.md` if architectural decision made
6. Commit: `feat(module): desc` | `fix(module): desc` | `test(module): desc`
7. **Stoppe. Warte auf nächste Anweisung.**

---

## Constraints (non-negotiable)

- No `#include <Arduino.h>` in `core/` or `hal/`
- No global variables (except `g_robot` in `main.cpp`)
- No raw owning pointers
- `Config` always as `std::shared_ptr<Config>`
- `nearObstacle = false` default — Alfred has no sonar
- BUG-07 (PWM/encoder swap) in `SerialRobotDriver` only
- AT-frame protocol unchanged in Phase 1
- No-Go zones: Python path planner only — Core ignores at runtime

---

## Phase Discipline

- **Phase 1:** Section A of TODO.md only. A.9 on hold pending Pi access.
- **Phase 2:** Pico-Driver — starts only after A.9 passes.

---

## Error Handling

- Missing file → create it, don't assume
- Compile error → state and fix before continuing
- Blocked TODO → note blocker, skip to next
- Never leave broken code — stub with TODO comment if unsure
