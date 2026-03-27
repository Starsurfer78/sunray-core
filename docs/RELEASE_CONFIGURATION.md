# RELEASE_CONFIGURATION.md

Stand: 2026-03-27
Ziel: Freigaberelevante Konfiguration und Betriebsgrenzen fuer den ersten Hardwaretest.

## Release-Version

- Pi/Core-Version: `0.1.0`
  Quelle: [`CMakeLists.txt`](/mnt/LappiDaten/Projekte/sunray-core/CMakeLists.txt)
- MCU-Version:
  wird zur Laufzeit ueber Telemetrie/API gemeldet (`mcu_v`, `GET /api/version`)

## Pflichtdateien vor Hardwaretest

- Konfiguration: `config.json`
  Ausgangsbasis: [`config.example.json`](/mnt/LappiDaten/Projekte/sunray-core/config.example.json)
- Karte: `map.json`
- Optional: `schedule.json`

## Release-Pflichtwerte

- `driver`: fuer echten Roboter auf `serial`
- `api_token`: muss gesetzt werden, nicht leer lassen
- `map_path`: muss auf die reale Karten-Datei zeigen
- `gps_port`: muss zum angeschlossenen GPS-Empfaenger passen
- `driver_port`: muss zum MCU-UART passen
- `enable_mow_motor`: fuer echten Maehbetrieb nur `true`, fuer Trockenlauf optional `false`

## Wichtige Defaults

- `battery_low_v`: `25.5`
- `battery_critical_v`: `18.9`
- `gps_no_signal_ms`: `3000`
- `gps_fix_timeout_ms`: `120000`
- `gps_recover_hysteresis_ms`: `1000`
- `dock_retry_max_attempts`: `3`
- `preflight_min_voltage`: `26.5`
- `preflight_gps_timeout_ms`: `60000`

## Bekannte Grenzen

- Linux-Build ist aktuell mit lokal vorliegenden FetchContent-Quellen validiert worden, nicht mit frischem Online-Download.
- Auth ist fuer REST/WS abgesichert, aber noch ohne echten End-to-End-Netzwerktest gegen einen laufenden Server validiert.
- Telemetrie ist fuer Debug erweitert, aber es gibt noch kein Replay-System und kein automatisiertes Artefakt-Paket.
- Hardwarefreigabe auf realem Roboter ist noch ausstehend; dieses Dokument ersetzt keinen Pi- oder Feldtest.

## Rollback

Bei Problemen vor oder waehrend des ersten Hardwaretests:

1. Auf die zuletzt bekannte funktionierende `config.json` und `map.json` zurueckgehen.
2. Falls noetig `enable_mow_motor=false` setzen und nur Trockenlauf/Diagnose fahren.
3. Bei unsicherem Verhalten nur mit `--sim` oder ohne aktiven Maehmotor weiter pruefen.
4. Release-Kandidat nicht auf den realen Roboter geben, solange ein A-Kriterium wieder offen ist.

## Freigabe-Check

- Linux-Build gruen
- Gesamte Testsuite gruen
- `api_token` gesetzt
- reale `config.json` vorbereitet
- reale `map.json` vorbereitet
- Pi/Core-Version bekannt
- Rollback-Dateien gesichert
