# Alfred App — Android-Companion für sunray-core

Native Android-App (Flutter) zur Steuerung und Überwachung des Alfred-Rasenroboters.
Verbindet sich automatisch per mDNS-Discovery — kein IP-Konfigurieren nötig.

## Features

### Dashboard
- Live-Status: Verbindung, Akku, GPS-Qualität, aktive Phase
- Checkliste: Verbunden / Perimeter / Dock / Mission — jeder Punkt direkt anspringbar
- Laufend: Fortschrittsbalken + Phase + Missionname während des Mähens
- Schnellstart, Stopp, Andocken per Tap

### Karte
- Roboterposition live auf OpenStreetMap
- Automatische Zentrierung beim ersten GPS-Fix
- "Roboter zentrieren"-Button jederzeit
- Perimeter, Mähzonen, No-Go und Dock in der Übersicht

### Karteneditor
- Perimeter, Zonen, No-Go-Zonen, Dock-Pfad anlegen und bearbeiten
- GPS-Recording: aktuelle Roboterposition per Button als Punkt speichern
- Punkte verschieben, einfügen, löschen

### Missionen
- Missionen anlegen und benennen
- Zonen per Tap auf der Karte oder über Chips zuweisen
- Mährouten-Vorschau vor dem Start
- Wiederkehrend, Nur-Trocken, Akkuschwellwert einstellbar

### Service
- App-OTA: Update aus GitHub Releases laden und installieren
- Pi-OTA: sunray-core ohne SSH aktualisieren, Neustart überwachen
- Diagnose: Akku, Ladestatus, MCU, GPS, Runtime-Health, Betriebszeit
- Logs: Ereignishistorie vom Roboter

### Verbindung
- mDNS-Discovery (multicast DNS) — Alfred wird automatisch gefunden
- Watchdog: erkennt stille TCP-Drops innerhalb von 10 Sekunden
- Automatischer Wiederverbindungsversuch mit exponentiellem Backoff (1–16 s)

## Technischer Stack

- **Flutter 3** / Dart — Android (iOS-Unterstützung vorbereitet)
- **Riverpod** — State Management
- **go_router** — Navigation
- **flutter_map** + **OpenStreetMap** — Kartendarstellung
- **web_socket_channel** — WebSocket-Telemetrie
- **Dio** — HTTP/REST
- **Drift** (SQLite) — lokale Persistenz
- **multicast_dns** — mDNS-Discovery

## Installieren

APK aus [GitHub Releases](https://github.com/Starsurfer78/sunray-core/releases) herunterladen.
Die App erkennt neue Versionen selbstständig und bietet direktes Update an.

## Selbst bauen

```bash
cd mobile-app
flutter pub get
flutter build apk --release
```

APK landet in `build/app/outputs/flutter-apk/app-release.apk`.

## Release erstellen

Version in `pubspec.yaml` erhöhen (`version: x.y.z+build`), dann:

```bash
git tag vx.y.z
git push origin vx.y.z
```

GitHub Actions baut und veröffentlicht das APK automatisch.
