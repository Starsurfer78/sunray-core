# TODO

Last updated: 2026-03-28

Dieses Dokument ist der detaillierte Aufgaben-Backlog für `sunray-core`.

- Die einzelnen Task-IDs hier sind der detaillierte Backlog.
- [`TASKS.md`](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md) ist die verdichtete Übersichts- und Prioritätenversion.
- [`TASK.md`](/mnt/LappiDaten/Projekte/sunray-core/TASK.md) ist inhaltlich abgelöst.
- `CLAUDE.md` verweist für den Arbeitskontext nur noch auf:
  - [`PROJECT_OVERVIEW.md`](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_OVERVIEW.md)
  - [`PROJECT_MAP.md`](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)
  - [`WORKING_RULES.md`](/mnt/LappiDaten/Projekte/sunray-core/WORKING_RULES.md)
  - [`TASKS.md`](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md)

> **Hinweis:** Die vorhandenen `<!-- ctx: -->` Profile sind Legacy-Metadaten aus einem älteren Claude-Workflow.
> Sie können als grobe Orientierung nützlich sein, sind aber nicht mehr der primäre Einstiegspunkt.
> Maßgeblich für den Arbeitskontext sind jetzt `PROJECT_OVERVIEW.md`, `PROJECT_MAP.md`, `WORKING_RULES.md` und `TASKS.md`.

---

## Erledigte Meilensteine

- [x] A.1–A.8, A.10 — C++ Fundament, SerialRobotDriver, Robot+DI, SimulationDriver, Op-State-Machine, Navigation, WebSocket-Server, Konfiguration, GPS-Treiber, Pi-Version
- [x] C.1–C.5, C.7, C.9–C.12 — WebUI, MQTT-Client, On-The-Fly Obstacles, Dashboard, Diagnose, Zeitplan, Zonen-Auswahl
- [x] P0 Blocker (A.9) — STM32 Flashen via Pi, CRC-Verifikation, Motor-API
- [x] 2026-03-28 Architektur Commit 1 — `Robot::run()` in private Schritte zerlegt, Telemetrie-Smoke-Test ergänzt, Diag-Early-Return gegen stilles Drift abgesichert
- [x] 2026-03-28 Architektur Commit 2 — Telemetrie-Vertrag explizit dokumentiert, WebSocket-Op-Contract vervollständigt, Frontend-Typen an immer gelieferte Felder angepasst
- [x] 2026-03-28 Architektur Commit 3 — GPS-/Escape-/Resume-Szenarien per Regressionstests ergänzt und gegen stille Zustandsdrifts abgesichert

---

## A — Core- und Plattformarbeit

### A.9 Alfred Build-Test ⏸ wartet auf Pi-Zugang

- [ ] A.9-a: Kompilieren auf Raspberry Pi 4B
  <!-- ctx: module:serial_robot_driver, module:hardware_interface | files:CMakeLists.txt | model:haiku -->
- [ ] A.9-b: Alfred fährt mit neuem Core identisch wie vorher
  <!-- ctx: module:serial_robot_driver, module:robot | files:hal/SerialRobotDriver/SerialRobotDriver.cpp | model:sonnet -->
- [ ] A.9-c: Alle Unit Tests grün auf Pi
  <!-- ctx: module:robot, module:simulation_driver | files:tests/ | model:haiku -->

---

## B — Pico-Driver ⏸ startet erst nach A.9

- [ ] B.1: `hal/PicoRobotDriver/PicoRobotDriver.h` — Interface + Stub
  <!-- ctx: module:hardware_interface | files:hal/HardwareInterface.h | model:haiku -->
- [ ] B.2: `hal/PicoRobotDriver/PicoRobotDriver.cpp` — PWM-Ausgabe (1–20 kHz) für BLDC-Controller
  <!-- ctx: module:hardware_interface, module:serial_robot_driver | files:hal/PicoRobotDriver/PicoRobotDriver.h | model:sonnet -->
- [ ] B.3: Hall-Sensor Odometrie (GPIO-Interrupt) in PicoRobotDriver
  <!-- ctx: module:hardware_interface | files:hal/PicoRobotDriver/PicoRobotDriver.h, hal/PicoRobotDriver/PicoRobotDriver.cpp | model:sonnet -->
