# RTK Context

## Zweck

Technischer Wissensspeicher fuer RTK/GNSS-bezogene Arbeit in `sunray-core`.

## Aktueller Stand

- GPS ist im Runtime-Modell zentral fuer Navigation und Degradationslogik
- `StateEstimator` fusioniert Odometrie, GPS und IMU
- `GpsWait` dient als Recovery-/Hold-Zustand
- Zielgenauigkeit fuer hochwertige RTK-Betriebsfaehigkeit: kleiner als 5 cm unter guten Bedingungen

## Strukturierte Punkte

### Baseline

- Ziel: stabile Fix-Erkennung, kontrollierte Degradierung bei Verlust
- Kritisch: Fix-Alter, Float-vs-Fix, stale data, Dockbereich mit schlechtem Himmel

### Edge Cases

- Kaltstart ohne Fix
- Float bleibt lange bestehen
- Multipath an Hauswand, Zaun oder Dock
- Teilabschattung unter Baeumen
- Spruenge durch stale corrections oder alte RTCM-Daten

### Compass / Heading

- IMU ist vorhanden, aber Feldverhalten und Kalibrierqualitaet muessen hardwareseitig belegt werden
- Magnetic declination ist als reales Feldthema zu behandeln, aber im Repo aktuell nicht als dokumentierter Fakt abgesichert

### Failover Strategien

- GPS no-signal Hysterese
- GPS fix timeout
- Resume target in `GpsWait`
- Dock- oder Error-Eskalation je nach Kontext

## Known Unknowns

- Exakte Receiver-Konfiguration auf produktiven Alfred-Einheiten
- Antennenplatzierung und Ground-Plane-Details
- RTCM/NTRIP-Produktionspfad

## Update Rules

- Nur echte Feldbeobachtungen oder codebasierte Erkenntnisse aufnehmen
- Receiver-/Firmware-Versionen immer mitschreiben, wenn bekannt
