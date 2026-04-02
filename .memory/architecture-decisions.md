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

## Update Rules

- Jede neue ADR braucht Kontext, Entscheidung und Konsequenzen
- Wenn eine Entscheidung spaeter revidiert wird, alten Eintrag nicht heimlich ueberschreiben
