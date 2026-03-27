# docs/STATUS.md — sunray-core Projektstatus

# Generiert: 2026-03-24 | Ersetzt: ANALYSIS_REPORT, BUG_REPORT, ARCHITECTURE, WEBUI_ARCHITECTURE

---

## BLOCKER vor A.9 (Hardware-Test) — P0

**Alle P0-Blocker beseitigt (2026-03-24).**

| ID      | Datei                           | Status   | Befund                                                                                                                                         |
| ------- | ------------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| BUG-004 | `SerialRobotDriver.cpp:246–250` | ✅ Kein Bug | Alfred-Firmware (`rm18.ino`) nutzt **additive Summe** (`crc += c`) — identisch mit unserem Code. XOR-Annahme in STATUS.md war falsch. Kein Fix nötig. |
| ARCH    | `core/op/GpsWaitFixOp.cpp`      | ✅ Bereits behoben | `ctx.stopMotors()` ruft intern `setMotorPwm(0,0,0)` auf. `setMowMotor` existiert nicht mehr im Code.                                         |

---

## P1 — Hoch ✅ Alle behoben (2026-03-24)

| ID      | Datei:Zeile               | Status | Behoben in                                                              |
| ------- | ------------------------- | ------ | ----------------------------------------------------------------------- |
| BUG-001 | `Robot.cpp:345,348`       | ✅     | nullptr-Guard `if (op)` im Code                                         |
| BUG-002 | `Robot.cpp:342`           | ✅     | `setBuzzer(true)` im Code                                               |
| BUG-005 | `WebSocketServer.cpp:603` | ✅     | NMEA-Queue `nmeaQueue_` + `nmeaMutex_`; nur serverThread_ sendet        |
| BUG-008 | `useMowPath.ts:519–535`   | ✅     | Links/Rechts-Bypass ergänzt (`left`/`right` Pt, BUG-008 fix)           |
| BUG-010 | `MapEditor.vue:627`       | ✅     | `if (e.detail >= 2) return` im Code                                     |

---

## P2 — Mittel ✅ Alle behoben (2026-03-24)

| ID      | Datei:Zeile                     | Status | Behoben in                                                                   |
| ------- | ------------------------------- | ------ | ---------------------------------------------------------------------------- |
| BUG-003 | `SerialRobotDriver.cpp:329–331` | ✅     | `fieldULong()` mit `std::stoul()` für Tick-Felder                            |
| BUG-006 | `Robot.cpp:135`                 | ✅     | `gpsLastFixTime_ms_` Member; `now_ms_ - gpsLastFixTime_ms_`                  |
| BUG-009 | `useMowPath.ts:682–684`         | ✅     | `Math.min(inset, stripLen * 2)` war korrekt (≡ `segLen * 0.4` mathematisch) |
| BUG-012 | `MapEditor.vue:1033`            | ✅     | Spiral-Option `disabled` + Tooltip "Noch nicht implementiert"                |

---

## Niedrig ✅ Alle behoben (2026-03-24)

| ID      | Datei:Zeile               | Status | Behoben in                                                       |
| ------- | ------------------------- | ------ | ---------------------------------------------------------------- |
| BUG-007 | `WebSocketServer.cpp:481` | ✅     | `nlohmann::json err; err["error"] = e.what(); err.dump()`        |
| BUG-011 | `MapEditor.vue:421`       | ✅     | `xs.reduce((a,b) => a<b?a:b, Infinity)` statt Spread             |

---

## Testlücken (kritisch vor A.9)

- **Navigation: 0 Tests** — StateEstimator, LineTracker, Map → `tests/test_navigation.cpp` fehlt
- **Op-State-Machine: 0 direkte Tests** — checkBattery-Path, GpsWait-Timeout → `tests/test_op_machine.cpp` fehlt
- **WebUI-Algorithmen: 0 Tests** — computeMowPath, K-Turn, routeAroundExclusions → `useMowPath.test.ts` fehlt
- **CRC-Algorithmus** ✅ per Code-Review verifiziert: additive Summe in sunray-core und Alfred identisch (2026-03-24)

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
| A.9 Hardware-Readiness    | ✅ Kein P0-Blocker mehr — wartet nur auf Pi-Zugang  |
| Alle Bugs behoben         | ✅ BUG-001–012 alle ✅ (2026-03-24)                 |
