# TASKS

Last updated: 2026-03-28

Dieses Dokument ist die zusammengeführte Aufgabenübersicht aus:

- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md)
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md)
- der bisherigen [`TASKS.md`](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md)

## Einordnung

- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md) ist fachlich abgelöst und verweist nur noch auf `TODO.md`.
- [`TODO.md`](/mnt/LappiDaten/Projekte/sunray-core/TODO.md) bleibt die detaillierte, taskgenaue Quelle inklusive `<!-- ctx: -->` Profilen.
- Diese `TASKS.md` ist die verdichtete Arbeits- und Prioritätenliste für Menschen.

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

### C.16 Benutzererlebnis / Nutzerführung

Das ist der sichtbarste offene Produktblock und der naheliegendste nächste Schritt nach Hardware-Validierung.

Offene Aufgaben:

- [ ] C.16-a: Dashboard-Preflight als echte Startfreigabe ergänzen
- [ ] C.16-b: Nutzerfreundliche Statussprache statt interner Op-Namen
- [ ] C.16-c: Direkte Handlungsempfehlungen für Blocker und Warnzustände
- [ ] C.16-d: Fehler- und Recovery-Karten vereinheitlichen
- [ ] C.16-e: Geführten Erststart-/Setup-Flow anlegen
- [ ] C.16-f: Mapping-Workflow als Assistent denken
- [ ] C.16-g: Kartenerstellung vor Freigabe validieren
- [ ] C.16-h: Ereignis- und Missionshistorie verständlich aufbauen
- [ ] C.16-i: Primäransicht klarer von Diagnoseansicht trennen
- [ ] C.16-j: UX-nahe Ereignistimeline ergänzen

Wichtige Dateien:

- [`webui/src/views/Dashboard.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Dashboard.vue)
- [`webui/src/components/RobotSidebar.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/components/RobotSidebar.vue)
- [`webui/src/views/MapEditor.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/MapEditor.vue)
- [`webui/src/views/Diagnostics.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Diagnostics.vue)
- [`webui/src/views/History.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/History.vue)
- [`core/WebSocketServer.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

## Priorität 3

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

## Priorität 4

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

## Priorität 5

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
