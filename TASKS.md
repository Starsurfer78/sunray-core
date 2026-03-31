# TASKS

Last updated: 2026-03-29

Dieses Dokument ist die zusammengeführte Aufgabenübersicht aus:

- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md)
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
- der bisherigen [`TASKS.md`](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md)

## Einordnung

- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md) ist fachlich abgelöst und verweist nur noch auf `TODO.md`.
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md) bleibt die detaillierte, taskgenaue Quelle inklusive `<!-- ctx: -->` Profilen.
- Diese `TASKS.md` ist die verdichtete Arbeits- und Prioritätenliste für Menschen.
- Die aktive WebUI-Basis ist `webui-svelte/`; die alte Vue-WebUI liegt nur noch als Referenz unter `ALTE_DATEIEN/webui-vue-reference/`.

## Aktueller Stand

- Native Architektur, Simulation, Navigation, Op-State-Machine, WebSocket-Server, MQTT und WebUI sind grundsätzlich vorhanden.
- Die lokale Build- und Testbasis ist gesund.
- Der größte verbleibende Unsicherheitsfaktor ist echter Hardwarebetrieb auf Raspberry Pi / Alfred.

Zuletzt verifiziert:

- `ctest --test-dir build_linux --output-on-failure`: 175/175 Tests grün
- `npm test` in `webui/`: 6/6 Tests grün

## Priorität 1

### A.9 Alfred Build-Test / reale Plattformvalidierung

Status: blockiert durch Pi-Zugang, aber fachlich höchstpriorisiert.

Offene Aufgaben:

- [ ] A.9-a: Kompilieren auf Raspberry Pi 4B
- [ ] A.9-b: Alfred fährt mit neuem Core identisch wie vorher
- [ ] A.9-c: Alle Unit Tests grün auf Pi

Warum das oben steht:

- Das ist die wichtigste Lücke zwischen lokal validierter Software und echter Einsatzfähigkeit.
- Hier hängen reale Aussagen zu UART, GPS, IMU, Ladeverhalten, Watchdog, Motorik und Shutdown-Semantik dran.

Wichtige Dateien:

- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
- [`main.cpp`](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [`docs/ALFRED_FLASHING.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_FLASHING.md)

## Priorität 2

### Alfred-Migrationslücken nach Originalvergleich

Diese Reihenfolge folgt direkt aus
[`ALFRED_MIGRATIONSPLAN.md`](/mnt/LappiDaten/Projekte/sunray-core/ALFRED_MIGRATIONSPLAN.md)
und schließt die wichtigsten Alfred-spezifischen Lücken nach der reinen
Pi-/Build-Validierung.

Offene Aufgaben:

- [x] Alfred-1: Button-Hold-Logic portieren und testen
- [x] Alfred-2: Battery-Fallbacks auf Originalwerte korrigieren
- [x] Alfred-3: Motorstrom-/Fault-Entscheidung treffen und umsetzen oder bewusst dokumentieren
- [ ] Alfred-4: echte Alfred-Hardwareabnahme inkl. Multiplexer/ADC/EEPROM durchführen und dokumentieren
- [ ] Alfred-5: NTRIP als echter Runtime-Pfad bewerten und priorisiert umsetzen
- [ ] Alfred-6: optionale Linux-Extras wie BLE, Audio, CAN, Kamera bewusst entscheiden
- [ ] Alfred-7: PID-Controller fuer geschlossene Radgeschwindigkeitsregelung planen und stufenweise einfuehren

Warum das hier steht:

- Diese Punkte schließen die wichtigsten Unterschiede zwischen Original-Alfred
  und `sunray-core`, ohne die modernere Architektur zurückzubauen.
- Die ersten vier Punkte betreffen reale Bedienbarkeit, Maschinenverhalten und
  Hardwareparität.
- NTRIP und Linux-Extras sind fachlich sinnvoll, aber nachgeordnet.

Aktueller Fortschritt:

- `Alfred-1` ist umgesetzt für die praxisrelevanten Hold-Aktionen `>=1s Stop`,
  `>=5s Dock`, `>=6s Mow`, `>=9s Shutdown` inklusive Sekunden-Beep und Tests.
- Die historischen Sonderpfade `3s R/C` und `12s WPS` sind damit noch nicht
  wiederhergestellt und bleiben Folgearbeit, falls dafür echte Anforderungen
  bestehen.
- `Alfred-2` ist umgesetzt: `Robot` nutzt als Fallback jetzt `25.5V` /
  `18.9V` statt der zu niedrigen Altwerte.
- `Alfred-3` ist entschieden als Variante A: Die Legacy-Current-/Fault-Parameter
  bleiben erhalten, sind aber jetzt explizit als aktuell ungenutzt markiert,
  weil die aktive Fault-Erkennung heute primär aus der MCU kommt.
- `Alfred-4` ist vorbereitet: Die Hardwareabnahme ist jetzt als
  [`docs/ALFRED_HARDWARE_ACCEPTANCE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_HARDWARE_ACCEPTANCE.md)
  konkret beschrieben. Offen bleibt die echte Ausfuehrung auf Pi/Alfred.
- `Alfred-7` ist jetzt als technischer Umsetzungsplan beschrieben in
  [`docs/PID_CONTROLLER_PLAN.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/PID_CONTROLLER_PLAN.md).
  Der Plan fuehrt den PID bewusst unterhalb von Stanley ein, nicht als Ersatz.
  Die erste Vorstufe ist bereits umgesetzt: Das bisherige direkte Open-Loop-
  Mapping aus `OpContext` steckt jetzt in einer eigenen
  `OpenLoopDriveController`-Klasse, noch ohne Verhaltensaenderung.

Wichtige Dateien:

- [`ALFRED_MIGRATIONSPLAN.md`](/mnt/LappiDaten/Projekte/sunray-core/ALFRED_MIGRATIONSPLAN.md)
- [`IST_SOLL_ANALYSE.md`](/mnt/LappiDaten/Projekte/sunray-core/IST_SOLL_ANALYSE.md)
- [`docs/ALFRED_HARDWARE_ACCEPTANCE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_HARDWARE_ACCEPTANCE.md)
- [`docs/PID_CONTROLLER_PLAN.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/PID_CONTROLLER_PLAN.md)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [`config.example.json`](/mnt/LappiDaten/Projekte/sunray-core/config.example.json)

## Priorität 3

### C.18 Missions-UX — Karten / Zonen / Missionen Redesign

Dieses Thema sollte vor den restlichen offenen C.16-UX-Punkten kommen, weil
es das fachliche Missionsmodell neu ordnet. Viele spätere Dashboard- und
Setup-Verbesserungen sollten darauf aufbauen statt vorher doppelt gebaut zu
werden.

Offene Aufgaben:

- [ ] C.18-a: Zone-Datenmodell erweitern
- [ ] C.18-b: Mission-Interface + Store anlegen
- [ ] C.18-c: Backend-API `/api/missions`
- [ ] C.18-d: Missions-Seite neu aufbauen
- [ ] C.18-e: Bahnvorschau-Canvas
- [ ] C.18-f: Zonen-Einstellungspanel
- [ ] C.18-g: Zeitplan je Mission
- [ ] C.18-h: Dashboard-Widget für Missionen
- [ ] C.18-i: `start` um `missionId` erweitern
- [ ] C.18-j: Telemetrie um Missions-Fortschritt erweitern

Wichtige Dateien:

- [`webui/design/missions_concept.html`](/mnt/LappiDaten/Projekte/sunray-core/webui/design/missions_concept.html)
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`core/WebSocketServer.h`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.h)
- [`core/op/MowOp.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp)
- [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)

## Priorität 4

### C.16 Benutzererlebnis / Nutzerführung

Der Block bleibt wichtig, sollte jetzt aber auf dem neuen Missionsmodell aus
`C.18` aufsetzen statt davor detailliert ausgebaut zu werden.

Offene Aufgaben:

- [ ] C.16-a: Dashboard-Preflight als echte Startfreigabe ergänzen
- [ ] C.16-c: Direkte Handlungsempfehlungen für Blocker und Warnzustände
- [ ] C.16-d: Fehler- und Recovery-Karten vereinheitlichen
- [ ] C.16-e: Geführten Erststart-/Setup-Flow anlegen
- [ ] C.16-f: Mapping-Workflow als Assistent denken
- [ ] C.16-g: Kartenerstellung vor Freigabe validieren
- [ ] C.16-i: Primäransicht klarer von Diagnoseansicht trennen

Wichtige Dateien:

- [`webui/src/views/Dashboard.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Dashboard.vue)
- [`webui/src/components/RobotSidebar.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/components/RobotSidebar.vue)
- [`webui/src/views/MapEditor.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/MapEditor.vue)
- [`webui/src/views/Diagnostics.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Diagnostics.vue)
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

## Priorität 5

### C.8-b Energie-Budget + Rückkehr-Berechnung

Offene Aufgabe:

- [ ] C.8-b: Energie-Budget + Rückkehr-Berechnung

Warum wichtig:

- Das ist fachlich relevant für sichere Missionsabbrüche, Docking-Entscheidungen und Verlässlichkeit im Feld.

Wichtige Dateien:

- [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)
- [`core/navigation/Map.h`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)

### Offene Integrationsfragen

- [ ] Q3: GPS no-motion Schwellenwert 0.05m ausreichend für RTK-Float?
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert?

Diese Punkte sind weniger reine Implementierungsaufgaben als offene Feld- und Integrationsthemen.

## Priorität 6

### B Pico-Driver

Startet explizit erst nach A.9.

Offene Aufgaben:

- [ ] B.1: `hal/PicoRobotDriver/PicoRobotDriver.h` Interface + Stub
- [ ] B.2: `hal/PicoRobotDriver/PicoRobotDriver.cpp` PWM-Ausgabe
- [ ] B.3: Hall-Sensor Odometrie in PicoRobotDriver
- [ ] B.4: `platform/INA226.h + .cpp` Strommessung I2C
- [ ] B.5: Pico-Firmware als separates Projekt

Wichtige Dateien:

- [`hal/HardwareInterface.h`](/mnt/LappiDaten/Projekte/sunray-core/hal/HardwareInterface.h)
- [`platform/I2C.h`](/mnt/LappiDaten/Projekte/sunray-core/platform/I2C.h)

## Priorität 7

### Größere Erweiterungen

Diese Themen sind sinnvoll, aber nicht auf dem kritischen Pfad.

#### E.3 Vision-basierte Navigation

- [ ] E.3-a: Kamera-Integration
- [ ] E.3-b: Visual Odometry
- [ ] E.3-c: Loop Closure

#### E.4 Flottenmanagement & Mesh

- [ ] E.4-a: MQTT-basiertes Mesh-Networking
- [ ] E.4-b: Task Splitting für mehrere Roboter
- [ ] E.4-c: Zentrales Dashboard für mehrere Roboter

#### E.5 Berührungslose Sensorik

- [ ] E.5-a: SonarDriver
- [ ] E.5-b: Safe-Stop via `nearObstacle`
- [ ] E.5-c: 3D-Sensing / Abgrunderkennung

#### P3.1 OTA Updates

- [ ] P3.1-a bis P3.1-h: OTA Manager, Signierung, Rollback, Resume, UI, Tests

#### P3.3 Adaptive Learning

- [ ] P3.3-a bis P3.3-d: Session-Metrics, TFLite, Online-Training, UI

#### P3.4 Mobile App

- [ ] P3.4-a: iOS App
- [ ] P3.4-b: Android App

## Architektur- und Betriebsrisiken

### 1. Robot als Risiko-Hub

[`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) trägt sehr viel Verantwortung gleichzeitig:

- Control Loop
- Telemetrieaufbau
- Sicherheitslogik
- Diagnosefluss
- Zeitplan-Handling
- GPS-Recovery
- Op-State-Machine-Verdrahtung

Folge:

- Änderungen an dieser Datei sind besonders regressionsanfällig.

### 2. Backend/Frontend-Vertrag ist eng gekoppelt

Die WebUI hängt direkt an:

- Telemetrieschlüsseln
- REST-Endpunkten
- WebSocket-Kommandos

Folge:

- Änderungen an [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp) und [`webui/src/composables/useTelemetry.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useTelemetry.ts) brauchen besondere Sorgfalt.

### Architektur-Reduktionsplan für die nächsten Commits

Vor Commit 1:

- aktuellen Telemetrie-Vertrag kurz fixieren, bevor `Robot::run()` angefasst wird
- Payload-Keys und ihre Bedeutung bewusst festhalten, damit Commit 1 nicht still das Frontend-Verhalten verändert
- Bestandsaufnahme von `Robot::run()` und aktuellen Testlücken schriftlich festhalten
- Baseline-Dokumente:
  - [`docs/ROBOT_RUN_BASELINE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ROBOT_RUN_BASELINE.md)
  - [`docs/TELEMETRY_CONTRACT.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/TELEMETRY_CONTRACT.md)

#### Commit 1 — `Robot` entlasten ohne Verhaltensänderung

Status: erledigt am 2026-03-28.

- `Robot::run()` in wenige klar benannte private Schritte zerlegen
- Telemetrieaufbau nur dann herausziehen, wenn vorher/nachher per Smoke-Test derselbe Output sichtbar bleibt
- vorhandene Robot-Tests vor oder zusammen mit dem Umbau stärken

Ergebnis:

- `Robot::run()` ist jetzt in klar benannte private Schritte zerlegt, ohne fachliche Ablaufänderung
- Telemetrie-Smoke-Test friert `Idle` und `NavToStart` weiterhin auf `op`, `state_phase`, `resume_target`, `event_reason` und `error_code` ein
- ein zusätzlicher Diag-Test hält fest, dass der Early-Return-Pfad weiterhin `controlLoops_++` überspringt

Änderungserlaubnis:

- erlaubt sind Struktur- und Lesbarkeitsänderungen
- nicht erlaubt sind neue Zustandsübergänge, geänderte Telemetrie-Keys oder neue fachliche Logik

Primäre Dateien:

- [`core/Robot.h`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [`tests/test_robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_robot.cpp)

#### Commit 2 — Telemetrie- und API-Vertrag explizit machen

Status: erledigt am 2026-03-28.

- Telemetrieformat kurz dokumentieren
- Contract-Tests für WebSocket/REST-Payloads ergänzen
- additive Änderungen bevorzugen, keine stillen Umbenennungen

Ergebnis:

- der Telemetrie-Vertrag ist in [`docs/TELEMETRY_CONTRACT.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/TELEMETRY_CONTRACT.md) explizit festgehalten
- der WebSocket-Contract-Test deckt jetzt den vollständigen gefrorenen Op-Satz inklusive `WaitRain` und `EscapeForward` ab
- das Frontend-Telemetry-Interface behandelt immer gelieferte Felder jetzt als required, statt still `undefined` zuzulassen

Änderungserlaubnis:

- erlaubt sind Dokumentation, Tests und additive Payload-Felder
- nicht erlaubt sind Breaking Changes an bestehenden Schlüsseln ohne explizite Gegenanpassung und Dokumentationsupdate

Primäre Dateien:

- [`core/WebSocketServer.h`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.h)
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`webui/src/composables/useTelemetry.ts`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/composables/useTelemetry.ts)
- [`tests/test_websocket_server.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_websocket_server.cpp)

#### Commit 3 — High-Risk-Szenarien absichern

Status: erledigt am 2026-03-28.

- gezielte Regressionstests für GPS-Verlust, Hindernis-Recovery, Docking-Retry und Resume
- Navigation nur noch zusammen mit Szenario-Tests ändern

Ergebnis:

- Escape-Recovery ist jetzt nicht nur beim Eintritt, sondern bis zur Rückkehr nach `Mow` abgesichert
- `resume_target` ist für den GPS-Recovery-Pfad szenariobasiert geprüft
- der Gegenbeweis für Zustände ohne Resume-Semantik ist als Fehlerfall-Telemetrie abgesichert

Änderungserlaubnis:

- erlaubt sind Tests und gezielte Stabilisierung an den betroffenen Pfaden
- nicht erlaubt sind größere Navigationsumbauten ohne begleitende Szenario-Tests

Pflicht nach jedem der drei Commits:

- [`TASKS.md`](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md) aktualisieren
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md) aktualisieren

Primäre Dateien:

- [`tests/test_op_machine.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_op_machine.cpp)
- [`tests/test_robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/tests/test_robot.cpp)
- [`core/navigation/Map.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- [`core/navigation/GridMap.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp)
- [`core/navigation/LineTracker.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

Status des Architektur-Reduktionsplans:

- Commit 1 abgeschlossen
- Commit 2 abgeschlossen
- Commit 3 abgeschlossen

### 3. Source-vs-generated Unschärfe im Repo

Im Repository liegen neben Quellcode auch generierte Inhalte:

- `build/`
- `build_gcc/`
- `build_linux/`
- `build_clang_ninja/`
- `webui/dist/`
- `webui/node_modules/`

Folge:

- Neue Mitarbeitende können schneller in Build-Artefakte statt in aktive Quellen greifen.

Status:

- `.gitignore` deckt die lokalen Build-Verzeichnisse und Frontend-Artefakte jetzt ab.
- Für saubere Builds soll auf Zielsystemen ein frisches Build-Verzeichnis verwendet werden, z. B. `build_pi/`.
- Bereits historisch getrackte Build-Artefakte sollten aus dem Git-Index entfernt bleiben.

Einordnung:

- Dieser Punkt ist technisch weitgehend gelöst.
- Offener Rest ist vor allem Repo-Hygiene im Commit-Verlauf und die konsequente Beibehaltung der neuen Regel.

### 4. Legacy-Material ist noch präsent

- `ALTE_DATEIEN/` ist als Hintergrundmaterial nützlich.
- Es ist aber nicht automatisch deckungsgleich mit dem aktuellen Code.

## Praktische Arbeitsreihenfolge

1. Alfred/Pi-Hardwarevalidierung abschließen.
2. UX-/Preflight-/Recovery-Themen aus C.16 angehen.
3. Energie-/Rückkehr-Logik fachlich schließen.
4. Danach Pico-Driver und weitere Hardwarepfade starten.
5. Größere Erweiterungen wie Vision, Mesh, OTA und Mobile erst anschließend.

## Wo die Details stehen

- Detaillierte Einzelaufgaben mit `ctx`-Profilen: [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
- Projektkontext und Architektur: [`PROJECT_OVERVIEW.md`](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_OVERVIEW.md)
- Dateistruktur und Orientierung: [`PROJECT_MAP.md`](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)

## Kurzfazit

`TASK.md` ist inhaltlich obsolet. Die echte Aufgabenquelle ist `TODO.md`. Diese `TASKS.md` ist jetzt die komprimierte Management- und Arbeitsübersicht darüber: zuerst reale Hardware absichern, dann UX und Betriebsreife verbessern, danach größere Erweiterungen angehen.
