# sunray-core

`sunray-core` ist der Linux-basierte Robot-Controller fuer einen RTK-GPS-Rasenmaehroboter auf Raspberry Pi.
Das Projekt umfasst:

- einen C++17-Core fuer Hardware, Navigation, Missionslogik und Telemetrie
- eine WebUI auf Basis von Vue 3 + TypeScript
- einen Simulationsmodus fuer Entwicklung ohne reale Hardware
- eine Test-Suite fuer Core-, Navigation-, State-Machine- und WebUI-Verhalten

Der aktuelle Schwerpunkt liegt auf einem robusten, nachvollziehbaren Software-Stand fuer Linux und den ersten echten Hardwaretest.

GitHub-Repository:
<https://github.com/Starsurfer78/sunray-core>

## Status

- Linux-Build: validiert
- C++-Tests: gruen
- WebUI-Tests: gruen
- Software-Release-Gate: fachlich abgearbeitet
- Hardware-/Feldtest: weiterhin eigener Schritt auf echter Plattform

Der Code bildet heute den echten Ist-Zustand ab. Aeltere Konzept- und Architekturdateien sind teilweise noch als Hintergrundmaterial im Repo vorhanden, aber nicht alle beschreiben den aktuellen Code 1:1.

## Kernfunktionen

- Hardware-Abstraktion fuer echten Roboter und Simulation
- Op-State-Machine mit `Idle`, `Undock`, `NavToStart`, `Mow`, `Dock`, `Charge`, `WaitRain`, `GpsWait`, `EscapeReverse`, `EscapeForward`, `Error`
- Navigation mit Odometrie, GPS, IMU und EKF-basierter Pose-Schaetzung
- Kartenverwaltung fuer Perimeter, No-Go-Zonen, Mow-Punkte, Dock-Pfade, Zonen und virtuelle Hindernisse
- Dynamisches Re-Routing ueber lokales `GridMap`-A*
- Telemetrie ueber WebSocket und optional MQTT
- WebUI fuer Dashboard, Map-Editor, Diagnose, Historie und Simulator
- API-/WebSocket-Auth ueber `api_token`

## Architektur

Das Projekt ist in drei Hauptschichten aufgebaut:

- `platform/`
  POSIX-nahe Bausteine wie Serial, I2C und Systemzugriffe
- `hal/`
  `HardwareInterface`, echter `SerialRobotDriver`, `SimulationDriver`, GPS- und IMU-Treiber
- `core/`
  `Robot`, State-Machine, Navigation, WebSocket/HTTP-Server, MQTT, Konfiguration

Die WebUI liegt separat unter `webui/` und wird im Produktionsbetrieb als statische Anwendung vom C++-Server aus `webui/dist` ausgeliefert.

## Repository-Struktur

```text
sunray-core/
├── core/                  C++-Core: Robot, Ops, Navigation, API, MQTT
├── hal/                   Hardware-Treiber und Simulation
├── platform/              Linux-nahe Hilfsbibliotheken
├── tests/                 Catch2-Test-Suite
├── webui/                 Vue 3 + TypeScript Frontend
├── docs/                  aktuelle Projektdokumentation
├── OLD_DOCS/              Alt-Dokumente und Analysen
├── config.example.json    Beispielkonfiguration
├── main.cpp               Einstiegspunkt
└── CMakeLists.txt         Top-Level-Build
```

## Voraussetzungen

Fuer den Linux-Build:

- Linux
- CMake >= 3.20
- C++17-Compiler (`g++` oder `clang++`)
- Node.js + npm fuer die WebUI

Fuer echten Robot-Betrieb zusaetzlich:

- Raspberry Pi
- STM32-/Motorcontroller ueber UART
- GPS-Empfaenger
- passende `config.json`
- passende `map.json`

## Schnellstart

### Repository klonen

```bash
git clone https://github.com/Starsurfer78/sunray-core.git
cd sunray-core
```

### 1. Repository bauen

```bash
cmake -S . -B build_linux
cmake --build build_linux -j2
```

### 2. WebUI bauen

```bash
cd webui
npm install
npm run build
```

### 3. Tests ausfuehren

