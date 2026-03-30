# WEBUI_TASK.md

Stand: 2026-03-30

Diese Datei ist die laufende Task-Liste fuer den Neustart der WebUI auf Basis
von `Svelte + Vite`.

Regel:

- kurz halten
- beim Abarbeiten aktualisieren
- pro Task eine knappe Beschreibung
- pro Task den wichtigsten Datei- oder Pfadbezug angeben

Design-Referenzen:

- `webui/design/dashboard_reference.html`
- `webui/design/sunray_dashboard_v5.html`

## Sinnvolle Reihenfolge

Bereits erledigt:

1. Projektgeruest
2. Verbindung und Telemetrie
3. Dashboard
4. Diagnose
5. Motor-Kalibrierung

Naechste sinnvolle Reihenfolge:

1. Karten-MVP fertigstellen
2. Schnittstellen fuer V1 sauber festziehen
3. NoGo-Zonen auf Basis der Karte ergaenzen
4. Missions-Seite bauen
5. Abschlusskriterien gegen die echte Hardware pruefen

Realer Hinweis aus Gartenscreenshots:

- Pan/Zoom im Karten-Canvas ist kein Nice-to-have, sondern Blocker fuer echte
  Tests bei grossen Karten.
- NoGo-Zonen muessen auch bei 10+ Hindernissen noch brauchbar bearbeitbar sein.
- Eine einfache lokale Gitterkarte reicht weiter aus; keine Hintergrundkarte.
- Dashboard und Mapping sollten funktional getrennte Modi bleiben.
- Die Karte ist ein metrischer Arbeitsraum fuer Robotikdaten, kein GIS.
- Aktives Werkzeug muss im Editor immer klar sichtbar sein.
- Status, Modus und Akku sollten auf allen Seiten sichtbar sein.
- NOTAUS sollte global erreichbar sein.
- Die Karte sollte im Mapping-Modus die primaere Flaeche bleiben; eine rechte
  Sidebar ist sinnvoller als eine breite linke Hauptsidebar.
- Hohe Informationsdichte ist fuer diese Art UI sinnvoller als grosse Hero-
  Sektionen.
- Dunkles Navy-Layout, semantische Kartenfarben und kompakte Status-Pills aus
  den alten HTML-Referenzen beibehalten.

## Status

- [ ] offen
- [~] in Arbeit
- [x] erledigt

## 1. Grundgerüst

- [x] Neues Frontend-Projekt anlegen
  Kurz: separates `Svelte + Vite + TypeScript` Projekt erstellen.
  Datei/Pfad: `webui-svelte/`

- [x] Build-Output festlegen
  Kurz: klaeren, wohin `dist/` gebaut und wie es spaeter ausgeliefert wird.
  Datei/Pfad: `webui-svelte/vite.config.ts`

- [x] Frontend-README anlegen
  Kurz: Start, Build, Deploy und Architektur kurz dokumentieren.
  Datei/Pfad: `webui-svelte/README.md`

## 2. Verbindung

- [x] WebSocket-Client bauen
  Kurz: Verbindung zu `/ws/telemetry` mit sauberem Reconnect.
  Datei/Pfad: `webui-svelte/src/lib/api/websocket.ts`

- [x] Telemetrie-Store bauen
  Kurz: letzten Telemetrie-Snapshot zentral halten.
  Datei/Pfad: `webui-svelte/src/lib/stores/telemetry.ts`

- [x] Verbindungsstatus-Store bauen
  Kurz: verbunden/getrennt, letzter Kontakt, Reconnect-Status.
  Datei/Pfad: `webui-svelte/src/lib/stores/connection.ts`

## 3. Dashboard

- [x] Dashboard-Seite bauen
  Kurz: kleinster Live-Ueberblick ueber den Roboter.
  Datei/Pfad: `webui-svelte/src/lib/pages/Dashboard.svelte`