- [ ] B.4: `platform/INA226.h + .cpp` — Strommessung I2C
  <!-- ctx: module:i2c | files:platform/I2C.h | model:haiku -->
- [ ] B.5: Pico-Firmware (separates Projekt, Pico SDK C++)
  <!-- ctx: module:hardware_interface | files:hal/PicoRobotDriver/PicoRobotDriver.h | model:sonnet -->

---

## C — Erweiterte Funktionen

### C.13 IMU-Integration ✅ Abgeschlossen (2026-03-24)

- [x] `hal/Imu/Mpu6050Driver.h + .cpp`
- [x] `Robot::run()` IMU-Tick
- [x] `TelemetryData` imu_heading/pitch/roll
- [x] StateEstimator Kalmanfilter GPS + Odo + IMU
- [x] `POST /api/diag/imu_calib`
- [x] Diagnostics.vue Kompassrose

### C.8 Später

- [x] C.8-a: WebUI auf C++ WebSocket-Server umstellen
  <!-- ctx: module:websocket_server, module:webui | files:core/WebSocketServer.h | model:sonnet -->
- [ ] C.8-b: Energie-Budget + Rückkehr-Berechnung
  <!-- ctx: module:robot, module:navigation | files:core/Robot.h, core/navigation/Map.h | model:sonnet -->

### C.14 RTK-gestützte Punktaufnahme / Kartenaufnahme

- [x] C.14-a: Neuer Kartenaufnahme-Flow in der WebUI: "Neue Karte starten" + Punktaufnahme-Modus
  <!-- ctx: module:webui, module:websocket_server | files:webui/src/views/Dashboard.vue, webui/src/views/MapEditor.vue, core/WebSocketServer.cpp | model:sonnet -->
- [x] C.14-b: Dashboard-Button "Aktuellen GPS-Punkt speichern" für manuelle Grenzaufnahme
  <!-- ctx: module:webui, module:websocket_server | files:webui/src/views/Dashboard.vue, webui/src/composables/useTelemetry.ts, core/WebSocketServer.cpp | model:sonnet -->
- [x] C.14-c: Punktaufnahme mit RTK-abhängiger UI-Entscheidung: FIX direkt speichern, FLOAT nur nach Bestätigung, INVALID klar blockieren
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, core/WebSocketServer.cpp, core/Robot.cpp | model:haiku -->
- [x] C.14-d: 3s-Sampling mit Mittelwertbildung vor Punkt-Commit, statt Sofort-Speichern
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, core/WebSocketServer.cpp, core/Robot.h, core/Robot.cpp | model:sonnet -->
- [x] C.14-e: Sampling-Fortschritt 0–100% im UI anzeigen; erfolgreicher Punkt erscheint sofort als Marker
  <!-- ctx: module:webui | files:webui/src/views/Dashboard.vue, webui/src/views/MapEditor.vue | model:haiku -->
- [x] C.14-f: Aufgenommene Punkte als `perimeter`-Punkte in die aktuelle Karte übernehmen und via `/api/map` persistieren
  <!-- ctx: module:webui, module:websocket_server, module:navigation | files:webui/src/views/Dashboard.vue, core/WebSocketServer.cpp, core/navigation/Map.h | model:sonnet -->
- [x] C.14-g: Kartenaufnahme-Metadaten vorsehen: `fix_duration_ms`, `sample_variance`, `mean_accuracy`
  <!-- ctx: module:websocket_server, module:robot | files:core/WebSocketServer.h, core/WebSocketServer.cpp, core/Robot.h | model:sonnet -->
- [x] C.14-h: Karten-Editor für Nachbearbeitung der aufgenommenen Punkte vervollständigen: Punkt löschen + Punkt auf Kante einfügen
  <!-- ctx: module:webui | files:webui/src/views/MapEditor.vue | model:sonnet -->

### C.15 Zustandsmodell / WebUI / Recovery nachziehen

