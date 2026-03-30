# Svelte + Vite WebUI Concept

Stand: 2026-03-30

Design-Referenzen:

- `webui/design/dashboard_reference.html`
- `webui/design/sunray_dashboard_v5.html`

## Ziel

Eine neue WebUI auf Basis von `Svelte + Vite`, die auf dem Pi robust läuft,
WebSocket-faehig ist und die fuer Alfred wirklich noetigen Funktionen abdeckt:

- Live-Telemetrie
- Diagnose
- Motor-Kalibrierung
- Karte und Zonen bearbeiten
- Start, Stop, Dock, Maehen

Die neue WebUI soll die aktuelle Vue-WebUI nicht weiter flicken, sondern einen
kontrollierten Neustart mit kleinerem, robusterem Scope ermoeglichen.

## Leitprinzipien

- so wenig Komplexitaet wie moeglich
- keine blockierenden Backend-Handler
- Frontend nur als statische `dist/`-App deployen
- WebSocket-Protokoll simpel halten
- REST nur fuer klar getrennte Lese- und Schreibvorgaenge
- Canvas oder SVG statt schwerer Kartenframeworks
- keine Hintergrundkarte wie OpenStreetMap; ein einfaches lokales Gitter reicht
- Diagnose und Karteneditor getrennt denken
- Status, Modus und Akku sollen seitenuebergreifend sichtbar bleiben
- NOTAUS soll global erreichbar sein
- keine breite linke Hauptsidebar; der Arbeitsraum hat Vorrang

## Ableitungen aus echten Screens

Aus vorhandenen Betriebs- und Mapping-Screens ergeben sich ein paar klare
fachliche Leitplanken:

- Die WebUI braucht zwei klar getrennte Modi:
  - Betriebsansicht fuer Status, Telemetrie und Steuerung
  - Mapping-/Editoransicht fuer Perimeter, Dock, Zonen und NoGo-Zonen
- Die Navigation sollte eher ueber eine feste Topbar mit Tabs laufen als ueber
  eine breite linke Sidebar.
- Der Karteneditor arbeitet sinnvoll in lokalen Meterkoordinaten, nicht in
  Lat/Lon. Sichtbare Skala und Gitter sind wichtiger als eine Hintergrundkarte.
- Der Roboter sollte auf der Karte nicht nur als Punkt, sondern spaeter auch
  mit Orientierung sichtbar sein. Fuer V1 reicht die Telemetrie-Position.
- Werkzeugmodus ist zentral:
  - aktives Werkzeug muss immer sichtbar sein
  - Undo/Loeschen muessen an das aktive Werkzeug gebunden sein
  - Cross-Tool-Aktionen sollen vermieden werden
- Viele kleine Hindernisse sind ein realer Use Case. NoGo-Zonen muessen daher
  nicht nur existieren, sondern auch bei 10+ Polygonen noch brauchbar waehlen
  und bearbeiten lassen.
- Pan und Zoom sind Pflicht. Bei realen Gartengroessen ist der Editor sonst
  nicht testbar.
- Die Karte ist kein GIS-System, sondern ein Arbeitsraum fuer Robotikdaten:
  Perimeter, Dock, Zonen, NoGo-Zonen, Roboterposition und spaeter Fahrpfad.
- Im Mapping-Modus muss die Karte die dominante Flaeche sein; Werkzeuge und
  Details gehoeren in eine kompakte rechte Sidebar.
- Die Informationsdichte darf hoch sein. Grosse Hero-Sektionen oder dekorative
  Header sind fuer diese Art Robotik-UI nicht hilfreich.

## Layout-Richtung

Funktional sollte die neue Svelte-WebUI die Staerken der alten Oberflaeche
uebernehmen:

- feste Topbar mit:
  - Roboternamen
  - Verbindungsstatus
  - Betriebsmodus
  - Akku-/Ladestatus
  - NOTAUS
