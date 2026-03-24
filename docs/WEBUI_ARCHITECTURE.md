# WebUI Architektur

Letzte Aktualisierung: 2026-03-24

---

## Inhaltsverzeichnis

1. [Stack](#1-stack)
2. [Views](#2-views)
3. [Composables](#3-composables)
4. [Karten-Editor](#4-karten-editor)
5. [Design-System](#5-design-system)

---

## 1. Stack

| Technologie | Version | Zweck |
|-------------|---------|-------|
| **Vue 3** | Composition API, `<script setup>` | Reaktives UI-Framework |
| **Vite** | — | Dev-Server, Production-Build (`webui/dist/`) |
| **Tailwind CSS** | — | Utility-CSS; alle Farben via `!important` erzwungen |
| **Vue Router 4** | Hash-History (`#/`) | Client-seitiges Routing |
| **Vitest** | — | Unit-Tests für Composables (`useMowPath.test.ts`) |

**Build:**
```bash
cd webui
npm install       # erstmalig
npm run dev       # Dev-Server (hot reload)
npm run build     # Produziert webui/dist/ — von Crow unter / serviert
```

**Routing (Hash-History):**

| Pfad | View | Label |
|------|------|-------|
| `/#/` | `Dashboard.vue` | Dashboard |
| `/#/map` | `MapEditor.vue` | Karte |
| `/#/schedule` | `Scheduler.vue` | Zeitpläne |
| `/#/history` | `History.vue` | Verlauf |
| `/#/statistics` | `Statistics.vue` | Statistiken |
| `/#/diagnostics` | `Diagnostics.vue` | Diagnose |
| `/#/simulator` | `Simulator.vue` | Simulator |
| `/#/settings` | `Settings.vue` | Einstellungen |

**Globale Navigation (`App.vue`):** Topbar mit NOTAUS immer sichtbar; Seitenleiste via `RobotSidebar.vue`.

---

## 2. Views

### Dashboard (`views/Dashboard.vue`)

**Zweck:** Echtzeit-Kartenanzeige mit Live-Roboter-Position.

**Komponenten:**
- `RobotMap.vue` — Canvas-basierte Karte mit Roboter-Marker und Heading-Pfeil
- `VirtualJoystick.vue` — Touchscreen-Joystick für manuelle Steuerung (optional)

**API-Calls:** Keine REST-Aufrufe; bezieht alle Daten aus `useTelemetry()`.

**Anzeigeelemente:**
- Vollbild-Karte (RobotMap) mit Perimeter, Mähpfad, Roboter-Position
- GPS-Badge oben links: `{lat}°N {lon}°E` oder `"Kein GPS"`
- Zoom-Buttons (+/−) und Roboter-Zentrieren-Button (⊙)

---

### Karteneditor (`views/MapEditor.vue`)

Ausführliche Beschreibung: → [Abschnitt 4: Karten-Editor](#4-karten-editor)

**Zweck:** Interaktiver Canvas-Editor für alle Kartenobjekte.

**API-Calls:**
- `GET /api/map` — beim Mount
- `POST /api/map` — beim Speichern
- `GET /api/map/geojson` / `POST /api/map/geojson` — GeoJSON-Export/Import

---

### Zeitpläne (`views/Scheduler.vue`)

**Zweck:** Wöchentliche Mäh-Zeitfenster konfigurieren.

**Status:** Platzhalter — noch nicht implementiert.

**Geplante API-Calls:**
- `GET /api/schedule`
- `PUT /api/schedule`

**Geplante Funktionen:** N Zeitfenster pro Tag, Wochentage, Regen-Verzögerung, automatischer Start aus ChargeOp.

---

### Verlauf (`views/History.vue`)

**Zweck:** Liste vergangener Mäh-Sessions mit Miniatur-Pfadvorschau.

**Komponenten:** `useSessionTracker` — liest `MowSession[]` aus `localStorage`.

**API-Calls:** Keine (rein lokaler Speicher via `localStorage['sunray_sessions']`).

**Anzeigeelemente pro Session:**
- Datum, Startzeit, Dauer
- Zurückgelegte Strecke (m oder km)
- Batteriespannung Start → Ende mit Δ
- Miniatur-Canvas mit gefahrenem Pfad (aus gesampleten `[x, y]`-Punkten)
- Löschen-Button

---

### Statistiken (`views/Statistics.vue`)

**Zweck:** Aggregierte Kennzahlen über alle gespeicherten Sessions.

**Komponenten:** `useSessionTracker` — berechnet Aggregate via `computed()`.

**API-Calls:** Keine.

**Kennzahlen:**
- Gesamt-Sessions, Gesamtdauer (h:mm), Gesamtstrecke (km)
- Durchschnittliche Session-Dauer
- Längste Session (Datum + Dauer)
- Balken-Chart: Strecke pro Session (Canvas, letzte N Sessions)

---

### Diagnose (`views/Diagnostics.vue`)

**Zweck:** Rohdaten-Inspektion und Motor-Tests.

**Komponenten:** `useTelemetry` — zeigt `telemetry`-Objekt als formatiertes JSON.

**API-Calls:**
- Motor-Test-Buttons: `POST /api/sim/motor` (ausstehend — Buttons deaktiviert)
- Log-Stream: `type:"log"`-Nachrichten aus useTelemetry (max. 200, neueste zuerst)

**Sektionen:**
1. Telemetrie-Rohdaten (JSON-Dump, grün, scrollbar)
2. Motor-Test (Buttons — deaktiviert bis REST-Endpoint implementiert)
3. Log-Stream (Placeholder — kommt nach REST-API)

---

### Simulator (`views/Simulator.vue`)

**Zweck:** Fault-Injection-Panel für den `--sim`-Modus.

**Nur im `--sim`-Modus nutzbar** (Hinweis im UI).

**API-Calls (alle `POST`):**

| Endpunkt | Body | Funktion |
|----------|------|----------|
| `/api/sim/bumper` | `{"side": "left"\|"right"}` | Bumper auslösen |
| `/api/sim/gps` | `{"quality": "fix"\|"float"\|"none"}` | GPS-Qualität setzen |
| `/api/sim/lift` | `{"active": true\|false}` | Lift-Sensor setzen |

---

### Einstellungen (`views/Settings.vue`)

**Zweck:** Konfigurations-Editor, System-Status, Diagnose-Sektion.

**Sektionen:**

**1. System**
- Mission Service Version (`/api/version`)
- WebSocket-Verbindungsstatus aus `useTelemetry`
- Uptime (h:mm aus `telemetry.uptime_s`)
- GPS-Text (`telemetry.gps_text`)
- Map-Pfad mit Link zu `/#/map`

**2. Roboter-Konfiguration** (bearbeitbare Config-Keys):

| Key | Beschreibung |
|-----|--------------|
| `strip_width_default` | Standard-Bahnbreite (m) |
| `speed_default` | Standard-Geschwindigkeit (m/s) |
| `battery_low_v` | Niedrigspannung → Dock (V) |
| `battery_critical_v` | Kritischspannung → Sofortstopp (V) |
| `gps_no_motion_threshold_m` | Schwelle für GPS-Bewegungslosigkeit (m) |
| `rain_delay_min` | Regen-Verzögerung (min) |

**API-Calls:**
- `GET /api/config` beim Mount (lädt alle Keys)
- `PUT /api/config` beim Speichern (nur geänderte Keys — dirty-Tracking)

**3. Diagnose**
- Motor-Tests (3 Buttons → `POST /api/sim/motor`)
- IMU-Kalibrierung (`POST /api/sim/imu_calib`, States: idle/running/done)
- Log-Stream (aus `useTelemetry.logs`, max. 200, neueste oben, Clear-Button)

---

## 3. Composables

### `useTelemetry.ts`

**Datei:** `webui/src/composables/useTelemetry.ts`

**Zweck:** WebSocket-Verbindung zu `/ws/telemetry` — Singleton-Pattern, Reconnect-Logik, Command-Sender.

**Exports:**

| Export | Typ | Beschreibung |
|--------|-----|--------------|
| `telemetry` | `Readonly<Ref<Telemetry>>` | Aktueller Telemetrie-Zustand |
| `connected` | `Readonly<Ref<boolean>>` | Verbindungsstatus |
| `lastUpdate` | `Readonly<Ref<number>>` | `Date.now()` beim letzten Update |
| `logs` | `Readonly<Ref<readonly string[]>>` | Log-Einträge (max. 200, neueste zuerst) |
| `sendCmd(cmd, extra?)` | Funktion | WebSocket-Command senden |
| `clearLogs()` | Funktion | Log-Array leeren |

**Telemetry-Interface:**

```typescript
interface Telemetry {
  type:       string     // "state"
  op:         string     // "Idle" | "Mow" | "Dock" | "Charge" | "EscapeReverse" | "GpsWait" | "Error"
  x:          number     // lokale Meter Ost
  y:          number     // lokale Meter Nord
  heading:    number     // Radiant, 0 = Ost
  battery_v:  number
  charge_v:   number
  gps_sol:    number     // 0=none, 4=RTK-Fix, 5=RTK-Float
  gps_text:   string
  gps_lat:    number
  gps_lon:    number
  bumper_l:   boolean
  bumper_r:   boolean
  motor_err:  boolean
  uptime_s:   number
}
```

**Reconnect-Logik:** Bei `onclose` oder `onerror` → `setTimeout(connect, 3000)` wenn noch mindestens ein aktiver Consumer vorhanden. Kein Reconnect wenn `refCount <= 0`.

**Singleton-Implementierung:** Alle Komponenten teilen dieselbe `ws`-Instanz und dieselben `ref`-Werte. `refCount` verhindert Disconnect solange ein Consumer aktiv ist.

**Nachrichten-Handling:**
- `type === "state"` → `telemetry.value` updaten
- `type === "log"` + `text: string` → in `logs` vorne einfügen (max. 200)

**Interne Cross-Composable-Exports:**
- `_telemetry` und `_connected` als rohe Refs für `useSessionTracker` — nicht direkt in Komponenten verwenden.

---

### `useMowPath.ts`

**Datei:** `webui/src/composables/useMowPath.ts`

**Zweck:** Mähbahnen-Berechnungsalgorithmus — erzeugt `MowPt[]`-Waypoint-Array aus Perimeter, Exclusions und Einstellungen.

**Exports:**

| Export | Beschreibung |
|--------|--------------|
| `MowPt` | `{p: Pt, rev?: boolean, slow?: boolean}` |
| `MowPathSettings` | Alle Berechnungsparameter |
| `DEFAULT_SETTINGS` | Standardwerte |
| `computeMowPath(perimeter, exclusions, settings)` | Hauptfunktion |
| `inwardOffsetPolygon(poly, dist)` | Randstreifen-Offset |

**MowPathSettings:**

| Parameter | Typ | Default | Beschreibung |
|-----------|-----|---------|--------------|
| `angle` | number | `0` | Streifenwinkel 0–179° (0 = horizontal/Ost-West) |
| `stripWidth` | number | `0.18` | Bahnbreite (m) |
| `overlap` | number | `10` | Überlappung 0–50 % |
| `edgeMowing` | boolean | `true` | Randstreifen aktivieren |
| `edgeRounds` | number | `1` | Randstreifen-Runden 1–5 |
| `turnType` | string | `"kturn"` | `"kturn"` oder `"zeroturn"` |
| `startSide` | string | `"auto"` | `"auto"`, `"top"`, `"bottom"`, `"left"`, `"right"` |

**Algorithmus (computeMowPath):**

```
1. Randstreifen (optional):
   - inwardOffsetPolygon(perimeter, stripWidth × edgeRounds) → Headland-Ringe
   - Jeder Ring: Polygon als Boustrophedon-Sequenz, Exclusion-Clipping per Segment

2. Hauptstreifen (Strip-Generierung):
   a. Bounding-Box rotieren um "angle" Grad
   b. Horizontale Scanlinien im rotierten Koordinatensystem erzeugen
   c. Jede Scanlinie gegen Perimeter clippen (clipLineToZone — BUG-4-Fix: dual clip)
   d. Jede geclippte Linie gegen Exclusions clippen (Intervall-Schnitt)
   e. Segmente kürzer als Mindestlänge verwerfen
   f. Ergebnis-Segmente zurückrotieren

3. Boustrophedon-Sortierung:
   - Nearest-Neighbor-Greedy: immer nächstgelegenen Streifen wählen
   - Streifen-Richtung (rechts→links oder links→rechts) alternieren

4. Übergänge:
   K-Turn:    P0 (Streifenende) → P1 (overshoot vorwärts/langsam) →
              P2 (rückwärts/langsam) → P3 (nextStart)
              rev=true + slow=true für P1 und P2
   Zero-Turn: direkte Verbindung von Ende zu Start mit routeAroundExclusions()

5. routeAroundExclusions(): Bypass-Berechnung um Exclusion-BBox
   (BEKANNTE EINSCHRÄNKUNG — BUG-008: nur oberhalb/unterhalb)
```

**K-Turn-Encoding in MowPt:**
- `rev: true` — Robot fährt rückwärts zu diesem Punkt
- `slow: true` — reduzierte Geschwindigkeit

**Kompatibilität:** Identisch mit C++ `MowPoint`-Struktur (`rev`/`slow`-Flags werden von Map.cpp erkannt).

**Bekannte Einschränkungen:**
- `inwardOffsetPolygon`: Vertex-Bisektoren-Methode — ungenau an konkaven Ecken (kein Clipping-Algorithm)
- Spiral-Pattern (`zone.settings.pattern = "spiral"`): nicht implementiert, wird als Streifen berechnet → BUG-012
- Exclusion-Bypass nur vertikal (oben/unten) → BUG-008
- K-Turn-Overshoot-Formel bei kurzen Randstreifen fehlerhaft → BUG-009
- Doppelklick-Polygon-Defekt im Editor → BUG-010 (betrifft alle berechneten Pfade)

---

### `useSessionTracker.ts`

**Datei:** `webui/src/composables/useSessionTracker.ts`

**Zweck:** Automatisches Aufzeichnen und Persistieren von Mäh-Sessions.

**Exports:**

| Export | Typ | Beschreibung |
|--------|-----|--------------|
| `sessions` | `Readonly<Ref<MowSession[]>>` | Liste aller gespeicherten Sessions |
| `recording` | `Readonly<Ref<boolean>>` | `true` während aktiver Mäh-Session |
| `clearSessions()` | Funktion | Alle Sessions löschen |
| `deleteSession(id)` | Funktion | Einzelne Session löschen |

**MowSession-Felder:**

| Feld | Beschreibung |
|------|--------------|
| `id` | ISO-Startzeit (unique key) |
| `startedAt` / `endedAt` | `Date.now()` ms |
| `durationMs` | Dauer in ms |
| `distanceM` | Integrierte Odometrie (m) |
| `battStart` / `battEnd` | Batteriespannung bei Start/Ende |
| `path` | Gesampelte `[x, y]`-Punkte (alle 2 s, max. 400) |

**Trigger:**
- Session startet: `telemetry.op === "Mow"` nach `!== "Mow"`
- Session endet: `telemetry.op !== "Mow"` nach `"Mow"` (wenn Dauer ≥ 30 s)

**Persistenz:** `localStorage["sunray_sessions"]` — max. 50 Sessions, älteste werden verworfen.

**Singleton-Mechanismus:** Module-level `watch(_telemetry, ...)` — läuft für die gesamte App-Laufzeit.

---

## 4. Karten-Editor

### Tool-System (5+2 Werkzeuge)

| Tool-Name | Funktion | Erzeugt/Editiert |
|-----------|----------|-----------------|
| `select` | Polygon/Vertex auswählen und verschieben | — |
| `perimeter` | Perimeter-Polygon zeichnen | `mapData.perimeter` |
| `exclusion` | No-Go-Zone zeichnen | `mapData.exclusions[]` |
| `zone` | Mähzone zeichnen | `mapData.zones[]` |
| `dockPath` | Dock-Anfahrts-Polyline zeichnen | `mapData.dockPath[]` |
| `obstacle` | Einzelnen Hindernispunkt setzen | `mapData.obstacles[]` |
| `delete` | Objekte löschen | — |

**Polygon-Zeichnen:** Linksklick = Punkt hinzufügen; Doppelklick = Polygon schließen (`closePolygon()`).

> **⚠ BUG-010:** Doppelklick löst `canvasClick` zweimal aus → doppelter Endpunkt im Polygon. Fix: `if (e.detail >= 2) return` am Anfang von `canvasClick`.

**Vertex-Drag:** Im `select`-Modus — Drag-Toleranz 12 px, Snap-to-First 12 px.

**Pan/Zoom:** Rechte Maustaste + Drag = Pan; Scroll = Zoom. Tastatur: Escape = aktuelles Werkzeug abbrechen.

---

### Koordinatensystem

```
Lokale Meter:
  +x = Ost
  +y = Nord
  Ursprung = origin_lat / origin_lon (WGS84, in mapData.origin gespeichert)

Canvas → Welt:  worldX = (canvasX - originX) / scale
                worldY = (originY - canvasY) / scale   (Y invertiert!)

Welt → Canvas:  canvasX = worldX * scale + originX
                canvasY = originY - worldY * scale
```

Alle Waypoints in `mapData` werden in lokalen Metern gespeichert.

GPS-Koordinaten → lokale Meter: Umrechnung via `origin_lat/lon` (equirectangular-Näherung, für Gärten < 1 km ausreichend).

---

### Mähbahnen-Berechnung

**Kollapsibles Panel** unterhalb der Zonen-Liste im Editor.

**Einstellungen im Panel:**

| Parameter | Bereich | Beschreibung |
|-----------|---------|--------------|
| Winkel | 0–179° | Streifenrichtung |
| Bahnbreite | — | Streifenbreite (m) |
| Überlappung | 0–50% | Streifenüberlapp |
| Randmähen | Checkbox | Headland-Ringe |
| Randstreifen-Runden | 1–5 | Anzahl inward-offset-Ringe |
| Wendetyp | K-Turn / Zero-Turn | Transitionstyp |
| Startseite | auto/oben/unten/links/rechts | — |
| Zone | Dropdown | Welche Zone berechnen |

**Workflow:**
1. „Bahnen berechnen" → `computePreview()` → ruft `computeMowPath()` auf
2. Vorschau auf Canvas: Cyan = Vorwärts, Orange = Rückwärts, gestrichelt = Rev-Segmente
3. „Als Mähpfad speichern" → `mapData.mow = previewMow`; nächster `POST /api/map` speichert den Pfad

**CaSSAndRA-Prinzip:** Der Algorithmus lehnt sich an das CaSSAndRA-Mähbahnen-System an — Boustrophedon-Streifen mit begrenztem inward-offset für Randstreifen.

**`clipPerimeterToZone`-Funktion:** Schneidet Streifen gleichzeitig gegen Perimeter UND Zone (dual-clip via Intervall-Schnitt max/min). Verhindert Überläufer an konkaven Polygon-Ecken.

---

### Zonen-Panel

**Zweck:** Verwaltung aller Mähzonen mit individuellen Einstellungen.

**Zone-Datenstruktur:**
```typescript
interface Zone {
  id:       string         // UUID
  polygon:  Pt[]           // lokale Meter
  order:    number         // Priorisierungsreihenfolge
  settings: ZoneSettings
}
interface ZoneSettings {
  name:       string       // Anzeigename
  stripWidth: number       // Bahnbreite (m)
  speed:      number       // Geschwindigkeit (m/s)
  pattern:    'stripe' | 'spiral'   // Muster (spiral nicht implementiert — BUG-012)
}
```

**Bedienung:**
- Zonen-Tool → Polygon zeichnen → Panel öffnet sich automatisch
- Inline-Editierung: name, stripWidth, speed, pattern
- Reihenfolge: ▲/▼ Buttons (`order`-Feld)
- Löschen: ✕ Button
- Canvas: Purple-Fill (`#a855f7`), ausgewählte Zone heller hervorgehoben

**Bekannte Einschränkung:**
- `pattern: "spiral"` im Dropdown wählbar, aber `computeMowPath()` ignoriert diesen Wert vollständig → immer Streifen-Ergebnis (BUG-012).

---

### Map-JSON-Format

```json
{
  "perimeter":  [[x, y], ...],
  "mow":        [{"p": [x, y], "rev": false, "slow": false}, ...],
  "dock":       [x, y],
  "dockPath":   [[x, y], ...],
  "exclusions": [[[x, y], ...], ...],
  "obstacles":  [[x, y], ...],
  "zones": [
    {
      "id": "uuid",
      "polygon": [[x, y], ...],
      "order": 0,
      "settings": {"name": "Zone A", "stripWidth": 0.18, "speed": 0.3, "pattern": "stripe"}
    }
  ],
  "origin": {"lat": 51.23456, "lon": 6.78901}
}
```

**GeoJSON-Import/Export:** Konvertiert Perimeter + Exclusions zu/von GeoJSON-Feature-Collections. Koordinaten: WGS84 → lokale Meter via `origin`.

---

## 5. Design-System

### Referenz-Datei

**Verbindliches Design-Mockup:** `webui/design/dashboard_reference.html`

Diese Datei darf **nicht verändert** werden ohne explizite Freigabe. Alle Views folgen diesem Farbschema.

### Farbschema „Dark Blue"

Alle Farben müssen mit `!important` erzwungen werden (Tailwind-Widget-Frame überschreibt sonst).

#### Hintergründe

| Element | Hex | Beschreibung |
|---------|-----|--------------|
| App-Hintergrund | `#070d18` | Haupthintergrund (nahezu schwarz-blau) |
| Navigationsleiste | `#060c17` | Dunkler Streifen, heller Text |
| Card/Panel | `#0f1829` | Karten-Hintergrund |
| Panel-Border | `#1e3a5f` | Blaue Rahmenlinie |
| Panel sekundär | `#0a1628` | Dunklerer Panel-Variante |

#### Text

| Element | Hex | Beschreibung |
|---------|-----|--------------|
| Primär-Text | `#e2e8f0` | Helles Grau-Weiß |
| Sekundär-Text | `#94a3b8` | Gedämpftes Grau-Blau |
| Deaktiviert | `#4b6a8a` | Dunkles Blau-Grau |
| Akzent (Links, Info) | `#60a5fa` | Mittleres Blau |

#### Interaktive Elemente

| Element | Hex | Beschreibung |
|---------|-----|--------------|
| Button Hintergrund | `#0f1829` | Wie Card |
| Button Border | `#1e3a5f` | Wie Panel-Border |
| Button Text | `#60a5fa` | Akzent-Blau |
| Button Hover | `#1a2a40` | Aufgehellt |

#### Karten-spezifische Farben

| Element | Hex | Beschreibung |
|---------|-----|--------------|
| Mähpfad (vorwärts) | `#22d3ee` | Cyan |
| Perimeter | `#2563eb` | Blau |
| Exclusion-Zonen | `#dc2626` | Rot gestrichelt |
| Zonen (Mähzone) | `#a855f7` | Purple |
| Roboter-Marker | `#22d3ee` | Cyan-Kreis mit Richtungspfeil |
| Dock-Marker | `#d97706` | Amber-Rechteck mit „D" |
| Karten-Raster | SVG/Canvas-Grid | Kein externer Kartendienst |

#### Status-Farben

| Status | Hex | Beschreibung |
|--------|-----|--------------|
| Mähend (Mow) | `#22d3ee` | Cyan |
| Fehler (Error) | `#ef4444` | Rot |
| Laden (Charge) | `#22c55e` | Grün |
| Warten/Idle | `#94a3b8` | Grau |
| GPS-Fix | `#22d3ee` | Cyan |
| GPS-Float | `#f59e0b` | Amber |
| Kein GPS | `#ef4444` | Rot |

### Layout-Regeln

1. **NOTAUS immer in der Topbar sichtbar** — nicht ausblendbar
2. **GPS: Rover + Base getrennt anzeigen** wenn beide verfügbar
3. **Kartenhintergrund:** SVG/Canvas-Raster-Grid — kein externer Kartendienst (kein Google Maps, kein OpenStreetMap)
4. **Kein Light-Mode** — das Design ist ausschließlich auf dunklem Hintergrund definiert
5. **Alle Farben mit `!important`** erzwingen — Tailwind-Purge und Widget-Frame-Overrides umgehen