- [x] C.15-a: Expliziten Startpfad modellieren: `Idle -> Undock -> NavToStart -> Mow`
  <!-- ctx: module:robot, module:op_statemachine, module:navigation | files:core/Robot.cpp, core/op/Op.h, core/op/Op.cpp | model:sonnet -->
- [x] C.15-b: `UndockOp` als eigene Operation implementieren
  <!-- ctx: module:op_statemachine, module:navigation | files:core/op/Op.h, core/op/Op.cpp, core/navigation/Map.h | model:sonnet -->
- [x] C.15-c: `NavToStartOp` als eigene Operation implementieren
  <!-- ctx: module:op_statemachine, module:navigation | files:core/op/Op.h, core/op/Op.cpp, core/navigation/LineTracker.cpp | model:sonnet -->
- [x] C.15-d: GPS-Recovery schärfen: `GpsWait` fachlich von hartem Fehler trennen, mit klarem Resume-Pfad
  <!-- ctx: module:robot, module:op_statemachine | files:core/Robot.cpp, core/op/GpsWaitFixOp.cpp, core/op/Op.h | model:sonnet -->
- [x] C.15-e: `WaitRainOp` als eigener Zustand statt reinem Dock-Shortcut vorbereiten
  <!-- ctx: module:op_statemachine, module:robot | files:core/op/Op.h, core/op/MowOp.cpp, core/Robot.cpp | model:sonnet -->
- [x] C.15-f: Kidnap-/Cross-Track-Recovery als expliziteren Verhaltensbaustein modellieren
  <!-- ctx: module:navigation, module:op_statemachine | files:core/navigation/LineTracker.cpp, core/op/Op.h, core/op/MowOp.cpp | model:sonnet -->
- [x] C.15-g: WebUI und Core-Zustandsmodell angleichen: UI soll fachliche Phasen nicht aus Telemetrie erraten müssen
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/components/RobotSidebar.vue, core/WebSocketServer.h, core/Robot.cpp | model:sonnet -->
- [x] C.15-h: Telemetrie weiter fachlich stabilisieren: Übergangsgründe, Fehlercodes und Zustandsphasen als bewusstes Modell
  <!-- ctx: module:websocket_server, module:robot | files:core/WebSocketServer.h, core/WebSocketServer.cpp, core/Robot.cpp | model:sonnet -->
- [x] C.15-i: Warm-Start-/Resume-Grundlagen prüfen: Kartenänderung erkennen und unsicheres Resume blockieren
  <!-- ctx: module:robot, module:navigation | files:core/Robot.h, core/Robot.cpp, core/navigation/Map.h | model:sonnet -->

### C.16 Benutzererlebnis / Nutzerführung

- [ ] C.16-a: Dashboard-Preflight als echte Startfreigabe ergänzen: GPS, Akku, Karte, Dock, Fehlerstatus, Verbindungsstatus sichtbar und verständlich bündeln
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/components/RobotSidebar.vue, core/WebSocketServer.h, core/Robot.cpp | model:sonnet -->
- [ ] C.16-b: Nutzerfreundliche Statussprache statt rein interner Op-Namen im Haupt-Dashboard etablieren (`Bereit`, `Fährt zum Startpunkt`, `Lädt`, `Wartet auf GPS`, ...)
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/composables/useTelemetry.ts, core/WebSocketServer.h, core/Robot.cpp | model:haiku -->
- [ ] C.16-c: Für Start-Blocker und Warnzustände direkte Handlungsempfehlungen im UI anzeigen, nicht nur Rohstatus
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/components/RobotSidebar.vue, core/Robot.cpp | model:sonnet -->
- [ ] C.16-d: Fehler- und Recovery-Karten vereinheitlichen: `Error`, `GpsWait`, `WaitRain`, `Dock`, `Charge` mit Ursache, Kritikalität und nächstem sinnvollen Schritt darstellen
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/views/Diagnostics.vue, core/WebSocketServer.h, core/Robot.cpp | model:sonnet -->
- [ ] C.16-e: Geführten Erststart-/Setup-Flow in der WebUI anlegen: Verbindung, GPS, Karte, Dock, Testfahrt, Startfreigabe
  <!-- ctx: module:webui, module:websocket_server | files:webui/src/views/Dashboard.vue, webui/src/router/index.ts, webui/src/components/RobotSidebar.vue | model:opus -->