- horizontale Tabs in der Topbar
- keine linke Hauptsidebar
- beim Karten-Tab:
  - Karte als primaere Arbeitsflaeche
  - Werkzeuge, Zonen und Details rechts
- beim Dashboard-Tab:
  - kompakte Status- und Telemetrie-Bloecke
  - kleine Karte optional als Nebenansicht

Wichtige konkrete Design-Signale aus den Referenzen:

- dunkles Navy-Layout statt hellem Marketing-Look
- kompakte Topbar mit Status-Pills
- NOTAUS visuell klar und immer rechts erreichbar
- horizontale Tabs statt linker Hauptnavigation
- Karte als primaere Flaeche
- rechte Sidebar fuer kontextabhaengige Inhalte
- semantische Farben:
  - Blau fuer Perimeter
  - Cyan fuer Mow-/Fahrpfad
  - Rot gestrichelt fuer Exclusions
  - Amber fuer Dock
- GPS mit Rover- und Base-Satelliten getrennt
- Roboter-Marker mit Richtungspfeil statt nur Punkt
- hohe Informationsdichte, wenig Leerraum

Skizze:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ Alfred   verbunden   [Charge/Mow/etc.]   Akku   [NOTAUS]                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ Dashboard | Karte | Mission | Diagnose                                     │
├───────────────────────────────────────┬─────────────────────────────────────┤
│                                       │                                     │
│ Karte / Canvas / Arbeitsflaeche       │ Seitenspezifische Sidebar           │
│                                       │ Werkzeuge, Zonen, Details           │
│                                       │                                     │
└───────────────────────────────────────┴─────────────────────────────────────┘
```

## Backend-Rolle

`sunray-core` bleibt das Backend fuer:

- Hardwarezugriff
- Robotik-Logik
- Telemetrie
- Diagnose-Operationen
- Karten- und Zonenpersistenz

Die neue WebUI spricht nur mit:

- `GET /api/...`
- `POST /api/...`
- `PUT /api/...`
- `ws://.../ws/telemetry`

## Frontend-Rolle

Die neue WebUI wird als neues, separates Projekt angelegt, zum Beispiel unter:

`webui-svelte/`

Die bestehende Vue-WebUI bleibt zunaechst unangetastet. Migration erfolgt
parallel statt als Big Bang.

## Empfohlener Stack

- `Svelte`
- `Vite`
- `TypeScript`
- `svelte/store` fuer globalen Zustand

Bewusst zunaechst vermeiden:

- grosse UI-Frameworks
- schwere Kartenbibliotheken
- komplexe State-Management-Layer

Optional spaeter:

- `zod` fuer API-Validierung
- leichter Router
- `Konva` nur falls der Karteneditor mit eigenem Canvas zu teuer wird

## Projektstruktur

Vorschlag:

```text
webui-svelte/
├── src/
│   ├── app.html
│   ├── main.ts
│   ├── App.svelte
│   ├── lib/
│   │   ├── api/
│   │   │   ├── rest.ts
│   │   │   ├── websocket.ts
│   │   │   └── types.ts
│   │   ├── stores/
│   │   │   ├── connection.ts
│   │   │   ├── telemetry.ts
│   │   │   ├── config.ts
│   │   │   ├── diagnostics.ts
│   │   │   └── map.ts
│   │   ├── components/
│   │   │   ├── StatusBar.svelte
│   │   │   ├── RobotCard.svelte
│   │   │   ├── TelemetryPanel.svelte
│   │   │   ├── MotorTestPanel.svelte
│   │   │   ├── ImuPanel.svelte
│   │   │   ├── GpsPanel.svelte
│   │   │   ├── BumperPanel.svelte
│   │   │   ├── MapCanvas.svelte
│   │   │   ├── ZoneEditor.svelte
│   │   │   └── Toolbar.svelte
│   │   └── pages/
│   │       ├── Dashboard.svelte
│   │       ├── Diagnostics.svelte
│   │       ├── Map.svelte
│   │       ├── Mission.svelte
│   │       └── Settings.svelte
├── package.json
├── vite.config.ts
└── tsconfig.json
```

