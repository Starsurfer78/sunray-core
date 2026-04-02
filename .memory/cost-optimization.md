# Cost Optimization

## Zweck

Praktische Regeln fuer API-/Agent-Kosten beim Arbeiten an `sunray-core`.

## Aktueller Stand

- Baseline aus Bootstrap-Vorgabe: etwa `$4.96` fuer `19` Stunden
- Repo hat genug Tiefe, dass gezielte Kontextladung wesentlich guenstiger ist als pauschales Voll-Scanning

## Regeln

### Per-Module Context Loading

- Nur betroffene Ordner laden
- Fuer Safety-Arbeit zuerst `core/Robot.cpp`, `core/op/*`, relevante Tests
- Fuer Hardwarearbeit zuerst `hal/*`, `platform/*`, Config und Tests

### Ctx-Profile Metadata

| Profile | Include |
| --- | --- |
| runtime-safety | `core/Robot.cpp`, `core/op/*`, relevante Tests |
| nav | `core/navigation/*`, `tests/test_navigation.cpp` |
| mqtt-ui | `core/MqttClient.*`, `core/WebSocketServer.*`, frontend contract docs |
| hardware-io | `hal/*`, `platform/*`, probe tools |

### Token Budget Tracking

- Vor jedem grossen Task Scope kurz fixieren
- Wiederholtes Voll-`find` und Voll-`sed` vermeiden
- Groesse PDFs oder node_modules nie unnötig laden

### Cache-Hit Strategies

- Repo-Struktur und Modulrollen in `.memory` pflegen
- Wiederkehrende Pfade und Begriffe standardisieren

### Hard-Stop Task Boundaries

- Discovery
- Fix
- Verify
- Document

## Known Unknowns

- Genaues Kostenprofil pro Tool-Pattern
- Ob spaetere Hardware-Discovery deutlich groessere Kontexte braucht

## Update Rules

- Neue kostspielige Muster dokumentieren
- Erfolgreiche Kurzprofile wiederverwendbar halten
