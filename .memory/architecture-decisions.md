# Architecture Decisions

## Zweck

ADR-artiger Arbeitsstand fuer wichtige Architekturentscheidungen.

## Eintraege

### ADR-001: Pi owns high-level control loop

- Kontext: Alfred braucht flexible Missionslogik, Telemetrie und Integrationen
- Entscheidung: Pi-seitiger `Robot`-Loop steuert Missions- und Safety-Policy
- Konsequenzen: Safety-Logik muss in `Robot.cpp` und `core/op/*` sehr sorgfaeltig getestet werden
- Offene Risiken: MCU/Pi-Grenze ist nicht vollstaendig dokumentiert

### ADR-002: HAL abstraction isolates hardware backends

- Kontext: Serial hardware, simulation, GPS, IMU und Linux-I/O sollen austauschbar bleiben
- Entscheidung: Hardwarezugriffe laufen ueber `HardwareInterface` und spezialisierte Treiber
- Konsequenzen: Tests koennen mit MockHardware und Simulation arbeiten
- Offene Risiken: Interface-Erweiterungen koennen breit streuen

### ADR-003: Navigation state remains inside backend runtime

- Kontext: UI und MQTT sollen nicht Quelle der Wahrheit fuer Motion sein
- Entscheidung: Map, planner, line tracker, estimator und op transitions bleiben im Backend
- Konsequenzen: Backend ist komplexer, aber die Kontrolle zentral
- Offene Risiken: Dokumentation muss die Modulgrenzen klar halten

### ADR-004: Documentation reset after repo cleanup

- Kontext: Alte Docs und Legacy-Referenzen machten den Zustand unklar
- Entscheidung: Neue Doku wird aus dem aktiven Repo und verifizierten Tests aufgebaut
- Konsequenzen: Weniger Widersprueche, aber noch viele offene Hardwarefragen
- Offene Risiken: echte Feldfakten muessen noch eingesammelt werden

### ADR-005: External control commands must be serialized into the runtime loop

- Kontext: `Robot` deklariert fast alle Control-Methoden als nicht thread-safe, waehrend WebSocket- und MQTT-Callbacks aktuell direkt Start-, Dock- und Stop-Befehle aus Fremdthreads ausloesen
- Entscheidung: Externe Steuerbefehle sollen nicht direkt gegen `Robot` mutieren, sondern ueber einen kontrollierten Uebergabepunkt in den Haupt-Loop laufen
- Konsequenzen: Kommandoverarbeitung wird deterministischer, testbarer und besser mit Safety-Review vereinbar
- Offene Risiken: Umstellung muss so erfolgen, dass Bedienlatenz, Not-Stopp-Semantik und bestehende Integrationen nicht regressieren

### ADR-006: Future operator features extend existing backend surfaces, not parallel control stacks

- Kontext: Die bestaetigte Architektur hat `Robot`, `WebSocketServer`, `MqttClient`, History/Events und Diagnostics bereits als zentrale Runtime-Flaechen
- Entscheidung: Neue operator- oder service-orientierte Features sollen auf diesen bestehenden Backend-Schnittstellen aufbauen statt neue konkurrierende Steuerpfade einzufuehren
- Konsequenzen: Feature-Arbeit bleibt mit der aktuellen Pi-zentrierten Architektur kompatibel und kann bestehende Safety-Gates wiederverwenden
- Offene Risiken: Wenn bestehende Telemetrie- oder Command-Vertraege zu instabil bleiben, blockiert das mehrere spaetere Features gleichzeitig

### ADR-007: Short GPS-loss coast during mowing may be accepted while estimator fusion remains operational

- Kontext: Kurze GPS-Aussetzer treten unter Eichenkronen und an Hauskanten auf, waehrend `StateEstimator` weiterhin ueber Odometrie und IMU im Predict-Mode weiterlaufen kann
- Entscheidung: Ein begrenztes GPS-Coast-Fenster waehrend `Mow` ist architektonisch zulaessig, wenn degradierte Fusion (`EKF+IMU` oder `Odo`) noch aktiv ist und das Zeitfenster klar begrenzt bleibt
- Konsequenzen: Bessere Resilienz in Schattenzonen; gleichzeitig braucht die Runtime klar konfigurierte Grenzen, damit echte Antennenfehler nicht zu lang toleriert werden
- Offene Risiken: Die Driftgrenzen stammen aktuell aus Feldbeobachtung und nicht aus repo-verifizierter Messkampagne; konservative Defaults und echte Feldvalidierung bleiben Pflicht

## Update Rules

- Jede neue ADR braucht Kontext, Entscheidung und Konsequenzen
- Wenn eine Entscheidung spaeter revidiert wird, alten Eintrag nicht heimlich ueberschreiben
