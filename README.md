# sunray-core

`sunray-core` ist ein Linux-basierter Robot-Controller fuer einen RTK-GPS-Rasenmaehroboter auf Raspberry Pi.

Das Projekt umfasst:

- einen C++17-Core fuer Hardware, Navigation, Missionslogik und Telemetrie
- eine aktive WebUI auf Basis von Svelte 5 + TypeScript
- einen Simulationsmodus fuer Entwicklung ohne reale Hardware
- native Tests fuer Core-, Navigation- und Op-Verhalten

GitHub-Repository:
<https://github.com/Starsurfer78/sunray-core>

## Herkunft

Dieses Projekt baut auf dem Ardumower-Sunray-Projekt auf:
<https://github.com/Ardumower/Sunray>

`sunray-core` ist eine Linux-/Pi-orientierte Weiterentwicklung und Neuordnung dieser Basis.

## Status

- Architektur- und Navigationsumbau ist im Code abgeschlossen und in [docs/NAVIGATION_UPGRADE.md](docs/NAVIGATION_UPGRADE.md) dokumentiert.
- Aktive Frontend-Basis ist `webui-svelte/`.
- Der groesste offene Block ist jetzt reale Linux-/Pi-/Hardware-Validierung.
- Offene Aufgaben stehen nur noch in [TODO.md](TODO.md).

## Einstieg Im Repo

Wenn du das Projekt neu aufmachst, starte hier:

- [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)
- [PROJECT_MAP.md](PROJECT_MAP.md)
- [TODO.md](TODO.md)
- [WORKING_RULES.md](WORKING_RULES.md)

Wichtige Fachdokumente:

- [docs/NAVIGATION_UPGRADE.md](docs/NAVIGATION_UPGRADE.md)
- [docs/OP_STATE_MACHINE.md](docs/OP_STATE_MACHINE.md)
- [docs/TELEMETRY_CONTRACT.md](docs/TELEMETRY_CONTRACT.md)
- [docs/ROBOT_RUN_BASELINE.md](docs/ROBOT_RUN_BASELINE.md)
- [docs/ALFRED_TEST_RUN_GUIDE.md](docs/ALFRED_TEST_RUN_GUIDE.md)
- [docs/ALFRED_FLASHING.md](docs/ALFRED_FLASHING.md)
- [docs/ROBOT_BUTTON_BUZZER_ERROR.md](docs/ROBOT_BUTTON_BUZZER_ERROR.md)

## Kernfunktionen

- Hardware-Abstraktion fuer echten Roboter und Simulation
- Op-State-Machine mit `Idle`, `Undock`, `NavToStart`, `Mow`, `Dock`, `Charge`, `WaitRain`, `GpsWait`, `EscapeReverse`, `EscapeForward`, `Error`
- Navigation mit Odometrie, GPS, IMU, State Estimation, Planner, Costmap und Route-Segmenten
- Kartenverwaltung fuer Perimeter, No-Go-Zonen, Dock-Pfad, Zonen und Hindernisse
- Telemetrie ueber WebSocket und optional MQTT
- WebUI fuer Dashboard, Karte, Mission, Diagnose und Historie
- API-/WebSocket-Auth ueber `api_token`

## Repository-Struktur

```text
sunray-core/
├── core/                  C++-Core: Robot, Ops, Navigation, API, MQTT
├── hal/                   Hardware-Treiber und Simulation
├── platform/              Linux-nahe Hilfsbibliotheken
├── tests/                 Catch2-Test-Suite
├── webui-svelte/          aktive Svelte-WebUI
├── docs/                  aktuelle Fachdokumentation
├── ALTE_DATEIEN/          archivierte Alt-Dokumente und Vue-Referenz
├── config.example.json    Beispielkonfiguration
├── main.cpp               Einstiegspunkt
└── CMakeLists.txt         Top-Level-Build
```

## Voraussetzungen

Fuer den Linux-Build:

- Linux
- CMake >= 3.20
- C++17-Compiler
- Node.js >= 20 fuer die WebUI

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

### Native bauen

```bash
cmake -S . -B build_pi
cmake --build build_pi -j2
ctest --test-dir build_pi --output-on-failure
```

### WebUI pruefen

```bash
cd webui-svelte
npm install
npm run check
npm run build
```

### Simulation starten

```bash
./build_pi/sunray-core --sim config.example.json
```

### Echter Roboter

```bash
./build_pi/sunray-core /etc/sunray-core/config.json
```

## Installations- Und Hilfsskripte

### `scripts/install_sunray.sh`

Installiert Linux-Abhaengigkeiten, baut Core und WebUI, legt bei Bedarf Laufzeitdateien an und kann `systemd`-Autostart einrichten.

Beispiele:

```bash
./scripts/install_sunray.sh
./scripts/install_sunray.sh --sim
./scripts/install_sunray.sh --autostart yes
./scripts/install_sunray.sh --no-start
```

### `scripts/flash_alfred.sh`

Baut und flasht die Alfred-STM32-Firmware ueber SWD/OpenOCD.

Beispiele:

```bash
sudo bash scripts/flash_alfred.sh probe
bash scripts/flash_alfred.sh build
sudo bash scripts/flash_alfred.sh build-flash
```

Details stehen in [docs/ALFRED_FLASHING.md](docs/ALFRED_FLASHING.md).

## Konfiguration

Ausgangspunkt ist [config.example.json](config.example.json).

Wichtige Schluessel:

- `driver`
- `driver_port`
- `gps_port`
- `api_token`
- `map_path`
- `mqtt_enabled`
- `enable_mow_motor`

Freigaberelevante Hinweise stehen in [docs/RELEASE_CONFIGURATION.md](docs/RELEASE_CONFIGURATION.md).

## Karte Und Mission

Der Core arbeitet mit einer JSON-Karte. Typische Inhalte:

- `perimeter`
- `mow`
- `dock`
- `exclusions`
- `zones`
- `captureMeta`

Die Karte wird ueber die WebUI und `/api/map` bearbeitet.

## WebUI

Die aktive WebUI bietet:

- Dashboard mit Live-Telemetrie
- Karteneditor fuer Perimeter, Zonen, Docking und Hindernisse
- Missionsansicht
- Diagnose
- Historie und Statistik
- Simulator-Steuerung

Weitere Frontend-Details stehen in [webui-svelte/README.md](webui-svelte/README.md).

## API Und Telemetrie

Der C++-Server liefert:

- statische WebUI-Dateien
- REST-Endpunkte fuer Config, Map, Missions, Schedule, Diagnose, History
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

## Entwicklungsworkflow

Typischer lokaler Ablauf:

```bash
cmake -S . -B build_pi
cmake --build build_pi -j2
ctest --test-dir build_pi --output-on-failure

cd webui-svelte
npm install
npm run check
npm run build
```

## Bekannte Grenzen

- Belastbare Aussagen zu Hardwareverhalten brauchen echte Linux-/Pi-/Feldtests.
- Bestehende Build-Artefakte im Repo sind nicht portabel und nicht autoritativ.
- Themen wie RTK-Float-Grenzen und Watchdog-Koordination muessen am echten System verifiziert werden.

## Lizenz / Hinweise

Im Repository gibt es aktuell keine separat gepflegte Top-Level-Lizenzdatei.
Vor breiterem Einsatz sollte die Lizenzlage explizit geklaert und dokumentiert werden.
