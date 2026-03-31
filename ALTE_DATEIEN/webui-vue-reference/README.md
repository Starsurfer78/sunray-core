# Sunray WebUI

Bedienoberfläche für den **sunray-core** Rasenmäher-Roboter.
Gebaut mit Vue 3 + TypeScript + Vite + Tailwind CSS.

---

## Tech-Stack

| Paket | Version | Zweck |
|-------|---------|-------|
| Vue 3 | ^3.5 | Reaktives UI-Framework |
| Vue Router 4 | ^4.6 | Client-seitiges Routing (Hash-History) |
| Tailwind CSS 4 | ^4.2 | Utility-CSS (via `@tailwindcss/vite`) |
| Vite | ^8.0 | Build-Tool + Dev-Server |
| TypeScript | ~5.9 | Typsicherheit |

Keine externen Kartenbibliotheken. Alle Karten-Renderings sind eigene `<canvas>`-Implementierungen.

---

## Schnellstart

Voraussetzung fuer den aktuellen Frontend-Stack:

- `Node.js >= 20`
- empfohlen: eine aktuelle LTS-Version von Node 20

Mit `Node 18` kann `npm install` noch Warnungen statt Fehler liefern, der
Produktions-Build (`npm run build`) ist damit aber nicht verlaesslich moeglich.

```bash
cd sunray-core/webui

# Abhängigkeiten installieren (einmalig)
npm install

# Dev-Server starten (mit Proxy → localhost:8765)
npm run dev

# Produktions-Build (→ dist/ — wird von Crow unter / ausgeliefert)
npm run build
```

Der Dev-Server läuft auf `http://localhost:5173` und proxyt `/api/*` und `/ws/*` automatisch nach `http://localhost:8765` (Crow C++ Backend).

Wenn im Backend `api_token` gesetzt ist, muss derselbe Wert in der WebUI in
der Topbar unter `API-Token` gespeichert werden. Erst dann liefern
geschuetzte Endpunkte wie `/api/config` Inhalte, und WebSocket-Kommandos sind
autorisiert.

---

## Projektstruktur

```
webui/
├── src/
│   ├── App.vue                     # Root-Komponente: Topbar + Layout
│   ├── main.ts                     # Vue-App Bootstrap
│   ├── router/
│   │   └── index.ts                # Routen-Definition (8 Views)
│   ├── composables/
│   │   ├── useTelemetry.ts         # WebSocket-Verbindung, Telemetrie-State, Log-Stream
│   │   └── useSessionTracker.ts    # Mäh-Session-Aufzeichnung (localStorage)
│   ├── views/
│   │   ├── Dashboard.vue           # Live-Karte (RobotMap)
│   │   ├── MapEditor.vue           # Karten-Editor: Perimeter, Zonen, Dock, Hindernisse
│   │   ├── Settings.vue            # Einstellungen: System, Roboter-Config, Diagnose
│   │   ├── History.vue             # Mäh-Session-Verlauf
│   │   ├── Statistics.vue          # Aggregierte Statistiken
│   │   ├── Simulator.vue           # Simulator-Steuerung (--sim Modus)
│   │   ├── Diagnostics.vue         # Telemetrie-Rohdaten + Motor-Tests
│   │   └── Scheduler.vue           # Zeitplan (Platzhalter — folgt)
│   └── components/
│       ├── RobotMap.vue            # Canvas-Karte (Roboter-Icon, Dock, Grid)
│       ├── RobotSidebar.vue        # Rechte Seitenleiste: Status, GPS, Akku, Steuerung
│       ├── LogPanel.vue            # Ausklappbares Log + GPS-Detail-Panel (unten)
│       └── VirtualJoystick.vue     # Touch/Maus Joystick-Overlay
├── design/
│   └── dashboard_reference.html   # Unveränderliches Design-Referenz-Mockup
├── dist/                           # Build-Output (von Crow unter / ausgeliefert)
├── vite.config.ts
└── package.json
```

---

## Layout

```
┌─────────────────────────────────────────────────────────────────┐
│  Topbar: Logo · Status-Pill · Op-Pill · Akku · NOTAUS · Nav    │
├─────────────────────────────────────────────────────────────────┤
│                                            │                    │
│   <router-view>                            │  RobotSidebar      │
│   (aktueller View füllt diesen Bereich)    │  - Status          │
│                                            │  - GPS             │
│                                            │  - Akku            │
│                                            │  - Steuerung       │
│                                            │  - Joystick        │
│──────────────────────────────────────────  │                    │
│   LogPanel (ausklappbar, unten)            │                    │
└─────────────────────────────────────────────────────────────────┘
```

