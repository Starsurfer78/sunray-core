# TODO.md — sunray-core

Stand: März 2026

> **Konvention:** Jeder offene Task `[ ]` hat ein `<!-- ctx: -->` Profil direkt darunter.
> Claude Code liest dieses Profil und lädt **nur** die dort genannten Module + Files.
> Format: `<!-- ctx: module:X, module:Y | files:path/to/A.h, path/to/B.cpp | model:haiku -->

---

## ✅ Erledigte Meilensteine

- [x] A.1–A.8, A.10 — C++ Fundament, SerialRobotDriver, Robot+DI, SimulationDriver, Op-State-Machine, Navigation, WebSocket-Server, Konfiguration, GPS-Treiber, Pi-Version
- [x] C.1–C.5, C.7, C.9–C.12 — WebUI, MQTT-Client, On-The-Fly Obstacles, Dashboard, Diagnose, Zeitplan, Zonen-Auswahl
- [x] P0 Blocker (A.9) — STM32 Flashen via Pi, CRC-Verifikation, Motor-API

---

## A — sunray-core (C++ Rewrite)

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

---

## E — Priorisierte Erweiterungen

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
- [x] E.2-c: Integration — EscapeReverseOp nutzt GridMap A* → Map::injectFreePath(); Fallback: Map::findPath()
- [x] E.2-d: Smooth Path — String-Pull-Visibility via GridMap::smoothPath()

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

## D — Offene Fragen

- [ ] Q1: StateEstimator Fehler-Propagation bei dauerhaft ungültigem GPS (globale Flag?)
  <!-- ctx: module:navigation | files:core/navigation/StateEstimator.h | model:haiku -->
- [ ] Q3: GPS no-motion Schwellenwert 0.05m — ausreichend für RTK-Float?
  <!-- ctx: module:navigation, module:gps_driver | files:core/navigation/StateEstimator.h | model:haiku -->
- [ ] Q5: missionAPI.run() Unix-Socket wirklich non-blocking?
  <!-- ctx: module:websocket_server | files:docs/ARCHITECTURE.md | model:haiku -->
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert?
  <!-- ctx: module:serial_robot_driver, module:robot | files:hal/SerialRobotDriver/SerialRobotDriver.cpp | model:haiku -->

---

## Legende

- [x] Erledigt | [ ] Offen | ⏸ Blockiert
- `<!-- ctx: -->` — Kontext-Profil für Claude Code (module: + files: + model:)
- **module:** Kürzel → Datei in `.memory/modules/<kürzel>.md`
- **files:** konkrete Files die gelesen werden sollen
- **model:** haiku / sonnet / opus
