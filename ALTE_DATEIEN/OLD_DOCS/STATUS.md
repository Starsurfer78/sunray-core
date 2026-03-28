# docs/STATUS.md — sunray-core Projektstatus

# Generiert: 2026-03-24 | Ersetzt: ANALYSIS_REPORT, BUG_REPORT, ARCHITECTURE, WEBUI_ARCHITECTURE

---

## BLOCKER vor A.9 (Hardware-Test) — P0

| ID      | Datei                           | Problem                                                                                                      | Fix                                                                            |
| ------- | ------------------------------- | ------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| BUG-004 | `SerialRobotDriver.cpp:246–250` | CRC: Code = Summe, Doku = XOR. Alfred-Firmware nutzt XOR → alle AT-Frames verworfen → Robot antwortet nicht. | `crc ^= c` statt `crc += c`. Erst Alfred-Original in `E:\TRAE\Sunray\` prüfen. |
| ARCH    | `core/op/GpsWaitFixOp.cpp`      | `ctx.hw.setMowMotor()` nicht in `HardwareInterface` → Build-Fehler auf Pi.                                   | `setMowMotor` → `setMotorPwm`                                                  |

---

## P1 — Hoch (vor erstem Feldeinsatz)

| ID      | Datei:Zeile               | Problem                                                                                         | Fix (1-2 Zeilen)                                                        |
| ------- | ------------------------- | ----------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| BUG-001 | `Robot.cpp:345,348`       | `activeOp()` → nullptr möglich → Crash in `checkBattery()`                                      | `Op* op = opMgr_.activeOp(); if (op) op->onBatteryUndervoltage(ctx);`   |
| BUG-002 | `Robot.cpp:342`           | `setBuzzer(false)` bei Critical-Battery — kein Alarm                                            | `setBuzzer(true)`                                                       |
| BUG-005 | `WebSocketServer.cpp:603` | `broadcastNmea()` aus Robot-Thread + Push-Loop aus serverThread\_ = Data Race → Heap-Korruption | NMEA in `std::queue<std::string>` + Mutex; serverThread\_ leert Queue   |
| BUG-008 | `useMowPath.ts:519–531`   | Bypass-Punkte nur oben/unten → Robot fährt durch horizontale No-Go-Zonen                        | Auch links/rechts testen: `[minX-margin, midY]` + `[maxX+margin, midY]` |
| BUG-010 | `MapEditor.vue:654–671`   | Doppelklick löst canvasClick 2× aus → spuriöser Vertex → jedes Polygon defekt                   | `if (e.detail >= 2) return` am Anfang von `canvasClick`                 |

---

## P2 — Mittel

| ID      | Datei:Zeile                     | Problem                                                                                          | Fix-Hinweis                                                               |
| ------- | ------------------------------- | ------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------- |
| BUG-003 | `SerialRobotDriver.cpp:329–331` | `fieldInt()` = signed int → Overflow nach ~248 Tagen (früher bei Schnellfahrt)                   | `std::stoul()` statt `std::stoi()`                                        |
| BUG-006 | `Robot.cpp:135,228`             | `gpsFixAge_ms = 9999999` hardcoded → GPS-Recovery-Timeout unzuverlässig                          | Echte Zeitdiff: `now_ms_ - gpsLastFixTime_ms_`                            |
| BUG-009 | `useMowPath.ts:682–684`         | K-Turn Overshoot: `stripLen * 2` statt `segLen * 0.4` → Wendemanöver verlässt Zone               | Overshoot = `Math.min(inset, segLen * 0.4)` konsistent mit generateStrips |
| BUG-012 | `MapEditor.vue:545–550`         | `zone.settings.pattern = 'spiral'` wird in computeMowPath() ignoriert → UI-Dropdown funktionslos | Spiral implementieren oder Dropdown deaktivieren                          |

---

## Niedrig

| ID      | Datei:Zeile               | Problem                                                                                                               |
| ------- | ------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| BUG-007 | `WebSocketServer.cpp:481` | `e.what()` unsanitized in JSON → invalid JSON bei Sonderzeichen. Fix: `nlohmann::json err; err["error"] = e.what();`  |
| BUG-011 | `MapEditor.vue:421`       | `Math.min(...xs)` Spread-Overflow bei >10k Waypoints → fitView() crasht. Fix: `xs.reduce((a,b) => a<b?a:b, Infinity)` |

---

## Testlücken (kritisch vor A.9)

- **Navigation: 0 Tests** — StateEstimator, LineTracker, Map → `tests/test_navigation.cpp` fehlt
- **Op-State-Machine: 0 direkte Tests** — checkBattery-Path, GpsWait-Timeout → `tests/test_op_machine.cpp` fehlt
- **WebUI-Algorithmen: 0 Tests** — computeMowPath, K-Turn, routeAroundExclusions → `useMowPath.test.ts` fehlt
- **CRC-Algorithmus** kann nur auf echter Hardware verifiziert werden (→ BUG-004)

---

## Phase-2-Stubs (akzeptiert für Phase 1)

- GPS-Fusion in StateEstimator: Stub, kein Komplementärfilter
- A\*-Pfadplanung in Map: Placeholder
- IMU: komplett entfernt, `onImuTilt()`/`onImuError()` in MowOp nie aufgerufen
- AT+W Waypoint-Batches: nicht implementiert → autonomes Mähen noch nicht möglich
- `gpsFixAge_ms`: hardcoded (→ BUG-006)
- Spiral-Pattern: nicht implementiert (→ BUG-012)

---

## Architektur-Status

| Bereich                   | Status                               |
| ------------------------- | ------------------------------------ |
| C++ Architektur (DI, HAL) | ✅ solide                            |
| Phase-1 Funktionsumfang   | ✅ vollständig                       |
| Infrastruktur-Tests       | ✅ ~87 Tests                         |
| Navigation-Tests          | ❌ 0 Tests                           |
| WebUI-Algorithmen         | ⚠️ einfache Gärten OK, komplex nicht |
| A.9 Hardware-Readiness    | ⚠️ 2 Blocker (BUG-004 + setMowMotor) |
