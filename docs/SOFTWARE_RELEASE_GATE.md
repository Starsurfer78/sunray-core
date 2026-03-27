# SOFTWARE_RELEASE_GATE.md

Stand: 2026-03-27  
Ziel: "Theoretisch voll einsatzbereit" vor erstem Test auf realem Mähroboter.

## Verwendung

- Status je Kriterium setzen: `PASS`, `FAIL`, `N/A`.
- Ein Release-Kandidat ist nur freigegeben, wenn:
1. Alle **A-Kriterien** `PASS` sind.
2. Maximal 2 **B-Kriterien** offen sind und mit Risiko dokumentiert wurden.
3. Kein offenes sicherheitskritisches Thema ohne Mitigation besteht.

## A-Kriterien (Must-Have vor Hardwaretest)

| ID | Kriterium | Prüfmethode ohne Hardware | Status |
|---|---|---|---|
| A1 | Reproduzierbarer Linux-Build (WSL2/CI) | `cmake -S . -B build_linux -G Ninja -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=build_gcc/_deps/nlohmann_json-src -DFETCHCONTENT_SOURCE_DIR_CATCH2=build_gcc/_deps/catch2-src -DFETCHCONTENT_SOURCE_DIR_ASIO=build_gcc/_deps/asio-src -DFETCHCONTENT_SOURCE_DIR_CROW=build_gcc/_deps/crow-src && cmake --build build_linux -j2` | PASS |
| A2 | Unit-/Integrationstests laufen stabil | `ctest --test-dir build_linux --output-on-failure` | PASS |
| A3 | Keine ungefangenen Exceptions in Hauptpfad | Fault-Injection in API/Map/Schedule + Logprüfung | PASS |
| A4 | State-Machine Safety-Invarianten geprüft | Tests: nie Mähmotor in `Idle`/`Error`, sichere Transitionen | PASS |
| A5 | GPS-Verluststrategie implementiert | Sim: GPS Dropout/Recovery, definierte Degradation + Timeout | PASS |
| A6 | Route/Map-Validierung vor Missionsstart | Tests für leere/inkonsistente Map, ungültige Zonen, Fallbacks | PASS |
| A7 | Auth für kritische API/WS erzwungen | `api_token` gesetzt, alle schreibenden Endpunkte + WS Commands geschützt | PASS |
| A8 | Deterministische Simulationsszenarien vorhanden | Szenarien: Bumper, Lift, MotorFault, Charger-Flattern, Battery-Drop | PASS |
| A9 | Telemetrie/Logs ausreichend für Debug | Zustandswechsel, Event-Gründe, Fehlercodes, Zeitstempel sichtbar | PASS |
| A10 | Release-Konfiguration dokumentiert | Version, Config Defaults, bekannte Grenzen, Rollback-Hinweis | PASS |

## B-Kriterien (Should-Have, hohe Wirkung auf Zuverlässigkeit)

| ID | Kriterium | Prüfmethode ohne Hardware | Status |
|---|---|---|---|
| B1 | Replay-Test aus Telemetrie-Logs | Aufgezeichnete Sequenzen reproduzierbar abspielbar | TODO |
| B2 | Rate-Limits für API/WS Commands | Lasttest: Kommandospam führt nicht zu instabilem Verhalten | TODO |
| B3 | Watchdogs für blockierende Pfade | Sim: absichtliche Verzögerungen, System bleibt steuerbar | TODO |
| B4 | Fallback-Strategien bei Routing-Fehlern | Sim: mehrfaches Path-Fail -> definierter Safe-State | TODO |
| B5 | Scheduler-Robustheit gegen Randzeiten | Tests über Tageswechsel, Wochentag, Sommerzeit | TODO |
| B6 | Konfigurationsschema validiert | Pflichtfelder, Wertebereiche, Typen, sinnvolle Defaults | TODO |
| B7 | Regressionstest für bekannte Bugs | BUG-Fixes als feste Testfälle hinterlegt | TODO |
| B8 | WebUI-Fehlerpfade verifiziert | Offline/API-Fehler/Timeouts sauber behandelt | TODO |

## C-Kriterien (Nice-to-Have vor Feldbetrieb)

