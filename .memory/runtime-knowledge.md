# Runtime Knowledge

## Zweck

Kompakte Sammlung von Boot- und Laufzeitwissen fuer `sunray-core`.

## Aktueller Stand

- `main.cpp` bootstrapt Backend-Runtime
- `Robot` ist das zentrale Laufzeitobjekt
- Safety guards laufen vor normaler Zustandsfortschreibung
- Telemetry wird ueber WebSocket und optional MQTT bereitgestellt

## Service Order

### FACT

- Exakte Produktions-Service-Reihenfolge ist noch nicht dokumentiert

### INFERENCE

- Netzwerk und serieller Hardwarezugang muessen rechtzeitig verfuegbar sein
- MQTT und UI sollten nicht die Kernsteuerung blockieren

## Startup Assumptions

- Konfigurationsdefaults sind brauchbar genug fuer Software-Start
- Hardware init muss erfolgreich sein, sonst wird abgebrochen
- History DB init ist Teil des erfolgreichen Initialisierungswegs

## Reconnect Behavior

- MQTT reconnect semantics sind nicht zentral dokumentiert
- GPS recovery hat Hysterese-Logik
- Dock/charge transitions reagieren auf chargerConnected state

## Watchdog Chain

- Runtime hat op-spezifischen Watchdog, zum Beispiel fuer Docking
- Externe Watchdog-Komponenten sind nicht hart dokumentiert

## Crash Handling Assumptions

- Unhandled exceptions in `Robot::run()` fuehren zu Motor-Stop und Loop-Stop
- Nicht belegt ist, ob ein externer Supervisor den Prozess neu startet

## Deployment Runtime Notes

- `build_verify` ist lokales Verifikationsartefakt und nun in `.gitignore`
- Frontend-Artefakte sollen nicht ungeprueft als Quellzustand betrachtet werden

## Known Unknowns

- Systemd services
- Logrotate / journald strategy
- On-device path layout for deployed binaries and configs

## Update Rules

- Nur belastbare Laufzeitannahmen dokumentieren
- Wenn etwas nur inferred ist, direkt markieren