- [ ] C.16-f: Mapping-Workflow als Assistent statt nur als Editor denken: neue Karte, RTK-Prüfung, Grenzaufnahme, Validierung, Docking-Pfad, Speichern
  <!-- ctx: module:webui, module:websocket_server, module:navigation | files:webui/src/views/Dashboard.vue, webui/src/views/MapEditor.vue, core/navigation/Map.h | model:sonnet -->
- [ ] C.16-g: Kartenerstellung vor Freigabe validieren: Perimeter geschlossen, ausreichend Punkte, Docking plausibel, Start grundsätzlich möglich
  <!-- ctx: module:webui, module:websocket_server, module:navigation | files:webui/src/views/MapEditor.vue, core/WebSocketServer.cpp, core/navigation/Map.h, core/navigation/Map.cpp | model:sonnet -->
- [ ] C.16-h: Ereignis- und Missionshistorie mit verständlichen Sessions und Abbruchgründen aufbauen, statt nur Live-Telemetrie anzuzeigen
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/History.vue, webui/src/composables/useSessionTracker.ts, core/Robot.cpp, core/WebSocketServer.h | model:sonnet -->
- [ ] C.16-i: Primäransicht und Diagnoseansicht stärker trennen, damit das Dashboard Betriebsoberfläche bleibt und Rohdaten in Diagnose landen
  <!-- ctx: module:webui | files:webui/src/views/Dashboard.vue, webui/src/views/Diagnostics.vue, webui/src/components/RobotSidebar.vue | model:haiku -->
- [ ] C.16-j: UX-nahe Ereignistimeline im Dashboard oder in History ergänzen (`Mission gestartet`, `GPS verloren`, `Docking begonnen`, `Laden begonnen`, ...)
  <!-- ctx: module:webui, module:websocket_server, module:robot | files:webui/src/views/Dashboard.vue, webui/src/views/History.vue, core/WebSocketServer.h, core/Robot.cpp | model:sonnet -->

---

## E — Größere Erweiterungen

### E.1 Sensorfusion (EKF) ✅ Abgeschlossen (2026-03-25)

- [x] E.1-a: `StateEstimator.h` — EkfState + EkfCovariance structs + private Methoden-Signaturen
- [x] E.1-b: `StateEstimator::predictStep()` — Odometrie-Delta → EkfState + Kovarianz-Propagation
- [x] E.1-c: `StateEstimator::updateGps()` — GPS-Messupdate, Messrauschen R aus Config
- [x] E.1-d: `StateEstimator::updateImu()` — IMU-Heading-Update
- [x] E.1-e: Tests — 3 Catch2-Tests (predictStep, updateGps, GPS-Failover)
- [x] E.1-f: Covariance Matrix — konfigurierbares Q/R via Config (`ekf_q_xy`, `ekf_q_theta`, `ekf_r_gps`, `ekf_r_imu`)
- [x] E.1-g: GPS-Failover — nahtloser Übergang zu Odometrie bei GPS-Verlust (`ekf_gps_failover_ms`)
- [x] E.1-h: Sensor Diagnostics — `ekf_health` Telemetry-Feld + Fusion-Badge in Diagnostics.vue

### E.2 Dynamisches Re-Routing (A*) ✅ Abgeschlossen (2026-03-25)

- [x] E.2-a: `core/navigation/GridMap.h + .cpp` — 40×40 lokales Belegungs-Gitter (0.25 m/Zelle)
- [x] E.2-b: A*-Algorithmus — 8-direktional, Euklidische Heuristik, ≤1600 Zellen
- [x] E.2-c: Integration — EscapeReverseOp nutzt GridMap A* → Map::injectFreePath(); Fallback: reguläres Replanen via `startDocking()`/`startMowing()`
- [x] E.2-d: Smooth Path — String-Pull-Visibility via GridMap::smoothPath()

### E.2x Docking- und Missionsverhalten fachlich schließen

