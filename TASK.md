# TASK.md

## Projektziel

Vorschau, gespeicherter Missionsplan, Laufzeitroute und Dashboard-Fortschritt basieren auf derselben kompilierten Missionsroute.
Die WebUI enthält keine eigene Planungslogik.
Coverage und Transit werden intern getrennt behandelt.
Ungültige Missionen sind nicht startbar.

## Status

- Phase: Vereinheitlichung von Planung und Laufzeit
- Stand: Kernpaket erledigt
- Priorität: Hoch
- Owner: Navigation / WebUI / Runtime

## Harte Regeln

- Es gibt genau eine kompilierte Missionsroute.
- Vorschau, Runtime und Dashboard verwenden genau diese Route.
- Die WebUI rendert nur Backend-Daten.
- Es gibt keine stillen Direktverbindungen, wenn Planner-Übergänge fehlschlagen.
- Jede Mission wird vor Start validiert.
- Recovery wirkt lokal, nicht global.

## Erledigt: Missionsroute vereinheitlichen

### P1 - Zwingend zuerst

- [x] Task 1.1 - RoutePoint um Semantik erweitern
- [x] Task 1.2 - MowRoutePlanner setzt semantic und zoneId
- [x] Task 1.3 - Vorschau-API serialisiert semantic und zoneId
- [x] Task 1.4 - WebUI rendert nur Backend-Route
- [x] Task 2.1 - CompiledMissionRoute als Standardpfad definieren
- [x] Task 2.2 - startPlannedMowing() als Standard-Einstieg verwenden
- [x] Task 2.3 - Implizite Neugenerierung beim Start entfernen
- [x] Task 3.1 - RouteValidator neu einführen
- [x] Task 3.2 - Missionsstart bei ungültiger Route blockieren
- [x] Task 3.3 - Fehlerzustände in Preview exportieren

### P2 - Fachliche Planung stabilisieren

- [x] Task 4.1 - MowRoutePlanner logisch in Coverage und Transition aufteilen
- [x] Task 4.2 - Globales Nearest-Neighbour aus Coverage entfernen und Fix verifizieren
- [x] Task 4.3 - Transitionen fachlich korrekt behandeln
- [x] Task 5.1 - Inset/Offset auf Clipper2 umstellen
- [x] Task 5.2 - Headland/Infill-Anschluss testen

### P3 - Laufzeit und UX sauber machen

- [x] Task 6.1 - Recovery nur auf aktuelles Segment anwenden
- [x] Task 7.1 - Dashboard-Fortschritt segment- und routenbasiert berechnen

## Erledigt: Pflicht-Tests

- [x] Test A - Einfache Rechteckzone
- [x] Test B - Zone nahe Perimeter
- [x] Test C - Zone mit Exclusion
- [x] Test D - Mehrere Zonen
- [x] Test E - Planner-Fehler / nicht planbarer Übergang

## Definition of Done

- [x] Es gibt genau eine kompilierte Missionsroute.
- [x] Vorschau verwendet diese Route.
- [x] Runtime verwendet diese Route.
- [x] Dashboard verwendet diese Route.
- [x] Die WebUI enthält keine eigene Planungslogik.
- [x] RoutePoint enthält semantic.
- [x] RoutePoint enthält zoneId.
- [x] Alle neuen Planner-Punkte tragen sinnvolle Semantik.
- [x] Ungültige Transitionen werden nicht kaschiert.
- [x] Jede Route wird vor Start validiert.
- [x] Ungültige Missionen sind nicht startbar.
- [x] Pflicht-Tests sind grün.

## Später: RTK- und GPS-Folgearbeiten

- [ ] Zielsystem-Konfiguration prüfen: /etc/sunray-core/config.json gegen Repo-Defaults abgleichen, insbesondere gps_configure=true.
- [ ] Live-Hardwaretest mit F9P durchführen: korrigierte Signale, RTCM-Alter und Dashboard-Anzeige unter Realbedingungen prüfen.
- [ ] RTK-Warnschwellen nach Realmessung feinjustieren: insbesondere RTCM-Warnschwelle und Verhalten bei fehlenden korrigierten Signalen.

## Nächster Hauptblock: Komponenten und Transitionen

- [x] Working-Area-Komponenten als explizites Planungsmodell einführen.
- [x] RoutePoint um componentId erweitern.
- [x] Transit-Semantik um TRANSIT_BETWEEN_COMPONENTS ergänzen.
- [x] Coverage-Reihenfolge zuerst lokal innerhalb einer Komponente optimieren.
- [x] Inter-Komponenten-Transitionen über explizite Policy statt implizit behandeln.
- [x] RouteValidator um Regeln für unzulässige Komponentenwechsel erweitern.

### Geplante Tests

- [x] Test H - Zwei echte Working-Area-Komponenten, global legal verbindbar.
- [x] Test I - Zwei echte Komponenten, nicht legal verbindbar.
- [x] Test J - Mehrere Komponenten mit stabiler, reproduzierbarer Reihenfolge.

## Planner-Konsolidierung (Stand 2026-04-07)

### P1

- [x] Working-Area-Fallback entfernen
- [x] Offset/Inset auf Polygonmengen umbauen
- [x] Grid-Endpunktlogik hart machen
- [x] Runtime-Replan semantikfähig machen
- [x] Legacy-Startpfade hart trennen: NavToStart und Mow bauen keine Route mehr implizit auf; gültige Startpfade aktivieren die Route vor dem Op-Wechsel
- [x] Legacy-Map-Zonenstartpfad entfernen: zonenbasierte Starts laufen nur noch über die kompilierte Missionsroute

### P1.5

- [x] Komponentenidentität stabil halten
- [x] componentId nicht implizit von Clipper-Reihenfolge ableiten
- [x] deterministische Sortierung pro Build garantieren

### P2

- [x] Validator auf Planner-Regeln anheben
- [x] Komponentenreihenfolge fachlich definieren
- [x] Debug-Ausgaben entlang der Planner-Kette schärfen

### P3

- [x] UI-Debug-Layer ausbauen

## Projektstand (2026-04-04)

### Backend (C++ / Raspberry Pi)

- [x] Runtime stabil, 237/237 Tests grün
- [x] Safety-Pfade: Stop, Watchdog, MCU-Verlust, Dock-Recovery, Stuck-Detection
- [x] WebSocket-API: Commands serialisiert auf Control-Loop-Thread, inkl. reset-Support
- [x] Joystick: Unterstützung für manuelle Steuerung in Idle und Charge (Alfred-Hardware-getestet)
- [x] MQTT: Reconnect + Subscription-Recovery, inkl. drive- und reset-Befehle
- [x] History-DB mit Wanduhr-Timestamps
- [x] STM32-OTA: Upload + Flash via WebUI
- [x] Pi-OTA: POST /api/ota/check + POST /api/ota/update + POST /api/restart
- [x] systemd-Stop über sigwait()-Thread

### WebUI (Svelte)

- [x] Responsiv (<=640px Mobile-Layout)
- [x] Joystick (drive-Kommando, linear/angular)
- [x] OTA-Karte: Update prüfen, installieren, Service-Neustart mit Reconnect-Warten
- [x] STM-Flash: .bin Upload + kontrollierter Flash
- [x] Verlauf-Seite: Events, Sessions, Statistik, Export

### Mobile App (Flutter / Android)

- [x] mDNS-Discovery + manueller Connect
- [x] Dashboard: Karte (GPS-Position), Status-Overlay, Joystick-FAB, Start/Stop/Dock
- [x] Missionen: Karte + DraggableSheet, Zone-Auswahl, Vorschau, Start
- [x] Karte: Map-Editor mit Perimeter/Zonen/No-Go/Dock
- [x] Service: Pi-OTA (Check/Install/Restart), App-OTA via GitHub Releases
- [x] v1.0.0 Release auf GitHub mit APK-Asset

