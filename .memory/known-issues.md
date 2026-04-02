# Known Issues

## Zweck

Aktuelle Bugs, Workarounds und Engpaesse in einem laufenden Register.

## Aktueller Stand

## Investigating

- Produktions-Service-Layout unbekannt
  - Status: Investigating
  - Workaround: Deployment nur bewusst und nicht automatisiert behandeln

- Vollstaendiges Hardware-Binding unklar
  - Status: Investigating
  - Workaround: FACT / INFERENCE / UNKNOWN strikt trennen

## In Progress

- Keine aktiven Code-Defekte aus dem letzten lokalen Testlauf offen

## Resolved

- Dock-Watchdog Startzeitproblem
- Diag-LED-Fruehreturn ohne Statusaktualisierung
- EscapeForward/EscapeReverse Operator-Mapping fuer Testszenarien
- StateEstimator-Defaultdrift zwischen no-config Tests und Runtime-Defaults

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