## Seitenmodell

### 1. Dashboard

Zweck:

- laufenden Zustand sehen
- Basissteuerung
- schneller Hardware- und Missionsueberblick
- bewusst getrennt vom eigentlichen Mapping-Editor

Inhalte:

- Verbindung ok/getrennt
- aktiver Op/State
- Batterie
- GPS/RTK-Status
- Bumper/Lift/Motorfehler
- IMU heading/roll/pitch
- Buttons:
  - Start
  - Stop
  - Dock
- kleine Karte mit:
  - Roboterposition
  - Dock
  - Perimeter
  - aktive Route

### 2. Diagnose

Zweck:

- Hardwaretests
- Motor-Kalibrierung
- Sensorpruefung

Inhalte:

- Sensor-Liveansicht
- IMU-Livewerte
- Motor-Test links/rechts
- Maehmotor-Test
- Button-Test
- Kalibrierung:
  - Encoder-Ticks pro Umdrehung
  - Drehrichtung links/rechts invertieren
- Ergebnisanzeige:
  - Ticks
  - Richtung
  - Abweichung

Wichtig:

- Diagnose-Aktionen nur async
- UI zeigt klar:
  - laeuft
  - fertig
  - fehlgeschlagen

### 3. Karte

Zweck:

- Perimeter
- Dock
- Zonen
- Ausschlussflaechen
- Hindernisse

Der Kartenbereich ist ein eigener Arbeitsmodus und kein Nebenpanel des
Dashboards.

Phase 1:

- Raster + Pan/Zoom
- Perimeterpunkte setzen, verschieben, loeschen
- Dockpunkt setzen
- Zonenpolygone zeichnen
- NoGo-Zonen zeichnen und bearbeiten
- Roboterposition anzeigen

Phase 2:

- Dock-Pfad
- Hindernisse
- Zoneneinstellungen
- Reihenfolge
- Positionsmodus absolut/relativ

Hinweis zum Positionsmodus:

- In CaSSAndRA entspricht das eher einem Positionsmodus als einem simplen
  "GPS-Ursprung setzen".
- `absolut` und `relativ` sollten in der neuen WebUI als bewusster Modus
  modelliert werden.
- Die absolute Variante wird sehr wahrscheinlich durch die RTK-Base-Station
  bzw. deren Weltbezug bestimmt.
- Die relative Variante ist eher ein lokaler Kartenarbeitsraum fuer Mapping und
  Korrektur.

Phase 3:

- Import/Export
- Snapping
- Undo/Redo

### 4. Mission

Zweck:

- Zonen auswaehlen
- Reihenfolge festlegen
- Maehen starten
- Docken
- Resume / Abbruch

### 5. Einstellungen

Zweck:

- nur die wirklich noetigen Werte zuerst
- spaeter mehr

Nicht als erste Prioritaet behandeln.

## State-Management

Ein zentrales, simples Modell reicht.

### connection store

- `connected`
- `lastSeen`
- `reconnectState`

### telemetry store

- letzter Snapshot
- abgeleitete Werte fuer die UI

### map store

- aktueller Kartenstand
- dirty flag
- selected tool
- selected zone

### diagnostics store

- laufender Test
- letzter Motor-Test
- letzte IMU-Kalibrierung
- Busy/Error/Success

## Kommunikationsmodell

### WebSocket

Nur fuer:

- Live-Telemetrie
- Event-Meldungen
- optional Log-Events
- Async-Diagnose-Status

Beispieltypen:

- `type: "state"`
- `type: "event"`
- `type: "diag_progress"`
- `type: "diag_result"`
- `type: "ping"`

### REST

Nur fuer:

- Config laden/speichern
- Map laden/speichern
- Schedule laden/speichern
- Diagnose-Request starten
- Mission-Request senden

Regel:

- REST startet Aktionen
- WebSocket liefert Status und Ergebnis zurueck