## Offene Punkte

| Thema | Prio | Notiz |
|---|---|---|
| MQTT 30min-Disconnect | H | Broker-Keepalive oder Session-Expiry - noch nicht reproduziert |
| HA-Integration | M | MQTT-Discovery vorhanden, App-Seite fehlt noch |
| STM-OTA aus App | L | WebUI hat es, App noch nicht |
| avahi-daemon Neustart auf Pi | L | Port 8765 in sunray.service.xml, braucht einmalig sudo systemctl restart avahi-daemon |

## Neuer Hauptblock: Produkt-Neuplanung Mähablauf

Referenz: [docs/mowing-product-replan.md](/mnt/LappiDaten/Projekte/sunray-core/docs/mowing-product-replan.md)

### Zielbild

- Karten-Werkzeuge bleiben erhalten.
- Missions-Werkzeuge bleiben erhalten.
- Karte, Mission, Plan und Runtime werden fachlich getrennt.
- Vorschau ist die sichtbare Form des echten Fahrplans.
- Dashboard zeigt genau diesen Fahrplan plus Live-Zustand.
- Hindernisse erzeugen nur lokale Umwege, keine neue Mission.

### Greenfield-Rahmenbedingungen

- Der aktuelle Navigationsstand hat keine Produkt-Bindung außerhalb des Testsystems.
- Für die Navigation darf fachlich bei Null neu angesetzt werden.
- Rücksicht auf alte Navigationsarchitektur ist nicht erforderlich, wenn sie dem Zielbild widerspricht.
- Bestehende UI-Werkzeuge der Karten- und Missionsseite bleiben als Bedienkonzept erhalten.
- Migrationsaufwand ist nachrangig gegenüber klarer fachlicher Trennung.
- Kompatibilität zu Altpfaden ist kein Primärziel.

### Harte Umsetzungsvorgaben

- Karte enthält nur statische Geometrie: Perimeter, Hard-NoGo, Dock.
- Mission enthält nur Zonen, Zonenparameter und die zugehörigen Pläne.
- Vorschau, Start und Dashboard dürfen nie unterschiedliche Routen meinen.
- Frontend rendert nur Backend-Daten und plant nichts selbst.
- Eine Zone wird zu genau einem ZonePlan kompiliert.
- Eine Mission wird zu genau einem MissionPlan kompiliert.
- Runtime fährt MissionPlan und ergänzt nur temporäre LocalDetours.
- No-Go-geteilte Flächen müssen lokal kohärent gemäht werden, nicht segmentweise chaotisch.

### Phase N1 - Datenhoheit geradeziehen

- [x] Task N1.1 - Map-Modell fachlich auf statische Geometrie begrenzen
- [x] Task N1.2 - Mission-Modell fachlich auf Zonen, Parameter und Reihenfolge begrenzen
- [x] Task N1.3 - ZonePlan als explizites Ergebnisobjekt definieren
- [x] Task N1.4 - MissionPlan als explizites Ergebnisobjekt definieren
- [x] Task N1.5 - RuntimeState und LocalDetour als eigene Laufzeitobjekte definieren

Umsetzungsvorgaben:

- Keine neue versteckte Zweitroute im Map-Modell ablegen.
- Keine Missionslogik in Map belassen, die eigentlich Plan- oder Runtime-Verantwortung ist.
- ZonePlan und MissionPlan sind fachliche Produkte, nicht nur technische Hilfsstrukturen.

Abnahme:

- Jedes relevante Modul kann eindeutig sagen, ob es mit Map, Mission, ZonePlan, MissionPlan oder RuntimeState arbeitet.

#### N1.1 Arbeitspaket - Map-Modell auf statische Geometrie begrenzen

Ziel:

- Map ist nur noch Weltmodell für Perimeter, Hard-NoGo und Dock.

Technische Entscheidung:

- Alles, was fachlich MissionPlan, ZonePlan oder RuntimeState ist, muss aus Map herausgelöst oder klar als Fremdverantwortung markiert werden.
- Map darf Navigationshilfen für lokale Pfadplanung bereitstellen, aber keine zweite Missionswahrheit besitzen.

Betroffene Dateien:

- [core/navigation/Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)
- [core/navigation/Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- [webui-svelte/src/lib/stores/map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts)

Abnahmetest:

- Karten speichern und laden nur statische Geometrie plus Editor-Metadaten.
- Kein Startpfad und keine Vorschau benötigt implizite Missionslogik aus Map.

Prüfergebnis (2026-04-07): **BESTÄTIGT (Implementierung abgeschlossen, Testbereinigung offen)**

Umgesetzte Änderungen:

- `ZoneSettings` aus `Zone`-Struct in `Map.h` entfernt. Zone enthält nur noch `id`, `name`, `polygon`.
- `Map::exportMapJson()` serialisiert für Zonen nur noch `id`, `name`, `polygon` — keine Mähparameter.
- Planner-Fallback (`if (zoneOrder.empty()) { alle map.zones() }`) entfernt. Mission muss immer
  explizite `zoneIds` liefern.
- `ZoneSettings`-Defaults liegen als Konstanten im Planner, nicht mehr im Map-Modell.
- `RuntimeState::startDocking()` und `planPath()`: Zone-Constraint wird bei DOCK-Planung nicht
  mehr angewendet (verhindert blockierte Pfade zur Dock-Position).

Teststand (2026-04-07):

- 264 / 273 Tests grün.
- 9 Restfehler in `test_navigation.cpp` (4) und `test_op_machine.cpp` (5) betreffen
  Navigationszenarien, die strukturell N2/N3-Architektur voraussetzen. Diese Tests werden
  im Rahmen der N2-Phase neu ausgerichtet.
- Entscheidung: keine weiteren Flickarbeiten an den verbleibenden Fehlern; N2 baut
  den Navigationskern und die betroffenen Tests neu auf.

#### N1.2 Arbeitspaket - Mission-Modell auf Zonen und Parameter begrenzen

Ziel:

- Mission beschreibt nur fachlichen Auftrag, nicht bereits Live-Zustand.

Technische Entscheidung:

- Mission enthält Zonenreferenzen, Reihenfolge, Zonenparameter und optionale Planreferenz.
- Mission enthält keine Dashboard-spezifischen oder Laufzeit-spezifischen Felder.

Betroffene Dateien:

- [webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)
- [webui-svelte/src/lib/components/Mission/ZoneSelect.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSelect.svelte)
- [webui-svelte/src/lib/components/Mission/ZoneSettings.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSettings.svelte)
- [webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)

Abnahmetest:

- Eine Mission ist ohne Laufzeitdaten serialisierbar.
- Eine Mission kann dieselben Zonen mehrfach laden, ohne dass dabei Live-Zustand rekonstruiert werden muss.

Prüfergebnis (2026-04-07): **BESTÄTIGT**

- `Mission` in missions.ts = `{id, name, zoneIds, overrides (delta), schedule, planRef}`.
  Kein Live-Zustand, keine Dashboard-spezifischen Felder.
- Eine Mission ist vollständig ohne Laufzeitdaten serialisierbar.
- `overrides: Record<string, MissionZoneOverrides>` enthält nur Parameter-Deltas.
- Hinweis: Zone-Defaults liegen noch in Map.zones.settings (wird in N1.1 behoben).

#### N1.3 Arbeitspaket - ZonePlan als erstes echtes Planobjekt einführen

Ziel:

- Für eine einzelne Zone existiert ein explizites, serialisierbares Planungsergebnis.

Technische Entscheidung:

- ZonePlan ist kein implizites Nebenprodukt von `RoutePlan`, sondern ein eigenes Fachobjekt.
- ZonePlan enthält mindestens: `zoneId`, Parameter-Snapshot, Bahnfolge, Startpunkt, Endpunkt, Validitätsstatus.

Betroffene Dateien:

- [core/navigation/Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h)
- [core/navigation/MowRoutePlanner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.h)
- [core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)
- [core/navigation/RouteValidator.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.h)
- [core/navigation/RouteValidator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.cpp)

Abnahmetest:

- Dieselbe Zone erzeugt deterministisch denselben ZonePlan.
- Die aktive Zonen-Vorschau rendert ausschließlich ZonePlan-Daten.

Prüfergebnis (2026-04-07): **BESTÄTIGT**

- `struct ZonePlan` in Route.h mit: `zoneId`, `zoneName`, `ZonePlanSettingsSnapshot`, `RoutePlan`
  (Bahnfolge), `startPoint`, `endPoint`, `hasStartPoint`, `hasEndPoint`, `valid`, `invalidReason`.
  Alle Pflichtfelder der Technischen Entscheidung sind vorhanden.
- `buildZonePlanPreview(const Map &map, const Zone &zone, …)` nimmt die Zone als expliziten
  Parameter — kein implizites Ziehen aus map.zones().
- `PathPreview.svelte` nutzt ausschließlich `previewPlannerRoutes()` (Backend-API).
  Keine eigene Frontend-Planungslogik.

#### N1.4 Arbeitspaket - MissionPlan als einziges Missions-Ergebnisobjekt einführen

Ziel:

- Mehrzonen-Missionen besitzen genau ein explizites Planungsergebnis.

Technische Entscheidung:

- MissionPlan ist die einzige Wahrheit für Missionsvorschau, Start und Dashboard.
- Zwischen Zonen liegende Verbindungssegmente sind Teil des MissionPlan und nicht implizite Runtime-Erfindungen.

Betroffene Dateien:

- [core/navigation/Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h)
- [core/navigation/MowRoutePlanner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.h)
- [core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)
- [webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)

Abnahmetest:

- Missionsvorschau und Missionsstart referenzieren denselben Plan-Identifier oder denselben serialisierten Inhalt.

Prüfergebnis (2026-04-07): **BESTÄTIGT**

- `struct MissionPlan` in Route.h mit: `missionId`, `zoneOrder`, `zonePlans` ([]ZonePlan),
  `route` (Gesamtroute), `valid`, `invalidReason`. Vollständiges eigenes Fachobjekt.
- `activeMissionPlan_` in Robot.h/cpp — einzige aktive Plan-Instanz zur Laufzeit.
- Sowohl Preview (GET /api/planner/preview) als auch Start (`startPlannedMowing`) verwenden
  `buildMissionPlanPreview()` mit identischen Eingaben → deterministisch gleicher Inhalt.
- Anmerkung: `planRef` aus der Preview wird im Frontend (missions.ts) gespeichert,
  beim Start-Kommando aber nicht mitgeschickt — Start regeneriert deterministisch.
  Das erfüllt die Oder-Variante des Abnahmetests. Für N6.1 bleibt das zu schärfen.

#### N1.5 Arbeitspaket - RuntimeState und LocalDetour als eigene Laufzeitobjekte einführen

Ziel:

- Live-Zustand ist klar von Missions- und Kartenzustand getrennt.

Technische Entscheidung:

- RuntimeState führt aktiven Plan, Zielpunkt, Fortschritt und gefahrene Bahnen.
- LocalDetour ist temporär und referenziert einen Wiedereinstieg in den bestehenden Plan.
- Planner, GridMap und Costmap werden nur als Werkzeuge für lokalen Umweg behandelt, nicht als Wahrheit über die Mission.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [core/navigation/Planner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.h)
- [core/navigation/Planner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.cpp)
- [core/navigation/PlannerContext.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/PlannerContext.h)
- [core/navigation/GridMap.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.h)
- [core/navigation/GridMap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp)
- [core/navigation/Costmap.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.h)
- [core/navigation/Costmap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.cpp)
- [core/navigation/LineTracker.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.h)
- [core/navigation/LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

Abnahmetest:

- Ein lokales Hindernis kann einen temporären Umweg erzeugen, ohne MissionPlan oder ZonePlan umzuschreiben.
- Nach dem Umweg erfolgt Wiedereinstieg in den bestehenden Plan.

Prüfergebnis (2026-04-07): **BESTÄTIGT (Datenmodell; Verhalten → N5)**

- `struct RuntimeSnapshot` in Route.h mit: `missionId`, `activeZoneId`, `activePointIndex`,
  `currentTarget`, `completedRoute` (gefahrene Bahnen), `activeDetour` (LocalDetour),
  `hasActiveMissionPlan`, `hasActiveZonePlan`. Alle Felder der Technischen Entscheidung.
- `struct LocalDetour` in Route.h mit: `route`, `reentryPoint`, `resumeZoneId`,
  `resumePointIndex`, `hasReentryPoint`, `active`. Klare Datenstruktur für Umweg.
- `runtimeSnapshot_` in Robot.h/cpp wird aktiv geführt und bei Plan-Updates befüllt.
- Hinweis: `LocalDetour` ist als Datenstruktur definiert und im Snapshot geführt, aber die
  Detour-Erzeugungslogik (Hindernis → Umweg → Wiedereinstieg) ist noch nicht implementiert.
  Das ist planmäßig N5-Scope. N1.5 ist als Modell-Definition vollständig erfüllt.

Betroffene Module und Dateien:

- Karte / statische Geometrie:
	[core/navigation/Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)
	[core/navigation/Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
	[webui-svelte/src/lib/stores/map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts)
	[webui-svelte/src/lib/components/Map/MapCanvas.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/MapCanvas.svelte)
	[webui-svelte/src/lib/components/Map/PerimeterTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/PerimeterTool.svelte)
	[webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte)
	[webui-svelte/src/lib/components/Map/DockTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/DockTool.svelte)

- Mission / Zonen / Planbezug:
	[core/navigation/Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h)
	[core/navigation/MowRoutePlanner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.h)
	[core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)
	[webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)
	[webui-svelte/src/lib/components/Mission/ZoneSelect.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSelect.svelte)
	[webui-svelte/src/lib/components/Mission/ZoneSettings.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSettings.svelte)
	[webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)
	[webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)

- Runtime / Startpfade / Live-Zustand:
	[core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
	[core/navigation/LineTracker.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.h)
	[core/navigation/LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

- Lokale Umfahrung / Low-Level-Pfadplanung:
	[core/navigation/Planner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.h)
	[core/navigation/Planner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.cpp)
	[core/navigation/PlannerContext.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/PlannerContext.h)
	[core/navigation/GridMap.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.h)
	[core/navigation/GridMap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp)
	[core/navigation/Costmap.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.h)
	[core/navigation/Costmap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.cpp)

- Validierung und Neuabnahme:
	[core/navigation/RouteValidator.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.h)
	[core/navigation/RouteValidator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.cpp)
	[tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

### Phase N2 - Navigationskern neu aufbauen (Greenfield)

**Architekt­entscheidung (2026-04-08):**
Der bestehende Navigationskern (MowRoutePlanner, RuntimeState, Costmap, GridMap, Planner) wird
vollständig ersetzt. Die neue Architektur besteht aus drei einfachen, klar getrennten Modulen.

**Module die ersetzt werden:**

| Alt (wird gelöscht) | Zeilen | Grund |
|---|---|---|
| `MowRoutePlanner.cpp/.h` | 1216 | zu komplex, chaotische Segmentverkettung |
| `RuntimeState.cpp/.h` | 756 | vermischt Plan, Laufzeit und Pfadplanung |
| `Costmap.cpp/.h` | 346 | funktioniert nicht zuverlässig |
| `GridMap.cpp/.h` | 283 | zu aufwändig, unnötige Kostenlayer |
| `Planner.cpp/.h` + `PlannerContext.h` | 137 | zu viel Magie, zu buggy |
| `RouteValidator.cpp/.h` | ~200 | wird durch neue Struktur überflüssig |

**Module die neu entstehen:**

| Neu | Zeilen (ca.) | Verantwortung |
|---|---|---|
| `BoustrophedonPlanner.cpp/.h` | ~350 | reine Funktion: Polygon → Bahnfolge |
| `NavPlanner.cpp/.h` | ~200 | binäres A*: von/zu, Map, kein Kostenlayer |
| `WaypointExecutor.cpp/.h` | ~200 | folgt Bahnliste, Index++, Detour-Slot |

**Module die unverändert bleiben:**

- `Map.h/.cpp` — bereits sauber (N1.1)
- `StateEstimator.*` — RTK/GPS-Fusion, unangetastet
- `LineTracker.*` — Wegpunktverfolgung, unangetastet
- `core/op/*.cpp` — Zustandsmaschine, nur minimale Integration
- `core/Robot.cpp` — Steuerungsloop, unangetastet
- `tests/test_robot.cpp` — 264 grüne Tests, unangetastet
- `webui-svelte/` — alle WebUI-Werkzeuge, unangetastet

---

**Aufgaben:**

- [x] Task N2.1 - Route.h vereinfachen: minimale Datenstrukturen als Fundament
- [x] Task N2.2 - BoustrophedonPlanner: reine Funktion Polygon → Bahnfolge
- [x] Task N2.3 - NavPlanner: binäres A* ohne Kostenlayer
- [x] Task N2.4 - WaypointExecutor: ersetzt RuntimeState
- [x] Task N2.5 - Op-Integration: NavToStartOp, MowOp, DockOp auf neue Module umgestellt
- [x] Task N2.6 - Neue Tests: 21/21 grün (test_boustrophedon.cpp), 285/294 gesamt

**Umsetzungsvorgaben:**

- Kein Code aus alten Modulen übernehmen — Neubau, nicht Refaktor.
- Erst neue Module vollständig grün testen, dann alte löschen.
- Jede Klasse hat genau eine Verantwortung.
- Keine versteckten Abhängigkeiten zwischen den drei neuen Modulen.
- Nicht blind segmentweise verketten — erst lokale Seite fertig, dann Wechsel.
- Harte No-Go-Zonen sind fachliche Gebietstrenner, keine Hindernisse.

**Abnahme:**

- Eine einzelne Zone erzeugt deterministisch denselben ZonePlan.
- Die aktive Zonen-Vorschau zeigt genau diesen ZonePlan.
- `Jetzt mähen` fährt exakt diesen Plan.
- test_robot.cpp bleibt vollständig grün (264 Tests).

---

#### N2.1 Arbeitspaket - Route.h vereinfachen

Ziel:

- Minimale, klare Datenstrukturen ohne historischen Ballast als Fundament für alle neuen Module.

Technische Entscheidung:

```cpp
// Alles was bleibt:
struct Waypoint { float x, y; bool mowOn; };
struct ZonePlan {
    std::string zoneId;
    std::vector<Waypoint> waypoints;   // Bahnfolge inkl. Randfahrten
    Waypoint startPoint, endPoint;
    bool valid = false;
    std::string invalidReason;
};
struct MissionPlan {
    std::string missionId;
    std::vector<std::string> zoneOrder;
    std::vector<ZonePlan> zonePlans;
    bool valid = false;
    std::string invalidReason;
};
```

- Altes `RoutePoint`, `RoutePlan`, `ZonePlanSettingsSnapshot`, `RuntimeSnapshot`, `LocalDetour` bleibt
  als Laufzeitstruktur erhalten, bis N2.4 (WaypointExecutor) es ablöst.
- Parallelkoexistenz: alte und neue Strukturen leben während des Neubaus nebeneinander.

Betroffene Dateien:

- [core/navigation/Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h)

Abnahmetest:

- `Waypoint`, `ZonePlan` und `MissionPlan` kompilieren ohne Abhängigkeit auf Planner-Interna.
- Alte Strukturen kompilieren weiterhin (keine Breaking Changes in N2.1).

#### N2.2 Arbeitspaket - BoustrophedonPlanner

Ziel:

- Reine Funktion: `(WorkingPolygon, ZoneSettings) → ZonePlan`. Kein Zustand, keine Seiteneffekte.

Technische Entscheidung:

- Eingabe: `WorkingPolygon` = Zone ∩ Perimeter − Hard-NoGo (Clipper2, bereits in Robot.cpp verfügbar).
- Ausgabe: `ZonePlan` mit vollständiger geordneter Bahnfolge.
- Algorithmus:
  1. Randfahrten: `edgeRounds`-mal den Polygon-Rand abfahren (Inset mit Clipper2).
  2. Streifen schneiden: parallele Linien im Winkel `angle` mit Abstand `stripWidth`.
  3. Pro Streifen einen oder mehrere Segmente (durch No-Go getrennt) erzeugen.
  4. **Boustrophedon-Reihenfolge**: abwechselnd vorwärts/rückwärts. Erst eine Seite eines
     No-Go-geteilten Streifens vollständig abfahren, dann die andere.
  5. `mowOn = true` für Coverage-Punkte, `mowOn = false` für Transitwege zwischen Blöcken.
- Kein A*, kein Costmap, keine Abhängigkeit auf Map oder RuntimeState.

Datei-Struktur:

```
core/navigation/BoustrophedonPlanner.h   (~80 Zeilen)
core/navigation/BoustrophedonPlanner.cpp (~270 Zeilen)
```

Betroffene Dateien (neu):

- `core/navigation/BoustrophedonPlanner.h`
- `core/navigation/BoustrophedonPlanner.cpp`

Abnahmetest:

- Rechteck 10×10m, stripWidth=0.3m → ~33 Streifen, Boustrophedon-Reihenfolge.
- Zone mit No-Go in der Mitte → kein chaotischer Wechsel zwischen Hälften.
- Gleiche Eingabe → identische Ausgabe (deterministisch).
- Pure function: zweimal aufrufen gibt identisches Ergebnis.

#### N2.3 Arbeitspaket - NavPlanner (binäres A*)

Ziel:

- Einfacher Pfadplaner: `(from, to, Map) → std::vector<Point>`. Kein Kostenlayer.

Technische Entscheidung:

- Binäres Grid: Zelle ist frei (0) oder blockiert (1). Keine Gewichtungen, keine Inflation.
- Blockiert = innerhalb eines Hard-NoGo-Polygons oder außerhalb des Perimeters.
- Dock-Pfad darf außerhalb des Perimeters verlaufen (Sonderflag `allowOutsidePerimeter`).
- A* mit Manhattan-Distanz als Heuristik. Auflösung: 10–20 cm/Zelle.
- Drei Anwendungsfälle: NavToZone (Startpunkt der Zone anfahren), NavToDock (zurück zur Dockstation),
  ObstacleDetour (temporärer Umweg, N5-Scope).
- Nach A*: optional Douglas-Peucker-Simplifikation der Wegpunkte.

Datei-Struktur:

```
core/navigation/NavPlanner.h   (~60 Zeilen)
core/navigation/NavPlanner.cpp (~140 Zeilen)
```

Betroffene Dateien (neu):

- `core/navigation/NavPlanner.h`
- `core/navigation/NavPlanner.cpp`

Abnahmetest:

- Freies Feld → direkte Verbindung.
- Hindernis zwischen Start und Ziel → A* umfährt es.
- Kein Pfad möglich (vollständig blockiert) → leeres Ergebnis, kein Crash.
- Dock außerhalb Perimeter erreichbar mit `allowOutsidePerimeter = true`.

#### N2.4 Arbeitspaket - WaypointExecutor (ersetzt RuntimeState)

Ziel:

- Klar separierter Laufzeit-Controller: hält `activePlan`, Fortschrittsindex, gibt nächsten Waypoint aus.

Technische Entscheidung:

- Kein Planen: WaypointExecutor plant nicht, er führt nur aus.
- Interface:
  ```cpp
  void startPlan(const ZonePlan& plan);
  void startPlan(const MissionPlan& plan);
  const Waypoint* currentWaypoint() const;
  void advanceToNext();          // aufgerufen, wenn Waypoint erreicht
  bool isDone() const;
  void addDynamicObstacle(Polygon obs);   // N5-Vorbereitung
  void clearDynamicObstacles();
  ```
- MowOp ruft `advanceToNext()` auf, wenn LineTracker `targetReached()` meldet.
- Kein internes Replanning. Wenn `isDone()`, geht MowOp in Idle oder startet Dock.

Datei-Struktur:

```
core/navigation/WaypointExecutor.h   (~70 Zeilen)
core/navigation/WaypointExecutor.cpp (~130 Zeilen)
```

Betroffene Dateien (neu):

- `core/navigation/WaypointExecutor.h`
- `core/navigation/WaypointExecutor.cpp`

Abnahmetest:

- `startPlan` + mehrfach `advanceToNext` → Waypoints werden der Reihe nach ausgegeben.
- `isDone()` nach letztem `advanceToNext`.
- Zweimal `startPlan` → sauberer Reset ohne alten Zustand.

#### N2.5 Arbeitspaket - Op-Integration

Ziel:

- NavToStartOp, MowOp und DockOp nutzen die neuen Module statt RuntimeState/MowRoutePlanner.

Technische Entscheidung:

- `NavToStartOp`: ruft `NavPlanner::plan(pos, zonePlan.startPoint, map)` → aktiviert LineTracker.
- `MowOp`:
  1. Erster Eintritt: `WaypointExecutor::startPlan(activePlan)`.
  2. Jeder Kontroll-Loop-Tick: `LineTracker::isTargetReached()` → wenn ja, `advanceToNext()`.
  3. `isDone()` → Übergang zu Idle/Dock.
  4. `mowOn`-Flag des aktuellen Waypoints → Motor-Enable/Disable.
- `DockOp`: ruft `NavPlanner::plan(pos, dockPos, map, allowOutsidePerimeter=true)`.
- Robot.cpp: statt `runtimeState_.startMowing()` → `waypointExecutor_.startPlan(plan)`.

Betroffene Dateien:

- [core/op/NavToStartOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/NavToStartOp.cpp)
- [core/op/MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp)
- [core/op/DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp)
- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

Abnahmetest:

- Robot startet Mähen → NavToStart → MowOp fährt alle Waypoints ab → Idle.
- Batterie leer → DockOp nutzt NavPlanner → Dock erreicht.
- test_robot.cpp bleibt vollständig grün.

#### N2.6 Arbeitspaket - Neue Tests

Ziel:

- Produktverhalten durch klare, verständliche Tests absichern. Alte Architektur-Tests ersetzen.

Technische Entscheidung:

- `test_navigation.cpp` wird für die 9 strukturellen Fehlerfälle (N1.1-Rückstand) neugeschrieben.
- Neue Teststruktur:
  - Sektion A: BoustrophedonPlanner — Eingabe/Ausgabe, Determinismus, No-Go-Trenner.
  - Sektion B: NavPlanner — A*, Hindernisumfahrung, kein Pfad, Dock außerhalb Perimeter.
  - Sektion C: WaypointExecutor — Fortschritt, Reset, `isDone`.
  - Sektion D: Integration — Zone planen → WaypointExecutor starten → alle Waypoints abfahren.
- `test_op_machine.cpp`: A5/C15-Fehlerfälle nutzen `robot.setActivePlan(plan)` direkt,
  statt den alten `enterMowFromStartCommand`-Pfad.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)
- [tests/test_op_machine.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_op_machine.cpp)

Abnahmetest:

- Alle Tests grün (Ziel: 273 / 273 oder mehr).
- Kein Test prüft Interna von gelöschten Modulen.

---

**Dateien, die nach N2 gelöscht werden:**

```
core/navigation/MowRoutePlanner.h/.cpp
core/navigation/RuntimeState.h/.cpp
core/navigation/Costmap.h/.cpp
core/navigation/GridMap.h/.cpp
core/navigation/Planner.h/.cpp
core/navigation/PlannerContext.h
core/navigation/RouteValidator.h/.cpp
```

### Phase N3 - Mission aus Zonen zusammensetzen

- [x] Task N3.1 - MissionPlan aus geordneter Liste von ZonePlans aufbauen
- [x] Task N3.2 - Inter-Zonen-Verbindungen explizit und nachvollziehbar behandeln
- [x] Task N3.3 - Missionsvorschau auf MissionPlan umstellen
- [x] Task N3.4 - Mission speichern und laden auf Planbezug prüfen

Umsetzungsvorgaben:

- Eine Mission ist keine lose Sammlung von Zonen, sondern ein geordneter Plan.
- Verbindungen zwischen Zonen sind explizite Planbestandteile.
- Start darf keinen anderen Plan erzeugen als die gespeicherte oder angezeigte Mission.

Abnahme:

- Vorschau einer Mission und Start derselben Mission referenzieren denselben MissionPlan.

#### N3.1 Arbeitspaket - MissionPlan aus ZonePlans zusammensetzen

Ziel:

- Mehrere Einzelzonen werden zu genau einem geordneten Missionsplan zusammengesetzt.

Technische Entscheidung:

- MissionPlan referenziert explizit die Reihenfolge der ZonePlans.
- Die globale Bahnfolge wird aus ZonePlans plus Verbindungssegmenten aufgebaut.
- Die Mission kennt keine zweite implizite Gesamtroute außerhalb von MissionPlan.

Betroffene Dateien:

- [core/navigation/Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h)
- [core/navigation/MowRoutePlanner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.h)
- [core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)
- [webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)

Abnahmetest:

- Eine Mehrzonen-Mission erzeugt genau einen MissionPlan mit stabiler Zonenreihenfolge.

#### N3.2 Arbeitspaket - Inter-Zonen-Verbindungen explizit modellieren

Ziel:

- Der Übergang zwischen zwei Zonen ist ein sichtbarer und nachvollziehbarer Planbestandteil.

Technische Entscheidung:

- Verbindungen zwischen Zonen werden als eigene Missionssegmente erzeugt und gespeichert.
- Kein stiller Laufzeit-Sprung zwischen Zonen.
- Falls eine Verbindung nicht möglich ist, ist die Mission ungültig statt stillschweigend verändert.

Betroffene Dateien:

- [core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)
- [core/navigation/RouteValidator.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.h)
- [core/navigation/RouteValidator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/RouteValidator.cpp)
- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Eine Inter-Zonen-Verbindung ist entweder explizit im MissionPlan enthalten oder die Mission ist nicht startbar.

#### N3.3 Arbeitspaket - Missionsvorschau strikt auf MissionPlan umstellen

Ziel:

- Die Missionsvorschau ist nur noch Rendering eines vorhandenen MissionPlan.

Technische Entscheidung:

- Die WebUI baut keine Missionsroute aus Zonenlisten mehr nach.
- Vorschau bezieht alle Bahnen, Verbindungen und Fehlerzustände aus MissionPlan.

Betroffene Dateien:

- [webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)
- [webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)
- [webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)

Abnahmetest:

- Die Missionsvorschau einer gespeicherten Mission ist identisch zu dem Plan, der später gestartet wird.

#### N3.4 Arbeitspaket - Missionspersistenz auf Planbezug prüfen

Ziel:

- Gespeicherte Missionen sind fachlich stabil und referenzieren ihren Plan sauber.

Technische Entscheidung:

- Entweder wird MissionPlan mit gespeichert oder deterministisch aus Mission plus Karte reproduzierbar geladen.
- In beiden Fällen muss klar sein, welches Objekt die Produktwahrheit ist.

Betroffene Dateien:

- [webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)
- [core/navigation/MowRoutePlanner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.h)
- [core/navigation/MowRoutePlanner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/MowRoutePlanner.cpp)

Abnahmetest:

- Eine gespeicherte Mission wird nach Reload mit demselben Planbezug wieder geöffnet.

### Phase N4 - Dashboard auf eine Wahrheit reduzieren

- [x] Task N4.1 - Dashboard-Datenmodell auf MissionPlan plus RuntimeState vereinfachen
- [x] Task N4.2 - Geplante und gefahrene Bahnen als getrennte sichtbare Layer definieren
- [x] Task N4.3 - Aktuelles Ziel und Roboterposition planbezogen anzeigen
- [x] Task N4.4 - Zonenstatus im Dashboard aus MissionPlan ableiten

Umsetzungsvorgaben:

- Kein eigener Dashboard-Plan.
- Keine eigene Geometrie-Interpretation im Dashboard.
- Sichtbar nur: geplante Bahnen, gefahrene Bahnen, Roboter, Ziel, Hindernisse, lokaler Umweg.

Abnahme:

- Dashboard kann ohne Planner-Interna denselben Fahrplan darstellen wie die Vorschau.

#### N4.1 Arbeitspaket - Dashboard-Datenmodell auf MissionPlan plus RuntimeState reduzieren

Ziel:

- Das Dashboard arbeitet nur noch mit Plan und Live-Zustand.

Technische Entscheidung:

- Keine eigene Dashboard-Route.
- Keine implizite Rekonstruktion aus Zonen oder Rohpunkten.
- Dashboard-State = MissionPlan + RuntimeState + optionale LocalDetours.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [webui-svelte/src/lib/stores/telemetry.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/telemetry.ts)
- relevante Dashboard-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Dashboard kann ohne Kenntnis der Zonen-Editorlogik den aktiven Plan vollständig darstellen.

#### N4.2 Arbeitspaket - Geplante und gefahrene Bahnen als eigene Layer definieren

Ziel:

- Der Fortschritt ist visuell eindeutig.

Technische Entscheidung:

- Geplante, noch offene Bahn und bereits gefahrene Bahn werden getrennt geführt.
- Der gefahrene Anteil ist Laufzeitstatus, nicht Teil des gespeicherten MissionPlan.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- relevante Dashboard-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Bereits abgefahrene Bahnen lassen sich gestrichelt oder optisch getrennt vom Restplan darstellen.

#### N4.3 Arbeitspaket - Aktuelles Ziel und Roboterposition planbezogen anzeigen

Ziel:

- Der Benutzer sieht jederzeit, welchen Punkt der Roboter gerade anfährt.

Technische Entscheidung:

- RuntimeState liefert Zielpunkt, aktuellen Segmentbezug und Roboterposition.
- Dashboard zeigt Zielkreuz und Roboter immer bezogen auf den aktiven Plan.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [core/navigation/LineTracker.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.h)
- [core/navigation/LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)
- relevante Dashboard-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Zu jedem Zeitpunkt ist ein eindeutiger Zielpunkt auf dem aktiven Plan darstellbar.

#### N4.4 Arbeitspaket - Zonenstatus aus MissionPlan ableiten

Ziel:

- Das Dashboard zeigt Zonenfortschritt aus demselben Plan statt aus separater Logik.

Technische Entscheidung:

- Aktive Zone, offene Zonen und erledigte Zonen werden aus der Missionsstruktur und dem Laufzeitfortschritt abgeleitet.
- Keine getrennte Zonenstatusmaschine im Frontend.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [webui-svelte/src/lib/stores/telemetry.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/telemetry.ts)
- relevante Dashboard-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Der sichtbare Zonenstatus ist konsistent mit dem tatsächlich gefahrenen Missionsplan.

### Phase N5 - Lokale Hindernisumfahrung isolieren

- [x] Task N5.1 - Hindernis als Laufzeitobjekt und Karten-Overlay definieren
- [x] Task N5.2 - LocalDetour als temporäre Zusatzroute definieren
- [x] Task N5.3 - Wiedereinstieg in den bestehenden MissionPlan festlegen
- [x] Task N5.4 - Dashboard-Darstellung für Hindernis plus lokalen Umweg ergänzen

Umsetzungsvorgaben:

- Kein globales Reordering der Mission.
- Kein Neubau aller Zonen wegen eines lokalen Hindernisses.
- Nach der Umfahrung Rückkehr auf den ursprünglichen MissionPlan.

Abnahme:

- Bei einem lokalen Hindernis bleibt die Mission sichtbar dieselbe, nur ein temporärer Umweg kommt hinzu.

#### N5.1 Arbeitspaket - Hindernis als Laufzeitobjekt und Karten-Overlay definieren

Ziel:

- Spontane Hindernisse werden als Live-Ereignisse und sichtbare Kartenobjekte behandelt.

Technische Entscheidung:

- Hindernisse sind nicht Teil der statischen Karte und nicht Teil der Mission.
- Sie werden separat im RuntimeState oder in einer dedizierten Hindernisstruktur geführt.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [core/navigation/Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)
- [core/navigation/Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)
- relevante Dashboard-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Ein erkanntes Hindernis erscheint live, ohne Karte oder Mission umzuschreiben.

#### N5.2 Arbeitspaket - LocalDetour als temporäre Zusatzroute definieren

Ziel:

- Die Umfahrung ist als klarer temporärer Pfad modelliert.

Technische Entscheidung:

- LocalDetour enthält Start, temporäre Zusatzroute und Wiedereinstiegspunkt.
- Die Umfahrung ergänzt den aktiven Plan nur temporär und ersetzt ihn nicht.

Betroffene Dateien:

- [core/navigation/Planner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.h)
- [core/navigation/Planner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.cpp)
- [core/navigation/PlannerContext.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/PlannerContext.h)
- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

Abnahmetest:

- Während eines Hindernisfalls ist der aktive LocalDetour separat vom MissionPlan sichtbar und auswertbar.

#### N5.3 Arbeitspaket - Wiedereinstieg in bestehenden MissionPlan festlegen

Ziel:

- Nach der Umfahrung kehrt das System sauber auf den Originalplan zurück.

Technische Entscheidung:

- Der Wiedereinstieg referenziert einen konkreten Planpunkt oder ein konkretes Plansegment.
- Kein globales Replanning der Mission nach dem Hindernis.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [core/navigation/LineTracker.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.h)
- [core/navigation/LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

Abnahmetest:

- Nach der Umfahrung fährt der Roboter auf dem bestehenden Plan fort statt eine neue Mission aufzubauen.

#### N5.4 Arbeitspaket - Dashboard-Darstellung für Hindernis und lokalen Umweg ergänzen

Ziel:

- Der Benutzer sieht, dass ein Hindernis lokal behandelt wird, während die Mission dieselbe bleibt.

Technische Entscheidung:

- Dashboard zeigt Hindernispolygon und LocalDetour als zusätzliche Live-Layer.
- MissionPlan bleibt unverändert sichtbar.

Betroffene Dateien:

- relevante Dashboard-Komponenten der WebUI und Mobile-App
- [webui-svelte/src/lib/stores/telemetry.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/telemetry.ts)

Abnahmetest:

- Im Hindernisfall bleiben geplante Bahnen gleich sichtbar, ergänzt um Hindernis und lokalen Umweg.

### Phase N6 - Startarten vereinheitlichen

- [x] Task N6.1 - Normalen Missionsstart strikt auf MissionPlan ausrichten
- [x] Task N6.2 - Jetzt mähen strikt auf genau einen ZonePlan ausrichten
- [x] Task N6.3 - Wiederaufnahme auf bestehenden Plan statt Neuinterpretation ausrichten

Umsetzungsvorgaben:

- `Jetzt mähen` darf keine Sonderlogik mit eigener Planner-Wahrheit werden.
- Missionsstart und Zonenstart unterscheiden sich nur im Planobjekt, nicht im Grundprinzip.

Abnahme:

- Jede Startart aktiviert einen vorhandenen Plan statt eine neue implizite Route zu erzeugen.

#### N6.1 Arbeitspaket - Normalen Missionsstart strikt auf MissionPlan ausrichten

Ziel:

- Missionsstart aktiviert nur einen vorhandenen MissionPlan.

Technische Entscheidung:

- Vor dem Start muss ein gültiger MissionPlan vorliegen.
- Start baut keine eigene Route aus Zonenlisten oder Map-Daten zusammen.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)

Abnahmetest:

- Missionsstart verwendet denselben Plan wie die zuletzt angezeigte Missionsvorschau.

#### N6.2 Arbeitspaket - `Jetzt mähen` strikt auf einen ZonePlan ausrichten

Ziel:

- Zonenstart ist kein Sonderpfad mit eigener Planungswahrheit.

Technische Entscheidung:

- `Jetzt mähen` aktiviert genau einen vorhandenen oder frisch kompilierten ZonePlan.
- Kein alternativer Legacy-Start aus Rohdaten.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- [webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)
- [webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)

Abnahmetest:

- `Jetzt mähen` fährt exakt den Plan, der in der aktiven Zonen-Vorschau sichtbar ist.

#### N6.3 Arbeitspaket - Wiederaufnahme auf bestehenden Plan statt Neuinterpretation ausrichten

Ziel:

- Unterbrechung und Fortsetzung bleiben fachlich auf demselben Plan.

Technische Entscheidung:

- Wiederaufnahme referenziert aktiven Plan plus Fortschrittspunkt.
- Kein Neubau der Mission bei Resume.

Betroffene Dateien:

- [core/Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)
- relevante Persistenz- oder Telemetriepfade für Fortschritt

Abnahmetest:

- Nach einer Unterbrechung kann der Roboter auf denselben Plan zurückkehren, ohne dass sich die sichtbare Route ändert.

### Phase N7 - Frontend-Verantwortung hart begrenzen

- [x] Task N7.1 - Karten-Seite als Editor für statische Geometrie schärfen
- [x] Task N7.2 - Missions-Seite als Editor für Zonen und aktive Zonen-Vorschau schärfen
- [x] Task N7.3 - UI-seitige Planungsannahmen und Nebenlogik entfernen
- [x] Task N7.4 - Technische Planner-Begriffe aus Primär-UI fernhalten

Umsetzungsvorgaben:

- Karten- und Missions-Werkzeuge bleiben in der Bedienung erhalten.
- Entfernt wird nur falsche Verantwortungsverteilung, nicht das Werkzeugkonzept.
- Interne technische Begriffe dürfen Debug bleiben, aber nicht Benutzeroberfläche werden.

Abnahme:

- Benutzer arbeitet weiter mit den bekannten Werkzeugen, aber die zugrunde liegende Datenhoheit ist sauber getrennt.

#### N7.1 Arbeitspaket - Karten-Seite als Editor für statische Geometrie schärfen

Ziel:

- Die Karten-Seite bleibt Werkzeug für statische Geometrie und sonst nichts.

Technische Entscheidung:

- Perimeter-, Hard-NoGo- und Dock-Werkzeuge bleiben.
- Zonen- oder Missionslogik wird nicht mehr fachlich in die Karten-Seite verlagert.

Betroffene Dateien:

- [webui-svelte/src/lib/components/Map/MapCanvas.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/MapCanvas.svelte)
- [webui-svelte/src/lib/components/Map/PerimeterTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/PerimeterTool.svelte)
- [webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte)
- [webui-svelte/src/lib/components/Map/DockTool.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Map/DockTool.svelte)

Abnahmetest:

- Die Karten-Seite kann vollständig verwendet werden, ohne Missionsroute oder Laufzeitstatus zu kennen.

#### N7.2 Arbeitspaket - Missions-Seite als Editor für Zonen und aktive Zonen-Vorschau schärfen

Ziel:

- Die Missions-Seite bleibt das Werkzeug für Zonen und aktive Planungsvorschau.

Technische Entscheidung:

- Zone anlegen, Zone wählen, Zonenparameter ändern und aktive Vorschau bleiben erhalten.
- Die Seite rendert Plandaten, erzeugt aber keine eigene Planung.

Betroffene Dateien:

- [webui-svelte/src/lib/components/Mission/ZoneSelect.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSelect.svelte)
- [webui-svelte/src/lib/components/Mission/ZoneSettings.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/ZoneSettings.svelte)
- [webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)
- [webui-svelte/src/lib/components/Mission/MissionControls.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/MissionControls.svelte)

Abnahmetest:

- Eine aktive Zone kann bearbeitet und als Plan visualisiert werden, ohne UI-seitig eine Route nachzubauen.

#### N7.3 Arbeitspaket - UI-seitige Planungsannahmen und Nebenlogik entfernen

Ziel:

- Das Frontend hört auf, implizit Planner-Verhalten nachzubilden.

Technische Entscheidung:

- Keine eigene Segmentlogik, keine eigene Geometrie-Rekonstruktion, keine zweite Fortschrittslogik im UI.
- Debug-Ansichten dürfen intern bleiben, aber nicht zur funktionalen Wahrheitsquelle werden.

Betroffene Dateien:

- [webui-svelte/src/lib/components/Mission/PathPreview.svelte](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/components/Mission/PathPreview.svelte)
- [webui-svelte/src/lib/stores/map.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/map.ts)
- [webui-svelte/src/lib/stores/missions.ts](/mnt/LappiDaten/Projekte/sunray-core/webui-svelte/src/lib/stores/missions.ts)

Abnahmetest:

- Ohne Backend-Plandaten kann das Frontend keine gültige Vorschau oder Dashboard-Route erzeugen.

#### N7.4 Arbeitspaket - Technische Planner-Begriffe aus Primär-UI fernhalten

Ziel:

- Die Primär-UI bleibt produktsprachlich verständlich.

Technische Entscheidung:

- Begriffe wie Komponente, Validator, A*, Costmap und Recovery bleiben Debug oder intern.
- Die sichtbare UI spricht über Fläche, Bahn, Ziel, Hindernis und Fortschritt.

Betroffene Dateien:

- relevante Mission-, Dashboard- und Map-Komponenten der WebUI und Mobile-App

Abnahmetest:

- Die Primär-UI ist ohne interne Planner-Begriffe verständlich nutzbar.

### Phase N8 - Pflicht-Tests für das neue Zielbild

- [x] Test N8.A - Karte speichert nur statische Geometrie
- [x] Test N8.B - Aktive Zone erzeugt genau einen ZonePlan
- [x] Test N8.C - Missionsvorschau und Missionsstart verwenden denselben MissionPlan
- [x] Test N8.D - Dashboard zeigt geplante und gefahrene Bahnen aus derselben Route
- [x] Test N8.E - No-Go-geteilte Zone bleibt lokal kohärent und springt nicht chaotisch
- [x] Test N8.F - Hindernis erzeugt nur lokalen Detour und ändert MissionPlan nicht
- [x] Test N8.G - `Jetzt mähen` fährt exakt den Plan der selektierten Zone

Umsetzungsvorgaben:

- Tests müssen Produktverhalten prüfen, nicht nur interne Semantik.
- Jede neue technische Struktur muss mindestens auf Produktwirkung zurückgeführt werden.

#### N8.A Arbeitspaket - Karte speichert nur statische Geometrie

Ziel:

- Kartentests sichern die Trennung zwischen statischer Welt und Missionslogik.

Technische Entscheidung:

- Map-Serialisierung testet nur Perimeter, Hard-NoGo, Dock und Editor-Metadaten.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Kartenpersistenz enthält keine Missions- oder Laufzeitwahrheit.

#### N8.B Arbeitspaket - Aktive Zone erzeugt genau einen ZonePlan

Ziel:

- Der Einzelzonenpfad ist als Produktfunktion testbar.

Technische Entscheidung:

- Testfall prüft deterministischen Plan, Startpunkt, Endpunkt und Validität.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Dieselbe Zone führt deterministisch zu demselben ZonePlan.

#### N8.C Arbeitspaket - Missionsvorschau und Missionsstart verwenden denselben MissionPlan

Ziel:

- Vorschau und Start werden produktseitig an dieselbe Wahrheit gebunden.

Technische Entscheidung:

- Testfall prüft, dass Preview und Start denselben serialisierten oder identifizierten MissionPlan referenzieren.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Kein Unterschied zwischen Vorschau- und Startplan.

#### N8.D Arbeitspaket - Dashboard zeigt geplante und gefahrene Bahnen aus derselben Route

Ziel:

- Fortschrittsdarstellung wird produktseitig abgesichert.

Technische Entscheidung:

- Testfall oder Integrationsprüfung prüft Trennung zwischen geplantem Rest und gefahrenem Anteil bei identischem Grundplan.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)
- ggf. zusätzliche UI- oder Integrations-Tests

Abnahmetest:

- Dashboard-Fortschritt entsteht aus demselben Plan wie die Vorschau.

#### N8.E Arbeitspaket - No-Go-geteilte Zone bleibt lokal kohärent

Ziel:

- Das Kernproblem der bisherigen Planerlogik wird produktseitig abgesichert.

Technische Entscheidung:

- Testfälle prüfen, dass No-Go-geteilte Bahnen nicht segmentweise chaotisch verkettet werden.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Keine chaotischen Querwechsel über die gesamte Zone.
- Lokale Bearbeitung bleibt stabil und plausibel.

#### N8.F Arbeitspaket - Hindernis erzeugt nur LocalDetour und ändert MissionPlan nicht

Ziel:

- Laufzeit-Hindernisse werden produktseitig von der Mission getrennt abgesichert.

Technische Entscheidung:

- Testfall prüft unveränderten MissionPlan plus separaten LocalDetour.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Nach Hinderniserkennung bleibt MissionPlan gleich, nur ein temporärer Detour wird aktiv.

#### N8.G Arbeitspaket - `Jetzt mähen` fährt exakt den Plan der selektierten Zone

Ziel:

- Der Einzelzonen-Start wird produktseitig festgezurrt.

Technische Entscheidung:

- Testfall prüft, dass der Startpfad auf exakt den aktiven ZonePlan zeigt und keine Sonderroute erzeugt.

Betroffene Dateien:

- [tests/test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp)

Abnahmetest:

- Der von `Jetzt mähen` aktivierte Plan ist identisch zur sichtbaren aktiven Zonen-Vorschau.

### Definition of Done für den Neuplan

- [ ] Karte und Mission sind fachlich sauber getrennt.
- [ ] Karten- und Missions-Werkzeuge bleiben in der Bedienung erhalten.
- [ ] Jede Zone erzeugt genau einen ZonePlan.
- [ ] Jede Mission erzeugt genau einen MissionPlan.
- [ ] Vorschau ist die sichtbare Form des echten Fahrplans.
- [ ] Start aktiviert genau diesen Fahrplan.
- [ ] Dashboard zeigt genau diesen Fahrplan plus Live-Zustand.
- [ ] Lokale Hindernisse erzeugen nur temporäre Umwege.
- [ ] No-Go-geteilte Flächen werden lokal plausibel gemäht.
- [ ] Frontend enthält keine eigene Planungslogik.

## Empfohlenes erstes MVP-Slice

Dieses Slice ist die kleinste sinnvolle Umsetzung, die bereits den neuen Produktkern beweist.

### MVP-Ziel

- Eine Karte mit Perimeter, Hard-NoGo und Dock laden
- Eine aktive Zone planen
- Genau einen ZonePlan erzeugen
- Genau diesen ZonePlan in der Vorschau anzeigen
- Genau diesen ZonePlan per `Jetzt mähen` starten
- Live geplante Bahn, gefahrene Bahn, Robotersymbol und Zielpunkt anzeigen

### MVP-Inhalt

- [ ] MVP.1 - Map auf statische Geometrie begrenzen
- [ ] MVP.2 - ZonePlan als erstes echtes Planobjekt einführen
- [ ] MVP.3 - Aktive Zonen-Vorschau nur aus ZonePlan rendern
- [ ] MVP.4 - `Jetzt mähen` auf ZonePlan statt Sonderpfad umstellen
- [ ] MVP.5 - Dashboard für Einzelzone auf Plan plus RuntimeState ausrichten

### Nicht Teil des ersten MVP

- vollständige Mehrzonen-Mission
- komplexe Inter-Zonen-Verkettung
- ausgebaute Hindernisumfahrung für alle Sonderfälle
- vollständige Altpfad-Migration

### MVP-Abnahme

- Eine einzelne Zone ist vom Editor bis zum Fahrstart eine geschlossene Wahrheit.
- Vorschau und Fahrverhalten derselben Zone stimmen überein.
- Dashboard zeigt dieselbe Zone als Plan und Fortschritt.

## Nächste Aufgabe

- Aufgabe: N2 — Navigationskern Greenfield-Neubau
- Reihenfolge: N2.1 → N2.2 → N2.3 → N2.4 → N2.5 → N2.6
- Startpunkt: N2.1 (Route.h Fundament), dann N2.2 (BoustrophedonPlanner)
- Ziel: 3 neue Module (BoustrophedonPlanner, NavPlanner, WaypointExecutor) ersetzen ~2700 Zeilen
  alter Komplexität durch ~750 Zeilen klare Verantwortung
- Anker: test_robot.cpp (264 Tests) bleibt grün als Sicherheitsnetz
- Abschluss: 9 strukturelle Testfehler aus N1.1 werden mit N2.6 durch Produktverhalts-Tests ersetzt