# TODO

Last updated: 2026-04-01

Dieses Dokument ist die einzige offene Aufgabenliste fuer `sunray-core`.
Alte Planungs-, Analyse- und Zwischenstandsdateien wurden in diese Liste verdichtet oder entfernt.

Relevante Kontextdokumente:

- [README.md](/mnt/LappiDaten/Projekte/sunray-core/README.md)
- [PROJECT_OVERVIEW.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_OVERVIEW.md)
- [PROJECT_MAP.md](/mnt/LappiDaten/Projekte/sunray-core/PROJECT_MAP.md)
- [WORKING_RULES.md](/mnt/LappiDaten/Projekte/sunray-core/WORKING_RULES.md)
- [docs/NAVIGATION_UPGRADE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/NAVIGATION_UPGRADE.md)
- [docs/OP_STATE_MACHINE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md)
- [docs/ALFRED_TEST_RUN_GUIDE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_TEST_RUN_GUIDE.md)

## Fokus Jetzt

- [~] Linux-/Pi-Build und Testlauf auf frischem Build-Verzeichnis verifizieren.
  Stand: Pi-Build wurde mit dem vorletzten GitHub-Stand bereits erfolgreich durchgefuehrt. Fuer die aktuelle Freigabe fehlt nur noch ein Build/Run gegen den neuesten Stand.
- [~] Alfred-Hardwaretest auf Raspberry Pi durchziehen: Build, Fahren, Docking, Laden, Buttons, LEDs, Buzzer.
  Stand: grossteils erfolgreich getestet. Unter Beobachtung bleiben aktuell der physische Button und die Tick-Erfassung bei hoeherer PWM.
- [ ] Tick-/Odometrie-Validierung als naechster Go/No-Go-Punkt abschliessen:
  - Geradeausfahrt ueber exakt 3 m bei normaler Maehgeschwindigkeit per Tick-Ziel
  - Plausibilisierung `ticks_per_revolution` gegen reale Strecke
  - linke/rechte Tick-Symmetrie pruefen
  - Verhalten bei hoeherer PWM messen, um Hardware-/Signalgrenzen sichtbar zu machen
- [ ] Offene Navigationsvalidierung sauber abschliessen:
  - bestehende Editorfunktionen mit optionalen Karten-Metadaten auf echter Karte pruefen
  - Replan-/Docking-/Resume-Verhalten im Feld gegen RTK-Drift absichern
- [ ] WebUI-Mapping- und Setup-Flow fertigstellen:
  - Setup-/Erststart-Assistent
  - serverseitige Kartenvalidierung
  - absolut/relativ Positionsmodus fachlich entscheiden und umsetzen
- [ ] Energie-Budget und sichere Rueckkehrberechnung vor Missionsstart einbauen.

## WebUI Und Produktreife

- [ ] Dashboard-/Map-Layout final beruhigen und auf echte Bedienung am Pi optimieren.
- [~] Diagnosebereich der WebUI fuer echte Fahrtests erweitern:
  Stand: 3-m-Streckentest, 90/180/360-Grad-Drehtests und Soll-/Ist-Anzeige fuer Strecke, Drehung und L/R-Ticks sind eingebaut. Offen bleibt die Feldvalidierung gegen reale Hardwaregrenzen und die Feinabstimmung der Test-PWM.
- [ ] Karteneditor-Komfort nachziehen:
  - Punkt auf Kante einfuegen
  - gezieltes Punkt-Loeschen weiter verbessern
  - GeoJSON Import/Export
- [ ] Obstacle-/Auto-Obstacle-Semantik fachlich klaeren und UI/Core danach vereinheitlichen.
- [ ] Nutzerfreundliche Einstellungen von Experten-/Service-Parametern trennen und die verbliebenen Servicewerte sauber in Config halten.
- [ ] Gesicherten Software-Neustart aus der WebUI nur fuer `Idle` vorbereiten.

## Navigation Und Regelung

- [ ] Echte Validierung des Navigation-Umbaus auf Hardware und in laengeren Missionen durchfuehren.
- [~] PID-/Drive-Controller als eigenen Control-Layer einfuehren:
  Stand: Differential-Drive-Controller mit PID-Korrektur aus Odometrie ist jetzt im Fahrpfad von `setLinearAngularSpeed()` verdrahtet. Offen bleiben Feldtuning, Telemetrie fuer feinere Regleranalyse und Validierung auf echter Hardware.
  - [x] PWM-Mapping aus Missions-/Op-Logik herausziehen
  - [x] Ist-Geschwindigkeit aus Odometry ableiten
  - [ ] geschlossenen Regler mit Telemetrie und Tuningpfad nachziehen
- [ ] GPS-No-Motion-Schwelle fuer RTK-Float mit echten Logs validieren.
- [ ] Watchdog-Koordination Pi <-> STM32 auf echter Hardware verifizieren.

## Alfred / Plattform

- [ ] Alfred-Betriebs- und Sicherheitsluecken gegen Originalsystem auf echter Hardware verifizieren.
- [ ] NTRIP als echten Runtime-Pfad integrieren.
- [ ] BLE unter Linux nur angehen, wenn ein konkreter Produktbedarf besteht.
- [ ] Audio/TTS, CAN/SLCAN und Kamera-/Vision nur nach Hardware-Basis und Control-Layer priorisieren.

## Langfristige Erweiterungen

- [ ] PicoRobotDriver nur starten, wenn Alfred-Pi-Stand stabil und vermessen ist.
- [ ] OTA-Update-Pfad erst nach belastbarer Plattformbasis angehen.
- [ ] Flotten-/Mesh-Funktionen nur als spaetere Produktphase behandeln.
- [ ] Lern-/Effizienzmodelle und Mobile Apps nicht vor Kernrobustheit beginnen.

## Erledigt Und Nicht Mehr Als Offener Backlog Fuehren

- [x] Navigation-Review, Sicherheitsnachzug, Costmap-/Planner-/Route-Refactor und Doku in [docs/NAVIGATION_UPGRADE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/NAVIGATION_UPGRADE.md)
- [x] WebUI-Svelte-Migration als aktive Frontend-Basis
- [x] Missionsmodell, Historie/Statistik, Telemetrie-Vertrag, Dashboard-/Diagnose-Grundlagen
- [x] Docking-/Resume-/Recovery-Fachschliessung im Core

## Arbeitsregel

Wenn ein Thema erledigt ist:

- Status hier aktualisieren
- nur die dauerhafte Fachdoku pflegen
- keine neue parallele Taskliste oder Plan-Datei anlegen, solange `TODO.md` ausreicht
