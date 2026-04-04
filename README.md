# sunray-core — Alfred, der smarte Rasenroboter

**sunray-core** ist ein vollständiger, Linux-nativer Controller für RTK-GPS-Rasenroboter auf Raspberry Pi-Basis.
Er ersetzt das originale Arduino/Sunray-Backend durch eine moderne C++17-Laufzeit mit WebUI und nativer Android-App.

> Basiert auf [Ardumower/Sunray](https://github.com/Ardumower/Sunray) — weiterentwickelt für Linux, Raspberry Pi 4 und echten Feldeinsatz.

---

## Was ist sunray-core?

Alfred ist ein selbstfahrender Rasenmähroboter, der mit RTK-GPS millimetergenau navigiert.
sunray-core ist sein Gehirn: C++17-Core auf dem Pi, gesteuert über Browser oder Smartphone — ohne Cloud, ohne Abo.

```
┌─────────────────────────────────────────────────────┐
│                   Raspberry Pi 4                    │
│                                                     │
│   sunray-core (C++17)                               │
│   ├── Navigation + RTK-GPS                          │
│   ├── Op-State-Machine                              │
│   ├── Crow HTTP/WebSocket-Server                    │
│   └── WebUI (Svelte) ─────► Browser                │
│                │                                    │
│                └──────────► Alfred App (Android)    │
│                                                     │
│   STM32 (UART) ────────────► Motoren, Sensoren      │
└─────────────────────────────────────────────────────┘
```

---

## Features

### Navigation & Betrieb

- **RTK-GPS-Navigation** — cm-genaue Positionierung mit NTRIP/RTK-Float/Fix
- **State Machine** — Idle → Undock → NavToStart → Mähen → Dock → Laden, automatische Fehlerbehandlung
- **Hindernisvermeidung** — Erkennung und automatische Umfahrung
- **Missionsplanung** — mehrere Mähzonen pro Mission, Planer-Vorschau vor dem Start
- **Perimeter & No-Go-Zonen** — flexibles Kartenformat mit Zonen, Hindernissen und Dock-Pfad
- **Wetterpause** — automatisches Warten bei Regen
- **Simulationsmodus** — volle Entwicklung und Tests ohne reale Hardware

### Konnektivität & Update

- **WebSocket-Telemetrie** — Echtzeit-Statusstream für Browser und App
- **REST-API** — Karte, Missionen, Diagnose, History, OTA vollständig über HTTP
- **OTA-Update** — sunray-core via App oder WebUI aktualisieren, kein SSH nötig
- **Systemd-Integration** — Autostart, Watchdog, automatischer Neustart
- **API-Token-Auth** — sichere Verbindung im Heimnetz

---

## WebUI — Alfred im Browser

Die Svelte-WebUI läuft direkt auf dem Pi und ist über jeden Browser im Heimnetz erreichbar.

### Seiten

| Seite | Inhalt |
|---|---|
| **Dashboard** | Live-Telemetrie, Roboterstatus, Steuerung, aktuelle Mission |
| **Karte** | Interaktiver Editor für Perimeter, Zonen, No-Go und Dock-Pfad |
| **Missionen** | Mähzonen auswählen, Route-Vorschau, Start/Stop |
| **Diagnose** | IMU-Panel, Motortest, Tick-Kalibrierung, Richtungsvalidierung, Sensorwerte |
| **Verlauf** | Ereignishistorie, Mähstatistik |
| **Einstellungen** | Konfiguration, OTA-Update |

### Karteneditor

- Perimeter, Zonen, No-Go-Zonen und Dock-Pfad direkt auf OpenStreetMap zeichnen
- Punkte verschieben, einfügen, löschen
- Sofortige Vorschau der Mähroute vor dem ersten Einsatz

### Schnellstart WebUI

```bash
cd webui-svelte
npm install
npm run dev        # Entwicklung (Vite-Dev-Server mit Proxy)
npm run build      # Produktions-Build → dist/
```

---

## Alfred App — Android-Companion

Die native Android-App (Flutter) verbindet sich per **mDNS-Discovery** automatisch mit Alfred im Heimnetz — kein IP-Konfigurieren nötig.

### Features

**Dashboard**
- Verbindungsstatus, Akku, GPS-Qualität, aktiver Mähzustand
- Checkliste: Verbunden / Perimeter vorhanden / Dock gesetzt / Mission bereit
- Laufender Betrieb: Fortschrittsbalken, Phase, Missionname, Akku
- Direktstart einer Mission mit einem Tap

**Karte**
- Live-Roboterposition auf OpenStreetMap — zentriert automatisch beim ersten GPS-Fix
- "Roboter zentrieren"-Button jederzeit verfügbar
- Perimeter, Zonen, No-Go und Dock-Pfad in der Übersicht

**Karteneditor**
- Perimeter, Zonen, No-Go-Zonen und Dock-Pfad anlegen und bearbeiten
- **GPS-Recording**: aktuelle Roboterposition per Button als Punkt speichern — kein manuelles Tippen
- Punkt-Counter und Echtzeit-Koordinatenanzeige während der Aufzeichnung

**Missionen**
- Missionen anlegen, benennen, Zonen zuweisen
- **Zone-Tap**: Mähzone direkt auf der Karte antippen zum Hinzufügen/Entfernen
- Mährouten-Vorschau vor dem Start
- Wiederkehrende Missionen, Nur-Trocken, Akku-Schwellwert

**Service**
- **App-OTA**: Update direkt aus GitHub Releases laden und installieren
- **Pi-OTA**: sunray-core-Update ohne SSH aufspielen und Neustart überwachen
- **Diagnose**: Akkuspannung, Ladestatus, MCU-Verbindung, GPS-Qualität, Runtime-Health, Betriebszeit
- **Logs**: Ereignishistorie live vom Roboter laden

**Verbindung**
- mDNS-Discovery — Alfred wird automatisch gefunden
- Aktive Verbindungsüberwachung (Watchdog): erkennt stille TCP-Drops innerhalb von 10 Sekunden
- Automatischer Wiederverbindungsversuch mit exponentiellem Backoff

### App installieren

Releases mit APK sind unter [GitHub Releases](https://github.com/Starsurfer78/sunray-core/releases) verfügbar.
Die App prüft beim Start selbst auf neue Versionen und bietet direktes Update an.

---

## Hardware

| Komponente | Details |
|---|---|
| Rechner | Raspberry Pi 4B |
| Motorcontroller | STM32F103 (Alfred-Board) via UART |
| GPS | RTK-fähiger Empfänger (z.B. u-blox) |
| IMU | I2C (BNO055 o.ä.) |
| Stromversorgung | LiPo 6S, Ladekontakt am Dock |

---

## Schnellstart

### Repository klonen

```bash
git clone https://github.com/Starsurfer78/sunray-core.git
cd sunray-core
```

### Bauen (Linux / Raspberry Pi)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Simulation starten

```bash
./build/sunray-core --sim config.example.json
```

WebUI dann unter `http://localhost:8765` erreichbar.

### Echter Roboter

```bash
./build/sunray-core /etc/sunray-core/config.json
```

### Installations-Skript (Pi)

```bash
bash scripts/install_sunray.sh              # Build + Start
bash scripts/install_sunray.sh --autostart yes  # + Systemd-Autostart
```

---

## Konfiguration

Ausgangspunkt: [`config.example.json`](config.example.json)

Wichtigste Parameter:

| Key | Bedeutung |
|---|---|
| `driver` | `alfred` für echten Roboter, `sim` für Simulation |
| `driver_port` | UART-Gerät des STM32 |
| `gps_port` | UART-Gerät des GPS-Empfängers |
| `api_token` | Zugangstoken für WebUI und App |
| `map_path` | Pfad zur Karten-JSON-Datei |
| `enable_mow_motor` | Mähmotor aktivieren (false für Tests) |

---

## Repository-Struktur

```
sunray-core/
├── core/            C++-Core: Robot, Ops, Navigation, API, WebSocket
├── hal/             Hardware-Treiber (Alfred, Simulation, GPS)
├── platform/        Linux-Plattformschicht (Serial, I2C)
├── tests/           Catch2-Unit-Tests
├── webui-svelte/    Svelte-WebUI (Dashboard, Karte, Diagnose, …)
├── mobile-app/      Flutter Android-App
├── scripts/         Install-, Deploy- und Flash-Skripte
├── docs/            Technische Dokumentation
├── config.example.json
└── CMakeLists.txt
```

---

## Voraussetzungen

**Build (Linux/Pi):**
- CMake ≥ 3.20
- C++17-Compiler (GCC oder Clang)
- Node.js ≥ 20 (für WebUI)

**App:**
- Android 8.0+ (API 26)
- APK aus Releases oder selbst mit Flutter bauen

---

## Herkunft & Lizenz

Dieses Projekt baut auf [Ardumower/Sunray](https://github.com/Ardumower/Sunray) auf.
sunray-core ist eine vollständige Linux-orientierte Neuarchitektur dieser Basis.

Lizenzlage: vor breitem Einsatz bitte prüfen — aktuell keine separate Top-Level-Lizenzdatei gepflegt.
