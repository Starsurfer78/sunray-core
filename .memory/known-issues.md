# Known Issues

## Zweck

Aktuelle Bugs, Workarounds und Engpaesse in einem laufenden Register.

## Aktueller Stand

## Investigating

- Produktions-Service-Layout unbekannt
  - Status: Investigating
  - Workaround: Deployment nur bewusst und nicht automatisiert behandeln; `bash scripts/check_deploy_state.sh` vor jedem echten Rollout ausfuehren

- Vollstaendiges Hardware-Binding unklar
  - Status: Investigating
  - Workaround: FACT / INFERENCE / UNKNOWN strikt trennen

- Maschinen-spezifische Runtime-Config bleibt ausserhalb des Repos
  - Status: Investigating
  - Workaround: `config.example.json` nur als kanonische Basis verwenden und Alfred-spezifische Zielwerte vor Ort pruefen

- Mow-Op nutzt degradierte EKF-Fusion bei kurzem GPS-Ausfall noch nicht aktiv aus
  - Status: Resolved
  - Workaround: nicht mehr nötig; bounded coast läuft jetzt über `mow_gps_coast_ms`

- Dock-Retries verwenden bei Kontaktfehlern weiterhin keine bewusst variierte laterale Annäherungsgeometrie
  - Status: Resolved
  - Workaround: nicht mehr nötig; spätere Retries nutzen jetzt `dock_retry_lateral_offset_m`

## Resolved

- Dock-Watchdog Startzeitproblem
- Diag-LED-Fruehreturn ohne Statusaktualisierung
- EscapeForward/EscapeReverse Operator-Mapping fuer Testszenarien
- StateEstimator-Defaultdrift zwischen no-config Tests und Runtime-Defaults
- Cross-thread WebSocket/MQTT-Kommandopfad gegen `Robot`
- Stop-Button umgeht Diag-Modus nicht mehr
- Pi-seitiger MCU-Kommunikationsverlust fuehrt jetzt in einen lokalen Fehlerzustand
- MQTT-Command-Subscription wird nach Reconnect oder Subscribe-Fehlern erneut angefordert
- Charger-Contact wird im Treiber gegen Kurzflattern gedämpft und als `charger_connected` telemetriert
- Zusätzliche Regression-Coverage deckt jetzt MQTT-Degraded-Lifecycle sowie Dock-/Charge-Flap-Szenarien ab; echte UART-Transport-Fault-Injection bleibt ohne Test-Seam offen
- Kompakte Runtime-Health-Telemetrie exponiert jetzt MCU-Comms, GPS-Degradation, Battery Guards, Recovery-Status und Watchdog-Hinweise zentral im Web-Telemetry-Contract
- Dock-bezogene GPS-Wiederaufnahme bleibt jetzt bei RTK Float in `GpsWait` und setzt den Dock-Pfad erst mit RTK Fix fort
- Dock-Retry erkennt jetzt fehlenden Bewegungsfortschritt und eskaliert frueher statt identische Stall-Retries bis zum Budget zu wiederholen
- Frontend zeigt jetzt eine sichtbare Fehlermeldung, wenn WebSocket-Kommandos nicht gesendet werden konnten
- Frontend behaelt die aktive Missionskarte jetzt auch waehrend `GpsWait`, `Escape*` und `WaitRain` sichtbar
- Encoder-basierte Stuck-Detektion löst jetzt einen dedizierten Safety-/Recovery-Pfad aus; `Mow`/`Dock` gehen auf `EscapeReverse`, `Undock` auf `Error`
- Wiederholte erfolglose Stuck-Recovery wird jetzt über `stuck_recovery_max_attempts` deterministisch zu `ERR_STUCK` / `stuck_recovery_exhausted` eskaliert
- History-Statistik liefert jetzt gruppierte Zaehler fuer Event-Reason, Event-Type und Event-Level als Basis fuer Feldoptimierung und Vorfallstatistik

## Performance Bottlenecks

- Frontend-Dependency-Installationen kosten Zeit bei leerem `node_modules`
- Backend-FetchContent benoetigt Netzwerkzugriff fuer frische Build-Setups

## Deprecated Code Sections

- Historische Altstruktur wurde entfernt; Legacy-Inhalte liegen nicht mehr im aktiven Repo

## 30min MQTT Disconnect Bug

- Status: Investigating
- Debug steps:
  1. Broker-side disconnect logs sichern
  2. Client heartbeat und reconnect logs mit Zeitstempel vergleichen
  3. Keepalive, session-expiry und auth refresh pruefen
- Hypothesen:
  - broker keepalive mismatch
  - stale session or token handling
  - reconnect path not fully restoring subscriptions
- Next checks:
  - instrument reconnect counters
  - add long-duration soak test
  - confirm broker configuration

## Update Rules

- Jeden Eintrag in genau einen Statusblock einsortieren
- Wenn geloest, kurz Ursache und Fix nennen