- [x] Statusleiste bauen
  Kurz: Verbindung, Zustand, Batterie kompakt anzeigen.
  Datei/Pfad: `webui-svelte/src/lib/components/StatusBar.svelte`

- [~] Globalen Topbar-Layout-Umbau machen
  Kurz: feste Topbar mit Status-Pills und horizontalen Tabs statt linker Hauptsidebar.
  Datei/Pfad: `webui-svelte/src/App.svelte`

- [x] Globalen NOTAUS-Platzhalter einbauen
  Kurz: roter globaler Button im Topbar, spaeter mit echter Backend-Aktion verdrahten.
  Datei/Pfad: `webui-svelte/src/App.svelte`

- [x] Status-Pills seitenuebergreifend machen
  Kurz: Verbindung, Modus, Akku und Laden auf allen Seiten sichtbar halten.
  Datei/Pfad: `webui-svelte/src/lib/components/StatusBar.svelte`

- [x] Globalen Fehlerbanner einbauen
  Kurz: Fehlerzustand und Fehlergrund bei OP_ERROR oben immer sichtbar machen.
  Datei/Pfad: `webui-svelte/src/App.svelte`

- [x] Ladeanzeige fachlich korrigieren
  Kurz: Dockkontakt und Ladestrom sauber unterscheiden statt nur `charge_v` als Laden anzuzeigen.
  Datei/Pfad: `webui-svelte/src/lib/components/StatusBar.svelte`

- [~] Altes Dashboard-Layout als Svelte-Referenz uebernehmen
  Kurz: Topbar, Tab-Navigation, dominante Karte und rechte Sidebar an den HTML-Referenzen ausrichten.
  Datei/Pfad: `webui/design/sunray_dashboard_v5.html`

- [ ] Farbsprache aus Referenz-HTML uebernehmen
  Kurz: Navy-Basis, Blau/Cyan/Rot/Amber semantisch konsistent in die neue UI uebertragen.
  Datei/Pfad: `webui/design/dashboard_reference.html`

- [x] Basis-Steuerung einbauen
  Kurz: Start, Stop und Dock als einfache Buttons.
  Datei/Pfad: `webui-svelte/src/lib/components/RobotControls.svelte`

- [x] Sensor-Kacheln bauen
  Kurz: GPS, IMU, Bumper, Lift, Motorfehler anzeigen.
  Datei/Pfad: `webui-svelte/src/lib/components/TelemetryPanel.svelte`

## 4. Diagnose

- [x] Diagnose-Seite bauen
  Kurz: eigener Bereich fuer Hardwaretests und Kalibrierung.
  Datei/Pfad: `webui-svelte/src/lib/pages/Diagnostics.svelte`

- [x] Sensor-Liveansicht bauen
  Kurz: Bumper, Lift, Motorfehler, GPS, IMU live darstellen.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/SensorPanel.svelte`

- [x] IMU-Panel bauen
  Kurz: Heading, Roll, Pitch sichtbar machen.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/ImuPanel.svelte`

- [x] Motor-Test-Panel bauen
  Kurz: linkes/rechtes Rad und Mähmotor getrennt testbar machen.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/MotorTestPanel.svelte`

## 5. Motor-Kalibrierung

- [x] Tick-Kalibrierung links/rechts
  Kurz: 1-Umdrehung-Test, Ticks messen, Soll/Ist/Abweichung zeigen.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/TickCalibration.svelte`

- [x] Ticks speichern
  Kurz: `ticks_per_revolution` aus der UI persistieren.
  Datei/Pfad: `webui-svelte/src/lib/api/rest.ts`

