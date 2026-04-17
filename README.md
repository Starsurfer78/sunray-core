# sunray-core

`sunray-core` ist der Linux-native Laufzeit-, Planungs- und Bedien-Stack fuer den RTK-GPS-Rasenroboter Alfred.
Das System laeuft auf einem Raspberry Pi, spricht mit dem STM32-Motorcontroller, liefert eine WebUI aus und wird zusaetzlich von einer nativen Android-App bedient.

## Produktbild

Das Repository enthaelt die komplette lokale Roboterplattform:

- C++17-Runtime fuer Navigation, Ops, Safety und Hardware-Anbindung
- HTTP- und WebSocket-API fuer Steuerung, Telemetrie, Karten und Missionen
- Svelte-WebUI fuer Browser-Bedienung im Heimnetz
- Flutter-App `mobile-app-v2` als aktuelle mobile Bedienoberflaeche
- OTA-, Deploy- und Service-Skripte fuer Betrieb und Wartung

## Kernfunktionen

- RTK-GPS-gestuetzte Navigation mit Missions- und Routenplanung
- eine konsolidierte Missionsroute fuer Preview, Runtime und Fortschritt
- Kartenmodell mit Perimeter, Hard-No-Go, Dock und Zonen
- manuelle Steuerung per WebUI und App
- OTA fuer Roboter und Android-App
- Diagnose-, Verlaufs- und Service-Funktionen fuer Feldtests und Betrieb

## Oberflaechen

### WebUI

Die Browser-Oberflaeche lebt in [webui-svelte](webui-svelte/README.md) und deckt Dashboard, Karte, Missionen, Diagnose, Verlauf und Einstellungen ab.

### Mobile App

Die aktuelle Android-App lebt in [mobile-app-v2](mobile-app-v2/README.md).
Sie bildet den freigegebenen Hauptfluss fuer Discovery, Kartenanlage, Zonen, Zeitplaene, Betrieb, Controller-Modus und OTA ab.

Der Ordner `mobile-app` bleibt derzeit nur noch als Altstand und Referenz im Repository.

## Schnellstart

### Backend bauen

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

### Simulation starten

```bash
./build/sunray-core --sim config.example.json
```

Danach ist die API mitsamt WebUI typischerweise unter `http://localhost:8765` erreichbar.

## WebUI lokal entwickeln

```bash
cd webui-svelte
npm install
npm run dev
```

## App lokal entwickeln

```bash
cd mobile-app-v2
flutter pub get
flutter analyze
flutter test
flutter run
```

## Wichtige Repository-Bereiche

```text
sunray-core/
├── core/             Robot, Ops, Navigation, Planner, API, WebSocket
├── hal/              Hardware-Treiber fuer Alfred, GPS und Simulation
├── platform/         Linux-nahe Plattformschicht
├── tests/            Backend-Tests
├── webui-svelte/     Svelte-WebUI
├── mobile-app-v2/    aktuelle Flutter-App
├── mobile-app/       Altstand / Referenz
├── scripts/          Install-, OTA- und Deploy-Helfer
└── docs/             technische Notizen und Produktspezifikation
```

## Betrieb und Updates

- der Roboter stellt lokal REST und WebSocket bereit
- die WebUI wird direkt vom Backend ausgeliefert
- `mobile-app-v2` unterstuetzt GitHub-App-OTA und Roboter-OTA
- Releases mit APK-Asset liegen unter GitHub Releases

## Hardware-Ziel

- Raspberry Pi 4B als Hostsystem
- STM32-basierter Motorcontroller via UART
- RTK-faehiger GPS-Empfaenger
- weitere Sensorik fuer Regen, Docking, Kollision und Lage

## Hinweis zum Projektstand

Der aktive Produktpfad fuer mobile Bedienung ist `mobile-app-v2`.
Wenn du den aktuellen Endnutzerfluss oder neue Mobile-Arbeit suchst, ist das fast immer der richtige Einstiegspunkt.