- [x] E.2x-a: Docking entlang definiertem Docking-Pfad verlässlich ausführen, statt impliziter Restlogik
  <!-- ctx: module:navigation, module:op_statemachine | files:core/navigation/Map.h, core/navigation/Map.cpp, core/op/DockOp.cpp | model:sonnet -->
- [x] E.2x-b: `DockOp` TODOs für Waypoint-Fortschritt und Retry entfernen
  <!-- ctx: module:op_statemachine, module:navigation | files:core/op/DockOp.cpp, core/navigation/Map.cpp | model:haiku -->
- [x] E.2x-c: Lade-Kontakt-Retry und Dock-Fehlergründe klarer modellieren
  <!-- ctx: module:op_statemachine, module:robot | files:core/op/ChargeOp.cpp, core/op/DockOp.cpp, core/Robot.cpp | model:sonnet -->

### E.3 Vision-basierte Navigation (V-SLAM) 🚀 Prio 3

- [ ] E.3-a: Kamera-Integration — Pi-Cam via V4L2, `hal/CameraDriver/CameraDriver.h`
  <!-- ctx: module:hardware_interface | files:hal/HardwareInterface.h | model:sonnet -->
- [ ] E.3-b: Visual Odometry — Bewegungsschätzung aus aufeinanderfolgenden Frames
  <!-- ctx: module:navigation | files:hal/CameraDriver/CameraDriver.h | model:opus -->
- [ ] E.3-c: Loop Closure — Erkennung bereits besuchter Orte
  <!-- ctx: module:navigation | files:core/navigation/StateEstimator.h | model:opus -->

### E.4 Flottenmanagement & Mesh 🚀 Prio 4

- [ ] E.4-a: Mesh-Networking — MQTT-basierte Roboter-zu-Roboter Kommunikation
  <!-- ctx: module:mqtt_client | files:core/MqttClient.h | model:sonnet -->
- [ ] E.4-b: Task Splitting — Flächenaufteilung auf mehrere Roboter
  <!-- ctx: module:navigation, module:mqtt_client | files:core/navigation/Map.h | model:opus -->
- [ ] E.4-c: Status Monitoring — zentrales Dashboard für alle Roboter
  <!-- ctx: module:webui, module:websocket_server | files:webui/src/ | model:sonnet -->

### E.5 Berührungslose Sensorik 🚀 Prio 5

- [ ] E.5-a: `hal/SonarDriver/SonarDriver.h + .cpp` — HC-SR04 GPIO-Trigger/Echo
  <!-- ctx: module:hardware_interface | files:hal/HardwareInterface.h | model:haiku -->
- [ ] E.5-b: Safe-Stop — automatisches Abbremsen via nearObstacle
  <!-- ctx: module:hardware_interface, module:op_statemachine | files:hal/HardwareInterface.h, core/op/MowOp.cpp | model:haiku -->
- [ ] E.5-c: 3D-Sensing — Abgrunderkennung via Neigungssensor oder ToF
  <!-- ctx: module:hardware_interface, module:navigation | files:hal/HardwareInterface.h | model:sonnet -->

---

## Phase 3 — Differentiators

### P3.1 OTA Updates

- [ ] P3.1-a: `core/ota/OtaManager.h` — Interface: checkUpdate(), downloadUpdate(), applyUpdate()
  <!-- ctx: module:config | files:core/Config.h | model:haiku -->
- [ ] P3.1-b: `core/ota/OtaManager.cpp` — HTTP Download + SHA256-Verifikation
  <!-- ctx: module:config | files:core/ota/OtaManager.h | model:sonnet -->
- [ ] P3.1-c: Ed25519 Binary Signing via mbedtls (FetchContent)
  <!-- ctx: module:config | files:core/ota/OtaManager.h | model:opus -->
- [ ] P3.1-d: Rollback — dual-slot Bootloader (Active/Backup)
  <!-- ctx: module:config | files:core/ota/OtaManager.h | model:opus -->
- [ ] P3.1-e: Download Resume bei Netzwerk-Dropout (HTTP Range)
  <!-- ctx: module:config | files:core/ota/OtaManager.h | model:sonnet -->