- [x] Richtungsprüfung links/rechts
  Kurz: Vorwaertsimpuls geben und Richtung manuell bestaetigen.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/DirectionValidation.svelte`

- [x] Richtungsinversion speichern
  Kurz: `invert_left_motor` und `invert_right_motor` setzen.
  Datei/Pfad: `webui-svelte/src/lib/api/rest.ts`

## 6. Karte

- [x] Karten-Seite bauen
  Kurz: einfacher Arbeitsraum fuer Perimeter, Dock und Zonen.
  Datei/Pfad: `webui-svelte/src/lib/pages/Map.svelte`

- [~] Kartenlayout auf dominante Arbeitsflaeche umbauen
  Kurz: Karte gross, Werkzeuge und Details in rechte Sidebar verschieben.
  Datei/Pfad: `webui-svelte/src/lib/pages/Map.svelte`

- [x] Karten-REST anbinden
  Kurz: Karte laden und speichern.
  Datei/Pfad: `webui-svelte/src/lib/api/rest.ts`

- [x] Canvas mit Gitter bauen
  Kurz: kein OSM, nur lokales Gitter, Pan und Zoom.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

- [x] Karten-Canvas gegen reale Gartengroesse pruefen
  Kurz: Pan/Zoom und Bedienbarkeit mit grossem Perimeter und vielen NoGo-Zonen verifizieren.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

- [x] Dock-Werkzeug bauen
  Kurz: Dockpunkt setzen und anzeigen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/DockTool.svelte`

- [x] Perimeter-Werkzeug bauen
  Kurz: Perimeterpunkte setzen, verschieben, loeschen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/PerimeterTool.svelte`

- [x] Zonen-Werkzeug bauen
  Kurz: Zonen anlegen und bearbeiten.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/ZoneTool.svelte`

- [x] NoGo-Zonen-Werkzeug bauen
  Kurz: NoGo-Zonen erst auf Basis einer vorhandenen Karte anlegen und bearbeiten.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte`

- [x] NoGo-Zonen im Canvas anzeigen
  Kurz: NoGo-Zonen im Kartenarbeitsraum sichtbar rendern.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

- [x] NoGo-Zonen in den Map-Store aufnehmen
  Kurz: NoGo-Zonen als eigener Kartenbestandteil im State fuehren.
  Datei/Pfad: `webui-svelte/src/lib/stores/map.ts`

- [x] NoGo-Zonen-UX fuer viele Hindernisse pruefen
  Kurz: Auswahl und Bearbeitung mit 10+ NoGo-Zonen praktikabel machen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/NoGoZoneTool.svelte`

- [ ] Dock-Pfad-Werkzeug bauen
  Kurz: zusaetzlich zum Dockpunkt einen echten Dock-Pfad anlegen und bearbeiten.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/DockPathTool.svelte`

- [ ] Zonen-Eigenschaften erweitern
  Kurz: Name, Streifenbreite, Geschwindigkeit und Muster pro Zone bearbeiten.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/ZoneTool.svelte`