```bash
ctest --test-dir build_linux --output-on-failure

cd webui
npm test
```

## Starten

### Simulation

```bash
./build_linux/sunray-core --sim config.example.json
```

Der Simulationsmodus ist der schnellste Weg, um State-Machine, API, Telemetrie und WebUI lokal zu pruefen.

### Echter Roboter

```bash
./build_linux/sunray-core /etc/sunray/config.json
```

Standardpfad fuer die Konfiguration ist `/etc/sunray/config.json`.

## Konfiguration

Ausgangspunkt ist [config.example.json](config.example.json).

Wichtige Schluessel:

- `driver`: `serial` fuer echten Roboter
- `driver_port`: UART zur MCU
- `gps_port`: GPS-Empfaenger
- `api_token`: Pflicht fuer schreibende REST-/WS-Zugriffe
- `map_path`: Pfad zur Karten-Datei
- `mqtt_enabled`: optionaler MQTT-Betrieb
- `enable_mow_motor`: fuer Trockenlauf auf `false` moeglich

Freigaberelevante Hinweise stehen in [RELEASE_CONFIGURATION.md](docs/RELEASE_CONFIGURATION.md).

## Karte und Mission

Der Core arbeitet mit einer JSON-Karte. Typische Inhalte:

- `perimeter`
- `mow`
- `dock`
- `exclusions`
- `zones`
- `captureMeta`

Die Karte kann ueber die WebUI bearbeitet und ueber die API geladen werden. Der aktuelle Mapping- und Editor-Stand ist eng mit der WebUI verzahnt.

## WebUI

Die WebUI bietet:

- Dashboard mit Live-Telemetrie und Karte
- Karteneditor fuer Perimeter, Zonen, Docking und Hindernisse
- Diagnose-Ansichten
- Verlauf und Statistiken
- Simulator-Steuerung

Weitere Frontend-Details stehen in [webui/README.md](webui/README.md).

## API und Telemetrie

Der C++-Server startet HTTP + WebSocket und liefert:

- statische WebUI-Dateien
- REST-Endpunkte fuer Config, Map, Schedule, Diagnose
- WebSocket-Telemetrie
- WebSocket-Kommandos wie `start`, `stop`, `dock`, `drive`, `startZones`

Die wichtigsten Telemetriedaten umfassen unter anderem:

- aktiven Op
- Pose (`x`, `y`, `heading`)
- GPS-Qualitaet
- Akku
- IMU
- `ekf_health`
- `state_phase`
- `event_reason`
- `error_code`

## Dokumentation im Repo

Aktuelle, relevante Dateien:

- [docs/STATUS.md](docs/STATUS.md)
- [docs/SOFTWARE_RELEASE_GATE.md](docs/SOFTWARE_RELEASE_GATE.md)
- [docs/RELEASE_CONFIGURATION.md](docs/RELEASE_CONFIGURATION.md)
- [VERHALTENSKONZEPT.md](VERHALTENSKONZEPT.md)
- [TODO.md](TODO.md)

Historische bzw. analytische Alt-Dokumente liegen unter `OLD_DOCS/`.

## Entwicklungsworkflow

Typischer lokaler Ablauf:

```bash
cd sunray-core
cmake -S . -B build_linux
cmake --build build_linux -j2
ctest --test-dir build_linux --output-on-failure

cd webui
npm install
npm run build
npm test
```

## Bekannte Grenzen

- Die Software ist fuer Linux validiert, nicht fuer andere Plattformen.
- Der Simulationsmodus deckt viel Logik ab, ersetzt aber keinen echten Hardware- und Feldtest.
- Einige Alt-Dokumente beschreiben ein groesseres Zielbild als der aktuelle Core heute direkt implementiert.
- Hardwarefragen wie RTK-Float-Grenzen oder Watchdog-Koordination muessen weiterhin am echten System verifiziert werden.

## Lizenz / Hinweise

Im Repository gibt es aktuell keine separat gepflegte Top-Level-Lizenzdatei.
Wenn das Projekt oeffentlich auf GitHub erscheinen soll, sollte als naechster Doku-Schritt auch eine klare Lizenz und ein kurzer Contribution-Hinweis ergaenzt werden.