So werden blockierende HTTP-Requests fuer lange Diagnosen vermieden.

### REST-Endpunkte fuer V1

Die neue WebUI braucht fuer V1 nur einen kleinen Kern an Endpunkten:

- `GET /api/version`
  - Pi- und MCU-Version lesen
- `GET /api/map`
  - Karte laden
- `POST /api/map`
  - Karte speichern
- `GET /api/schedule`
  - Zeitplaene lesen
- `PUT /api/schedule`
  - Zeitplaene speichern
- `GET /api/config`
  - Konfiguration lesen
- `PUT /api/config`
  - Kalibrier- und Basiswerte speichern
- `POST /api/diag/motor`
  - Motor-Test oder Tick-Kalibrierung starten
- `POST /api/diag/mow`
  - Maehmotor ein/aus testen
- `POST /api/diag/imu_calib`
  - IMU-Kalibrierung starten

### WebSocket-Nachrichtenformat fuer V1

Die WebSocket-Verbindung bleibt bewusst klein und stabil.

Pfad:

- `/ws/telemetry`

V1-Nachrichten:

- `type: "state"`
  - kompletter Live-Snapshot fuer Dashboard, Diagnose und Kartenposition
- `type: "event"`
  - optionale kompakte Ereignisse spaeter
- `type: "diag_progress"`
  - optional spaeter fuer laufende Diagnosefortschritte
- `type: "diag_result"`
  - optional spaeter fuer Diagnose-Endergebnisse
- `type: "ping"`
  - optional spaeter fuer Verbindungswartung

Regel fuer V1:

- Das Frontend soll mit vollstaendigen `state`-Paketen funktionieren.
- Falls spaeter partielle Deltas kommen, muss der Store robust patchen statt
  blind zu ersetzen.

## Diagnose-Konzept fuer Motor-Kalibrierung

Das ist einer der wichtigsten Teile der neuen WebUI.

Die Motor-Kalibrierung muss zwei explizite, getrennte Aufgaben abdecken:

1. Encoder-Ticks pro Umdrehung messen
2. Drehrichtung validieren

Beides darf in der UI nicht nur als allgemeiner "Motor-Test" erscheinen,
sondern als klar benannte Kalibrierschritte mit dokumentiertem Ergebnis.

Jeder Test sollte so laufen:

1. UI sendet `POST /api/diag/.../start`
2. Backend startet Worker oder Task
3. Backend sendet ueber WebSocket:
   - `diag_progress`
   - `diag_result`
4. UI zeigt:
   - laeuft
   - ticks
   - ticks target
   - delta
   - Richtung ok/falsch

### 1. Encoder-Ticks pro Umdrehung messen

Ziel:

- Rad dreht sich genau einmal
- gemessene Ticks werden mit dem Sollwert verglichen
- der Wert kann direkt gespeichert werden

Empfohlener Ablauf:

1. Benutzer waehlt `links` oder `rechts`
2. UI startet Test `1 Umdrehung`
3. Backend dreht genau eine Radumdrehung oder stoppt bei der konfigurierten
   Tick-Zahl
4. Backend liefert:
   - gemessene Ticks
   - Soll-Ticks
   - Abweichung in Prozent
5. UI zeigt:
   - `Soll: 320`
   - `Ist: 334`
   - `Abweichung: +4.4%`
6. Benutzer kann den neuen Wert fuer
   `ticks_per_revolution` bestaetigen und speichern

Wichtig:

- Die UI muss klar sagen, ob nur gemessen oder auch gespeichert wurde.
- Links und rechts sollten getrennt testbar sein, auch wenn am Ende nur ein
  gemeinsamer Konfigurationswert gespeichert wird.

### Motor-Richtung

Ziel:

- Vorwaerts-Befehl fuehrt auch physisch zu Vorwaerts-Drehung des jeweiligen Rads
- falls nicht, wird die Richtung fuer links oder rechts invertiert

Expliziter Test:

- Vorwaerts links
- Vorwaerts rechts