Die Sidebar ist kollapsibel (‹/› Toggle). Der LogPanel klappt von unten auf und zeigt wahlweise **Live Log** oder **GPS-Details**.

---

## Views

### Dashboard (`/`)

Zeigt die Live-Karte über den gesamten Inhaltsbereich.

- **RobotMap-Komponente**: Canvas-Raster-Karte, Roboter-Icon mit Richtungspfeil, Dock-Marker
- GPS-Koordinaten-Badge (oben links, WGS84)
- Zoom-Buttons: `+` / `−` / `⊙` (Roboter zentrieren)
- Empfängt Pose (`x`, `y`, `heading`, `op`) live aus `useTelemetry`

---

### Karte / MapEditor (`/map`)

Interaktiver Canvas-Editor zur Konfiguration der Mähkarte.

#### Werkzeuge

| Werkzeug | Taste/Aktion | Farbe | Beschreibung |
|----------|-------------|-------|--------------|
| **Zeiger** | Klicken+Ziehen | — | Punkte verschieben; freier Bereich = Karte verschieben |
| **Perimeter** | Klick = Punkt, Doppelklick/⊙Erstpunkt = schließen | Grün `#22c55e` | Erlaubter Mähbereich |
| **No-Go** | wie Perimeter | Rot `#ef4444` | Verbotene Zonen (werden aus Pfad herausgeschnitten) |
| **Mähzone** | wie Perimeter | Lila `#a855f7` | Benannte Unter-Zonen mit eigenen Einstellungen |
| **Dock-Pfad** | Klick = Wegpunkt, Doppelklick = abschließen | Himmelblau `#38bdf8` | Anfahrtsweg; letzter Punkt = Dock-Station |
| **Hindernis** | Einzel-Klick | Amber `#d97706` | Simulations-Hindernisse (Kreis r=0.5 m) |
| **Löschen** | Klick auf nächsten Punkt | — | Entfernt den nächsten Vertex |

> Mähzone und Dock-Pfad erfordern einen vorhandenen Perimeter (Button sonst deaktiviert).

#### Tastaturkürzel

| Taste | Funktion |
|-------|----------|
| `Backspace` / `Delete` | Letzten Zeichenpunkt entfernen |
| `Escape` | Aktuelle Zeichnung abbrechen |
| Scrollrad | Zoom |
| Mittlere Maustaste + Ziehen | Pan |

#### Mähzonen-Panel

Erscheint unterhalb der Karte sobald Mähzonen vorhanden sind:

- Zonen sortiert nach Ausführungsreihenfolge (`order`)
- **Inline-Editor** (Klick auf Zeile): Name, Schnittbreite (m), Geschwindigkeit (m/s), Muster (Streifen/Spirale)
- ▲/▼ zum Umordnen, ✕ zum Löschen
- Selektierte Zone wird auf der Karte hell hervorgehoben

#### GeoJSON Import/Export

- **↑ GeoJSON**: Lädt eine `.geojson`-Datei → POST `/api/map/geojson` → Karte wird neu geladen
- **↓ GeoJSON**: GET `/api/map/geojson` → Download als `sunray-map.geojson`

#### GPS-Ursprung

Lat/Lon des lokalen Koordinaten-Ursprungs. Wird für WGS84 ↔ lokal Konvertierung benötigt. „Von GPS" übernimmt den aktuellen GPS-Fix aus der Telemetrie.

#### Karten-Format (JSON)

```json
{
  "perimeter":  [[x,y], ...],
  "mow": [
    [x, y],                          // kompakt: kein Rückwärtsabschnitt
    { "p": [x,y], "rev": true },     // Rückwärts-Segment (K-Turn)
    { "p": [x,y], "slow": true }     // Verlangsamung (Wende-Übergang)
  ],
  "dock":       [[x,y]],
  "dockPath":   [[x,y], ...],
  "exclusions": [[[x,y], ...], ...],
  "obstacles":  [[x,y], ...],
  "zones": [
    {
      "id": "1710000000000",
      "order": 1,
      "polygon": [[x,y], ...],
      "settings": {
        "name": "Hauptfläche",
        "stripWidth": 0.18,
        "speed": 1.0,
        "pattern": "stripe"
      }
    }
  ],
  "origin": { "lat": 48.1234567, "lon": 11.1234567 }
}
```

Koordinaten sind lokale Meter (x = Ost, y = Nord). Alle Felder optional (Defaults: leere Arrays).

---