- [ ] Zonen-Reihenfolge bearbeiten
  Kurz: Zonen im UI hoch und runter verschieben koennen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/ZoneTool.svelte`

- [ ] Punkt-auf-Kante-einfuegen bauen
  Kurz: neuen Punkt gezielt in eine bestehende Perimeter-, Zonen- oder NoGo-Kante einfuegen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

- [ ] Gezieltes Punkt-Loeschen bauen
  Kurz: nicht nur den letzten Punkt, sondern beliebige nahe Punkte loeschen koennen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

- [ ] Mähpfad-Vorschau vorbereiten
  Kurz: Mow-Path berechnen, anzeigen, uebernehmen oder verwerfen koennen.
  Datei/Pfad: `webui-svelte/src/lib/pages/Map.svelte`

- [ ] GeoJSON Import/Export ergaenzen
  Kurz: Karten als GeoJSON exportieren und wieder importieren koennen.
  Datei/Pfad: `webui-svelte/src/lib/api/rest.ts`

- [ ] Positionsmodus absolut/relativ abbilden
  Kurz: Cassandra-Bezug aufnehmen; Ursprung und Kartenausrichtung passend zu RTK-Base/relativem Mapping fuehren.
  Datei/Pfad: `webui-svelte/src/lib/pages/Map.svelte`

- [ ] Obstacle-Punkte / Auto-Obstacles fachlich klaeren
  Kurz: klaeren, ob manuelle/persistente Obstacle-Punkte und auto-detected Hindernisse als eigenes Konzept neben NoGo-Zonen in V1 oder spaeter sichtbar sein muessen.
  Datei/Pfad: `docs/SVELTE_WEBUI_CONCEPT.md`

- [ ] Karten-Editor-Komfort nachziehen
  Kurz: Undo/Abbrechen, aktives Werkzeug klar halten und spaeter Snapping ergaenzen.
  Datei/Pfad: `webui-svelte/src/lib/components/Map/MapCanvas.svelte`

## 7. Schnittstellen

- [x] REST-Schnittstellen auflisten
  Kurz: nur die wirklich benoetigten Endpunkte fuer V1 festhalten.
  Datei/Pfad: `docs/SVELTE_WEBUI_CONCEPT.md`

- [x] WS-Nachrichtenformat festhalten
  Kurz: `state`, `event`, `diag_progress`, `diag_result`, `ping`.
  Datei/Pfad: `docs/SVELTE_WEBUI_CONCEPT.md`

- [x] Diagnose async definieren
  Kurz: kein blockierender Handler, Status und Ergebnis ueber WS.
  Datei/Pfad: `core/WebSocketServer.cpp`

## 8. Mission

- [x] Missions-Seite bauen
  Kurz: Zonen auswaehlen und Maehen starten.
  Datei/Pfad: `webui-svelte/src/lib/pages/Mission.svelte`

- [x] Zonenwahl einbauen
  Kurz: welche Zonen gemaeht werden sollen.
  Datei/Pfad: `webui-svelte/src/lib/components/Mission/ZoneSelect.svelte`

- [x] Mission-Buttons einbauen
  Kurz: Start, Dock, Abbrechen.
  Datei/Pfad: `webui-svelte/src/lib/components/Mission/MissionControls.svelte`

## 9. Nicht in V1

- [x] Keine Hintergrundkarte
  Kurz: kein OpenStreetMap, kein Satellitenlayer.
  Datei/Pfad: `docs/SVELTE_WEBUI_CONCEPT.md`

- [ ] Keine Historie
  Kurz: nicht fuer den ersten Wurf.
  Datei/Pfad: `webui-svelte/src/lib/pages/History.svelte`

- [ ] Keine Statistik
  Kurz: nicht fuer den ersten Wurf.
  Datei/Pfad: `webui-svelte/src/lib/pages/Statistics.svelte`

- [ ] Keine grosse Einstellungsseite
  Kurz: erst spaeter, wenn Basis stabil ist.
  Datei/Pfad: `webui-svelte/src/lib/pages/Settings.svelte`

## 10. Abschlusskriterien V1

- [ ] Dashboard stabil
  Kurz: Telemetrie und Basis-Steuerung laufen stabil.
  Datei/Pfad: `webui-svelte/src/lib/pages/Dashboard.svelte`

- [ ] Diagnose stabil
  Kurz: Hardwaretests und Kalibrierung sind benutzbar.
  Datei/Pfad: `webui-svelte/src/lib/pages/Diagnostics.svelte`

- [ ] Karte stabil
  Kurz: Perimeter, Dock und Zonen lassen sich bearbeiten und speichern.
  Datei/Pfad: `webui-svelte/src/lib/pages/Map.svelte`

- [ ] Motor-Kalibrierung benutzbar
  Kurz: Tick-Messung und Richtungsinversion funktionieren.
  Datei/Pfad: `webui-svelte/src/lib/components/Diagnostics/`

- [ ] Robotersteuerung benutzbar
  Kurz: Start, Stop und Dock funktionieren aus der WebUI.
  Datei/Pfad: `webui-svelte/src/lib/components/RobotControls.svelte`