- [ ] P3.1-f: Periodic Update Check via Config `ota_check_interval_hours`
  <!-- ctx: module:config, module:robot | files:core/ota/OtaManager.h, core/Robot.h | model:haiku -->
- [ ] P3.1-g: WebUI Settings → "Check for Updates" + Progress-Bar
  <!-- ctx: module:webui | files:webui/src/views/Settings.vue | model:haiku -->
- [ ] P3.1-h: Tests — Corruption, Network Loss, Rollback-Szenarien
  <!-- ctx: module:config | files:core/ota/OtaManager.h, tests/ | model:haiku -->

### P3.3 Adaptive Learning

- [ ] P3.3-a: Session-Metrics Sampling — nach Mäh-Session in SQLite speichern
  <!-- ctx: module:robot, module:navigation | files:core/Robot.h | model:sonnet -->
- [ ] P3.3-b: TensorFlow Lite Integration (FetchContent, 100KB Model)
  <!-- ctx: module:config | files:core/Robot.h | model:opus -->
- [ ] P3.3-c: Online Training nach Session 5/15/25
  <!-- ctx: module:config | files:core/Robot.h | model:opus -->
- [ ] P3.3-d: WebUI Efficiency-Chart
  <!-- ctx: module:webui | files:webui/src/views/ | model:haiku -->

### P3.4 Mobile App

- [ ] P3.4-a: iOS SwiftUI App — Dashboard + Map (separates Xcode-Projekt)
  <!-- ctx: module:websocket_server | files:docs/ARCHITECTURE.md | model:opus -->
- [ ] P3.4-b: Android Jetpack Compose App (separates Android-Studio-Projekt)
  <!-- ctx: module:websocket_server | files:docs/ARCHITECTURE.md | model:opus -->

---

## D — Geklärte / externe Punkte

- [x] Q1: StateEstimator Fehler-Propagation bei dauerhaft ungültigem GPS geklärt
  Entscheidung: aktueller Core fällt bereits kontrolliert auf Odometrie zurück (`gpsHasFix`/`gpsHasFloat` + `ekf_health`). Falls gewünscht, später als explizites Telemetrie-/Policy-Flag (`odometry_only` / `localisation_degraded`) ausbauen.
  <!-- ctx: module:navigation | files:core/navigation/StateEstimator.h, core/navigation/StateEstimator.cpp, core/Robot.cpp | model:haiku -->
- [ ] Q3: GPS no-motion Schwellenwert 0.05m — ausreichend für RTK-Float?
  Offener Feldtest: nur mit echten RTK-Float-Logs bzw. Hardwarefahrt belastbar entscheidbar.
  <!-- ctx: module:navigation, module:gps_driver | files:core/navigation/StateEstimator.h, core/Config.cpp | model:haiku -->
- [x] Q5: missionAPI.run() Unix-Socket wirklich non-blocking?
  Obsolet im aktuellen Core: die frühere MissionAPI/Unix-Socket-Architektur ist im aktuellen Codepfad nicht mehr vorhanden; Altfrage aus Legacy-Dokumentation.
  <!-- ctx: module:websocket_server | files:OLD_DOCS/cassandra_mission_service_analysis.md | model:haiku -->
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert?
  Offener Integrationstest: Codepfad ist bekannt (`keepPowerOn(false)` mit 5 s Grace), echte Timing-Absicherung braucht Hardware.
  <!-- ctx: module:serial_robot_driver, module:robot | files:hal/SerialRobotDriver/SerialRobotDriver.cpp | model:haiku -->

---

## Hinweise zur Nutzung

- [x] Erledigt | [ ] Offen | ⏸ Blockiert
- `<!-- ctx: -->` ist Legacy-Metadatenformat: `module:` + `files:` + `model:`
- `module:` verweist auf Einträge in [`.memory/module_index.md`](/mnt/LappiDaten/Projekte/sunray-core/.memory/module_index.md) bzw. `.memory/modules/`
- `files:` nennt die konkret zu lesenden Dateien
- `model:` ist das frühere vorgeschlagene Modell: `haiku`, `sonnet` oder `opus`
