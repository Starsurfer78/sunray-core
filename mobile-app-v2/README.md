# Sunray Mobile App v2

`mobile-app-v2` ist die aktuelle Flutter-App fuer `sunray-core` und ersetzt den frueheren `mobile-app`-Stand als produktiven Hauptpfad.

Die App verbindet sich lokal im Heimnetz direkt mit dem Roboter, ohne Cloud, und deckt den freigegebenen Kernablauf fuer Discovery, Kartenanlage, Missionen und Betrieb ab.

## Kernfunktionen

- automatische Robotersuche per mDNS plus manueller Connect
- Dashboard mit Live-Status, Karte, Start, Stop, Dock und direktem Zonenstart
- echter Karteneditor fuer Perimeter, No-Go, Dock und Zonen
- Setup-Flow fuer Karte: Grundflaeche, Ausschlussflaechen, Dock, Validierung, Speichern
- Zoneneinstellungen und Planner-Vorschau im Editor
- Missions- und Zeitplan-Flow
- Controller-Modus im Querformat mit virtuellem Joystick
- Mähmotor-Schalter mit bestaetigungspflichtigem Warnhinweis
- Service-Bereich mit Roboter-OTA und App-OTA ueber GitHub Releases
- Settings-, Notifications- und Statistics-Screens

## Technischer Stand

- Architektur: `AppController` / `AppScope`
- Navigation: `go_router`
- Kartenbasis: `flutter_map` + OpenStreetMap
- Kommunikation: HTTP ueber `dio`, Telemetrie und Commands ueber WebSocket
- Lokale Persistenz: JSON-basiert ueber `app_state_storage.dart`

## Wichtige Hinweise

- Die Android-`applicationId` lautet `de.sunray.mobile`.
- GitHub-App-OTA ist eingerichtet und erwartet Releases mit APK-Asset.
- Fuer frische manuelle Installationen steht das Release-APK in GitHub Releases bereit.
- Hardware-Abnahme auf echtem Roboter bleibt fuer Editor, Controller und OTA weiterhin sinnvoll.

## Entwicklung

```bash
cd mobile-app-v2
flutter pub get
flutter analyze
flutter test
flutter run
```

## Release-Build

```bash
cd mobile-app-v2
flutter build apk --release
```

Das Release-APK liegt danach in `build/app/outputs/flutter-apk/app-release.apk`.

## Projektstruktur

```text
lib/
├── app/                App-Shell, Router, Controller
├── core/               Konstanten und Basiskonfiguration
├── data/               Discovery, Robot API, OTA, lokale Speicherung
├── domain/             Karten-, Missions-, Robot- und Update-Modelle
├── features/           Dashboard, Map, Missions, Controller, Service, ...
└── shared/widgets/     gemeinsame UI-Bausteine
```

## Abgrenzung zu `mobile-app`

- `mobile-app-v2` ist der aktive Produktpfad.
- `mobile-app` bleibt vorerst nur als Altstand und Referenz fuer noch nicht uebernommene Details im Repository.