| ID | Kriterium | Prüfmethode ohne Hardware | Status |
|---|---|---|---|
| C1 | KPI-Dashboard für Testläufe | Erfolgsquote pro Szenario, mittlere Recovery-Zeit | TODO |
| C2 | Performance-Budget dokumentiert | Loop-Zeit, WS-Latenz, CPU-Last mit Grenzwerten | TODO |
| C3 | Automatische Artefakte je Testlauf | Logs, Telemetrie, Build-Metadaten als Paket | TODO |
| C4 | "Release Readiness Report" automatisiert | Zusammenfassung A/B/C aus CI erzeugt | TODO |

## Empfohlene Reihenfolge

1. A1-A4 (Build, Tests, Safety-Invarianten).
2. A5-A8 (GPS-Ausfall, Sim-Robustheit).
3. A7 + B2/B6 (Security + Config-Härtung).
4. A9-A10 (Betriebsfähigkeit und Freigabedoku).

## Konkrete nächste 3 Tasks

1. GPS-Resilience-Paket definieren: Degradation, Timeout, Recovery-Hysterese.
2. 6 Pflicht-Simulationsszenarien als automatische Tests aufsetzen.
3. State-Machine-Invariantentests (`Idle/Error/Mow/Dock`) ergänzen.

## Validierter Stand

- A1 ist am 2026-03-27 auf Linux lokal validiert worden.
- Erfolgreicher Build mit frischem Buildordner: `cmake --build build_linux -j2`
- A2 ist am 2026-03-27 auf Linux lokal validiert worden.
- Erfolgreicher Testlauf: `ctest --test-dir build_linux --output-on-failure`
- Ergebnis: `167/167` Tests bestanden
- A3 ist am 2026-03-27 durch Fault-Injection und Fehlerpfad-Tests validiert worden:
  `Robot: run() catches hardware exceptions and stops safely`,
  `Robot: run() catches sensor read exceptions and stops safely`,
  `Robot: loadMap() with invalid JSON returns false without throwing`,
  `Robot: loadSchedule() with invalid JSON returns false without throwing`
- A4 ist am 2026-03-27 durch grüne Invariantentests validiert worden:
  `A4: Error state keeps motors at zero`,
  `A4: Emergency stop returns to Idle with mower off`,
  `A4: Low battery transitions Mow toward Dock safely`
- A5 ist am 2026-03-27 durch grüne GPS-Resilience-Tests validiert worden:
  `A5: startup without valid GPS transitions to GpsWait and recovers to Mow`,
  `A5: prolonged GPS loss during Mow escalates to Dock`
- A6 ist am 2026-03-27 durch grüne Map-/Routenvalidierungstests validiert worden:
  `Map: load() with inconsistent JSON returns false and clears state`,
  `Map: startMowing() returns false when no mow route is available`,
  `Map: startMowingZones() filters mow points to requested zone`,
  `Map: startMowingZones() falls back to all mow points for unknown zone`
- A7 ist am 2026-03-27 durch grüne Auth-Tests und Beispielkonfiguration validiert worden:
  `WebSocketServer: HTTP auth rejects missing token when api_token is set`,
  `WebSocketServer: HTTP auth accepts X-Api-Token header`,
  `WebSocketServer: HTTP auth accepts Bearer token`,
  `WebSocketServer: WS auth rejects command without token when api_token is set`,
  `WebSocketServer: WS auth accepts matching token`
- A8 ist am 2026-03-27 durch grüne Simulationsszenarien validiert worden:
  `A8 Scenario 1: bumper hit in Mow transitions to EscapeReverse`,
  `A8 Scenario 2: lift event in Mow transitions to Error`,
  `A8 Scenario 3: motor fault in Mow transitions to Error`,
  `A8 Scenario 4: charger flapping from Dock remains stable in Charge`,
  `A8 Scenario 5: low battery in Mow transitions to Dock`,
  `A8 Scenario 6: critical battery forces Error and loop stop`
- A9 ist am 2026-03-27 durch erweiterte Debug-Telemetrie validiert worden:
  `ts_ms`, `state_since_ms`, `event_reason`, `error_code` sind im Telemetrie-JSON sichtbar,
  abgesichert durch `WebSocketServer: buildTelemetryJson — mandatory debug keys present`
  und `WebSocketServer: buildTelemetryJson — debug fields pass through`
- A10 ist am 2026-03-27 durch die Release-Doku validiert worden:
  [`RELEASE_CONFIGURATION.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/RELEASE_CONFIGURATION.md)
- Verwendete lokale FetchContent-Quellen:
  `build_gcc/_deps/nlohmann_json-src`,
  `build_gcc/_deps/catch2-src`,
  `build_gcc/_deps/asio-src`,
  `build_gcc/_deps/crow-src`