### Einstellungen (`/settings`)

Drei Sektionen:

#### Sektion 1 — System
- Mission Service Version (GET `/api/version`)
- WebSocket-Verbindungsstatus aus `useTelemetry`
- Uptime des Roboters (h:mm)
- GPS-Text aus Telemetrie
- Karten-Pfad (Link zu `/map`)
- Links zu `/docs` und `/redoc`

#### Sektion 2 — Roboter-Konfiguration
Lädt beim Mount: GET `/api/config`

Bearbeitbare Felder:

| Config-Key | Label | Einheit |
|------------|-------|---------|
| `strip_width_default` | Standard-Schnittbreite | m |
| `speed_default` | Standard-Geschwindigkeit | m/s |
| `battery_low_v` | Akku niedrig | V |
| `battery_critical_v` | Akku kritisch | V |
| `gps_no_motion_threshold_m` | GPS No-Motion-Schwelle | m |
| `rain_delay_min` | Regen-Verzögerung | min |

Speichern sendet nur geänderte Keys (PUT `/api/config`). Speichern-Button nur aktiv wenn Änderungen vorhanden.

#### Sektion 3 — Diagnose
- Motor-Tests: Buttons → POST `/api/sim/motor` (Left/Right/Mow)
- IMU-Kalibrierung: POST `/api/sim/imu_calib` (Statusanzeige: idle/running/done)
- Log-Stream: WebSocket `type:"log"` Nachrichten, max. 200 Zeilen, neueste zuerst, Clear-Button

---

### Verlauf (`/history`)

Automatisch aufgezeichnete Mäh-Sessions (gespeichert im `localStorage`).

- Mini-Pfad-Canvas (80×80 px) pro Session: Cyan-Linie, grüner Startpunkt, roter Endpunkt
- Datum, Uhrzeit, Dauer, Strecke, Akkuspannung Start→Ende
- Aktive Session: blinkendes „● Aufzeichnung läuft" Badge + Puls-Indikator
- Session löschen (✕) oder alle löschen (Bestätigung erforderlich)

Sessions werden nur gespeichert wenn:
- Op war `"Mow"` (erkannt via `useTelemetry`)
- Mindestdauer ≥ 30 Sekunden

---

### Statistiken (`/statistics`)

Aggregierte Auswertung aller aufgezeichneten Sessions:

- **4 Summary-Cards**: Anzahl Sessions, Gesamtzeit, Gesamtstrecke, Ø Session-Dauer
- **Balkendiagramm** (Canvas): Letzte 10 Sessions nach Dauer, Datums-Beschriftung, Wert über Balken
- **Längste Session**: Datum, Dauer, Strecke

---

### Simulator (`/simulator`)

Nur relevant wenn sunray-core mit `--sim` gestartet wurde.

- **Bumper auslösen**: Links / Rechts / Beide → POST `/api/sim/bumper`
- **GPS-Qualität**: Fix / Float / No-Fix → POST `/api/sim/gps`
- **Lift-Sensor**: Heben / Senken → POST `/api/sim/lift`

Fehler werden inline als rotes Banner angezeigt.

---

### Diagnose (`/diagnostics`)

- **Telemetrie-Rohdaten**: JSON-Pretty-Print des aktuellen Telemetrie-Objekts (live)
- **Motor-Tests**: Platzhalter-Buttons (deaktiviert — folgt in nächster Iteration)
- **Log-Stream**: Platzhalter (Log läuft bereits im LogPanel am unteren Rand)

---

### Zeitplan (`/schedule`)

Platzhalter — Implementierung folgt in der nächsten Iteration.
Geplant: wöchentliche Mäh-Zeitfenster, Regen-Verzögerung, automatischer Start aus ChargeOp.

---

## Komponenten

### RobotMap

Canvas-Karte für das Dashboard.

**Props:** `x: number`, `y: number`, `heading: number` (rad), `op: string`

**Exponierte Methoden (via `defineExpose`):** `zoomIn()`, `zoomOut()`, `centerRobot()`