Danach manuelle Bestaetigung:

- korrekt
- invertiert

Dann Konfigurationswerte speichern:

- `invert_left_motor`
- `invert_right_motor`

Empfohlener UI-Ablauf:

1. Benutzer startet `Richtung links pruefen`
2. Backend gibt kurzen Vorwaertsimpuls auf das linke Rad
3. UI fragt:
   - `Drehte sich das linke Rad vorwaerts?`
   - `Ja`
   - `Nein, invertieren`
4. Bei `Nein` setzt die UI `invert_left_motor = true`

Dasselbe danach fuer rechts mit `invert_right_motor`.

Wichtig:

- Richtungsvalidierung und Tick-Kalibrierung sind zwei getrennte Schritte.
- Erst Richtung validieren, dann Tick-Wert fein kalibrieren.
- Die aktuelle Konfiguration muss in der UI sichtbar sein:
  - links normal/invertiert
  - rechts normal/invertiert

## Karteneditor-Konzept

Nicht zu gross anfangen.

Empfehlung:

- eigener Canvas-Editor
- keine grossen Kartenframeworks
- keine OSM- oder Satelliten-Hintergrundkarte
- stattdessen ein einfaches Gittermuster als lokaler Arbeitsraum
- lokale Meterkoordinaten im Mittelpunkt

Erst spaeter:

- GeoJSON
- Imports
- Snap-Logik
- komplexere Werkzeuge

## Robustheit auf dem Pi

Das ist entscheidend und beeinflusst die Architektur direkt.

- Frontend-Build nicht auf dem Pi entwickeln
- nur fertige `dist/` deployen
- wenige Dependencies
- keine schweren Chart- und Map-Libs zuerst
- keine riesigen State-Frameworks
- WebSocket-Reconnect simpel und idempotent
- jede Route muss ohne Side Effects mounten
- Diagnose nie ueber lang blockierende HTTP-Requests

## Migrationsstrategie

Nicht alles auf einmal ersetzen.

### Phase 1

Neues Projekt `webui-svelte/`

Minimaler End-to-End-Pfad:

- verbinden
- Telemetrie anzeigen
- Start/Stop/Dock

### Phase 2

Diagnose-Seite:

- Motor-Test
- IMU live
- Sensoren

### Phase 3

Karte und Zonen:

- Perimeter
- Dock
- Zonen

### Phase 4

Mission:

- Zonen waehlen
- Maehen starten
- Docken

### Phase 5

Erst danach:

- Historie
- Statistiken
- Einstellungen ausbauen
- alte Vue-WebUI abschalten

## Minimaler MVP

Wenn schnell wieder ein arbeitsfaehiges UI entstehen soll, dann nur:

- Dashboard
- Diagnose
- Karte

Mit genau diesen Features:

### Dashboard

- Verbindung
- Batterie
- GPS
- IMU
- Bumper/Lift
- Start/Stop/Dock

### Diagnose

- linkes/rechtes Rad testen
- Maehmotor testen
- Encoder-Ticks anzeigen
- IMU anzeigen
- Richtungsinversion setzen

### Karte

- Perimeter
- Dock
- Zonen
- Speichern

Alles andere kann spaeter folgen.

## Klare Empfehlung

Wenn wirklich neu angefangen wird:

- neues Verzeichnis `webui-svelte/`
- `Svelte + Vite + TypeScript`
- eigener einfacher Canvas-Karteneditor
- REST fuer Start/Speichern
- WebSocket fuer Live-Zustand und Diagnose-Ergebnisse
- Diagnose strikt asynchron, nie blockierend
- zuerst MVP mit:
  - Dashboard
  - Diagnose
  - Karte

## Naechster Schritt

Aus diesem Konzept kann als naechstes ein konkreter Umsetzungsplan entstehen:

- Ordnerstruktur
- Stores
- WS-Nachrichtenformat
- REST-Endpunkte
- Implementierungsreihenfolge in 5 bis 7 Schritten