**Rendering:**
- Dunkles Raster-Grid (#1f2937)
- Dock-Marker: Amber-Rechteck mit „D", Beschriftung „Dock-Station"
- Roboter: Kreis (#22d3ee) mit Richtungs-Dreieck, Spur der letzten Positionen
- Op-abhängige Farbe: Mow = Cyan, Dock/Charge = Blau, Error = Rot, Idle = Grau

---

### RobotSidebar

Kollapsible rechte Seitenleiste mit vollständigem Roboter-Status und Steuerung.

**Abschnitte:**
- **Status**: Op-Label, aktuelle Zone, Position (x/y/heading)
- **GPS**: Fix-Typ (RTK-Fix/Float/Kein Fix) mit Farbindikator, Satelliten-Anzeige Rover/Base
- **Akku**: Spannung, Ladebalken (22 V = 0%, 29.4 V = 100%), Ladespannung
- **Steuerung**: Start / Dock / Stop Buttons (kontextabhängig aktiviert/deaktiviert)
- **Joystick**: Ausklappbarer VirtualJoystick

**Button-Logik:**
- Start: aktiv wenn `op ∈ {Idle, Charge}`
- Dock: aktiv wenn `op = Mow`
- Stop: aktiv wenn `op ∉ {Idle, Charge, Error}`

---

### LogPanel

Ausklappbares Panel am unteren Rand (über allen Views).

**Tabs:**
- **Live Log**: WebSocket `type:"log"` Nachrichten, neueste zuerst, max. 200 Zeilen, Clear-Button, Auto-Scroll
- **GPS-Details**: Fix-Typ, Lat/Lon, Positions-Genauigkeit, DGPS-Alter, Satellitenanzahl

---

### VirtualJoystick

Touch- und Maus-fähiger Joystick für manuelle Steuerung.

- Kreisförmige Stick-Darstellung mit Deadzone
- Sendet `{"cmd":"setMotor","left":v,"right":v}` via WebSocket
- 50 ms Intervall solange gedrückt; bei Release → Stop-Befehl

---

## Composables

### `useTelemetry`

Singleton-WebSocket-Verbindung. Hält die Verbindung über alle View-Wechsel.

```typescript
const {
  telemetry,    // Ref<Telemetry> — aktueller Roboter-Zustand
  connected,    // Ref<boolean>
  lastUpdate,   // Ref<number> — Date.now() der letzten Nachricht
  logs,         // Ref<readonly string[]> — max. 200 Log-Zeilen
  sendCmd,      // (cmd: string, extra?: object) => void
  clearLogs,    // () => void
} = useTelemetry()
```

**Telemetrie-Felder** (gefroren — Änderungen erfordern simultanes Backend-Update):

| Feld | Typ | Beschreibung |
|------|-----|--------------|
| `op` | string | `Mow` \| `Idle` \| `Dock` \| `Charge` \| `EscapeReverse` \| `GpsWait` \| `Error` |
| `x` | number | Lokale Meter Ost |
| `y` | number | Lokale Meter Nord |
| `heading` | number | Radiant (0 = Ost) |
| `battery_v` | number | Akkuspannung V |
| `charge_v` | number | Ladespannung V |
| `gps_sol` | number | 0=Kein Fix, 4=RTK-Fix, 5=RTK-Float |
| `gps_text` | string | Menschenlesbarer GPS-Status |
| `gps_lat` | number | Breitengrad WGS84 |
| `gps_lon` | number | Längengrad WGS84 |
| `bumper_l` | boolean | Linker Bumper ausgelöst |
| `bumper_r` | boolean | Rechter Bumper ausgelöst |
| `motor_err` | boolean | Motor-Fehler aktiv |
| `uptime_s` | number | Sekunden seit Start |

**WebSocket-Protokoll:**
- Endpoint: `ws://host/ws/telemetry`
- Server sendet `{"type":"state", ...}` mit 10 Hz
- Server sendet `{"type":"ping"}` als Keepalive bei Datenstillstand
- Server sendet `{"type":"log","text":"..."}` für Log-Nachrichten
- Client sendet `{"cmd":"start|stop|dock|charge|setpos", ...}`
- Auto-Reconnect nach 3 s bei Verbindungsabbruch

---

### `useSessionTracker`

Zeichnet Mäh-Sessions automatisch auf, basierend auf dem `op`-Feld der Telemetrie.

```typescript
const {
  sessions,       // Ref<readonly MowSession[]> — gespeicherte Sessions (neueste zuerst)
  recording,      // Ref<boolean> — läuft gerade eine Session?
  clearSessions,  // () => void
  deleteSession,  // (id: string) => void
} = useSessionTracker()
```

**MowSession-Format:**

```typescript
interface MowSession {
  id:         string              // ISO-Zeitstempel (Unique Key)
  startedAt:  number              // Date.now() ms
  endedAt:    number
  durationMs: number
  distanceM:  number              // Integrierte Odometrie (m)
  battStart:  number              // Spannung V bei Start
  battEnd:    number              // Spannung V bei Ende
  path:       [number, number][]  // Abgetasteter Pfad [x, y] in lokalen Metern
}
```

**Speicher-Grenzen:**
- Mindestdauer: 30 s (kürzere Sessions werden verworfen)
- Max. Sessions: 50 (älteste werden herausgedrängt)
- Pfad-Abtastung: alle 2 s, max. 400 Punkte pro Session
- Persistenz: `localStorage` (Key: `sunray_sessions`)

---

## REST API

Alle Endpunkte werden vom Crow C++ Server unter `http://host:8765` bereitgestellt.

| Methode | Pfad | Beschreibung |
|---------|------|--------------|
| GET | `/api/config` | Gesamte Konfiguration als JSON |
| PUT | `/api/config` | Partielle Konfiguration aktualisieren |
| GET | `/api/map` | Kartenfile laden (JSON) |
| POST | `/api/map` | Kartenfile speichern |
| GET | `/api/map/geojson` | Karte als GeoJSON-FeatureCollection exportieren |
| POST | `/api/map/geojson` | GeoJSON importieren (WGS84 → lokal) |
| POST | `/api/sim/bumper` | Bumper auslösen `{ side: "left"\|"right"\|"both" }` |
| POST | `/api/sim/gps` | GPS-Qualität setzen `{ quality: "fix"\|"float"\|"none" }` |
| POST | `/api/sim/lift` | Lift-Sensor `{ raised: true\|false }` |
| POST | `/api/sim/obstacle` | Hindernis setzen `{ x, y, radius }` |
| POST | `/api/sim/obstacles_clear` | Alle Hindernisse löschen |

---

## Design-System

**Farbschema „Dark Blue"** (Referenz: `design/dashboard_reference.html` — nicht verändern).

| Token | Hex | Verwendung |
|-------|-----|------------|
| Hintergrund (App) | `#0a0f1a` | Body-Hintergrund |
| Hintergrund (Panel) | `#0f1829` | Cards, Sidebar, Toolbar |
| Hintergrund (Karte) | `#070d18` | Canvas-Hintergrund |
| Border | `#1e3a5f` | Alle Panel-Rahmen |
| Text (primär) | `#e2e8f0` | Überschriften, Werte |
| Text (sekundär) | `#94a3b8` | Labels, Subtext |
| Text (gedimmt) | `#4b6a8a` | Disabled, Metadaten |
| Blau (Akzent) | `#60a5fa` | Links, Buttons, GPS-Werte |
| Blau (Perimeter) | `#2563eb` | Perimeter-Polygon |
| Cyan (Pfad) | `#22d3ee` | Mähpfad, Roboter-Marker |
| Grün (Perimeter fill) | `#22c55e` | Perimeter-Füllung |
| Rot (No-Go) | `#ef4444` | Exclusion-Zonen |
| Lila (Mähzone) | `#a855f7` | Mähzonen |
| Himmelblau (Dock) | `#38bdf8` | Dock-Pfad |
| Amber (Hindernis/Dock) | `#d97706` | Sim-Hindernisse, Dock-Marker |
| NOTAUS | `#dc2626` / `#450a0a` | Emergency-Stop-Button |

**Regeln:**
- Alle Farb-Overrides mit `!important` (Widget-Frame-Schutz)
- NOTAUS immer sichtbar in der Topbar
- Kein externer Kartendienst — nur Canvas/SVG

---

## Entwicklungs-Hinweise

### Dev-Server Proxy

`vite.config.ts` proxyt im Entwicklungsmodus:
- `/ws/*` → `ws://localhost:8765` (WebSocket)
- `/api/*` → `http://localhost:8765` (REST)

Der sunray-core Prozess muss laufen: `./sunray-core --sim`

### Build & Deploy

```bash
npm run build          # Erzeugt webui/dist/
```

Der C++ Crow-Server liefert `dist/` statisch unter `/` aus (via `WebSocketServer` A.6). Kein separater HTTP-Server notwendig.

### TypeScript prüfen

```bash
npx vue-tsc --noEmit   # Nur Type-Check, kein Build
```

### Neue View hinzufügen

1. `src/views/MeineView.vue` anlegen
2. In `src/router/index.ts` eintragen (path, component, name, label, icon)
3. Der Nav-Link erscheint automatisch in der Topbar

### Frozen: Telemetrie-Format

Die Felder in `useTelemetry.ts → interface Telemetry` und die Op-Strings (`"Mow"`, `"Idle"`, etc.) sind eingefroren. Eine Änderung erfordert simultanes Update des C++ `WebSocketServer` und aller davon abhängigen Komponenten.
