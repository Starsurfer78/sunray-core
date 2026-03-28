# Analyse: Sunray Mission Service + CaSSAndRA

Erstellt: 2026-03-22 via Subagent-Analyse der Codebasis
Quellen: `alfred/mission/` (52 Dateien), `sunray/mission_api.cpp/h`, `alfred/mission/static/index.html`

---

## 1. Architektur-Überblick

### Komponenten und ihre Rollen

```
┌─────────────────────────────────────────────────────────────────┐
│                    Browser / PWA Frontend                        │
│            (Leaflet.js, Alpine.js, Tailwind CSS)                │
└──────────────────────────┬──────────────────────────────────────┘
                           │ HTTP + WebSocket
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│           Mission Service (FastAPI Python on Pi)                │
│                    :8080                                         │
├─────────────────────────────────────────────────────────────────┤
│  API Layer (FastAPI routers)                                    │
│  ├─ /api/maps/* (CRUD, activate, export GeoJSON)               │
│  ├─ /api/maps/{id}/zones/* (CRUD, path preview)                │
│  ├─ /api/mission/* (start, stop, dock, pause, resume, status)  │
│  ├─ /api/mission/coverage/* (query, reset mowed area)          │
│  ├─ /api/schedules/* (timetable CRUD, next-runs)               │
│  └─ /ws/telemetry (WebSocket: 10 Hz robot state broadcast)     │
├─────────────────────────────────────────────────────────────────┤
│  Core Services (background asyncio tasks)                       │
│  ├─ SunrayClient (Unix socket ↔ Sunray C++)                    │
│  ├─ MissionRunner (state machine: IDLE/RUNNING/PAUSED/...)     │
│  ├─ CoverageTracker (accumulate mowed polygons)                │
│  ├─ PathPlanner (wrapper: C++ coverage_path_planner or Python) │
│  └─ Scheduler (APScheduler: time-window driven starts/stops)   │
├─────────────────────────────────────────────────────────────────┤
│  Data Layer (SQLAlchemy ORM async)                              │
│  └─ SQLite: maps, zones, schedules, time_windows, coverage     │
└──────────────────────────┬──────────────────────────────────────┘
                           │ /run/sunray/mission.sock
                           │ (Unix domain socket, newline-delimited JSON)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│         Sunray C++ Firmware (MissionAPI)                        │
│              robot.cpp: missionAPI.run()                        │
├─────────────────────────────────────────────────────────────────┤
│  ← Commands (setpos, start, stop, dock, charge)                │
│  → Telemetry (~10 Hz): state, position, battery, GPS, sensors  │
└─────────────────────────────────────────────────────────────────┘
```

### Kommunikationswege

| Weg | Protokoll | Details |
|-----|-----------|---------|
| Browser ↔ Mission Service | HTTP REST + WebSocket | Fetch API, Leaflet WS |
| Mission Service ↔ Sunray C++ | Unix Domain Socket | `/run/sunray/mission.sock`, newline-delimited JSON |
| Coverage Tracking | Intern (pub-sub) | TelemetryHub → CoverageTracker |

---

## 2. API-Endpunkte (vollständig)

### Health & System

| Methode | Pfad | Response |
|---------|------|----------|
| GET | `/health` | `{"status":"ok","sunray_connected":bool,"mission_state":"IDLE|RUNNING|…"}` |

### Maps CRUD

| Methode | Pfad | Zweck |
|---------|------|-------|
| GET | `/api/maps` | Alle Maps auflisten |
| POST | `/api/maps` | Map anlegen |
| GET | `/api/maps/{map_id}` | Map-Details |
| PUT | `/api/maps/{map_id}` | Map aktualisieren |
| DELETE | `/api/maps/{map_id}` | Map löschen |
| POST | `/api/maps/{map_id}/activate` | Als aktiv setzen |
| GET | `/api/maps/{map_id}/export` | GeoJSON FeatureCollection exportieren |

### Zones CRUD

| Methode | Pfad | Zweck |
|---------|------|-------|
| GET | `/api/maps/{map_id}/zones` | Zonen auflisten |
| POST | `/api/maps/{map_id}/zones` | Zone anlegen |
| PUT | `/api/maps/{map_id}/zones/{zone_id}` | Zone aktualisieren |
| DELETE | `/api/maps/{map_id}/zones/{zone_id}` | Zone löschen |
| POST | `/api/maps/{map_id}/zones/{zone_id}/preview` | Pfad-Vorschau berechnen → `{path (GeoJSON LineString), waypoints, duration_min}` |

**Zone-Typen:** `"mow"` (Mähfläche), `"nogo"` (Ausschluss), `"slow"` (reduzierte Geschwindigkeit)
**Muster:** `"lines"`, `"squares"`, `"spiral"`, `"rings"`

### Mission Control

| Methode | Pfad | Zweck |
|---------|------|-------|
| POST | `/api/mission/start` | Mission starten `{map_id, zone_ids:[]}` |
| POST | `/api/mission/stop` | Mission stoppen |
| POST | `/api/mission/pause` | Pause |
| POST | `/api/mission/resume` | Weiterführen |
| POST | `/api/mission/dock` | Zur Ladestation |
| GET | `/api/mission/status` | Aktueller State |
| GET | `/api/mission/telemetry` | Letzter Telemetrie-Snapshot (Poll-Fallback) |

### Coverage

| Methode | Pfad | Zweck |
|---------|------|-------|
| GET | `/api/mission/coverage` | Alle Zonen-Coverage |
| GET | `/api/mission/coverage/{zone_id}` | Eine Zone |
| DELETE | `/api/mission/coverage/{zone_id}` | Coverage zurücksetzen |

### Schedules

| Methode | Pfad | Zweck |
|---------|------|-------|
| GET | `/api/schedules` | Alle Zeitpläne |
| POST | `/api/schedules` | Zeitplan anlegen |
| PUT | `/api/schedules/{id}` | Zeitplan aktualisieren |
| DELETE | `/api/schedules/{id}` | Zeitplan löschen |
| PUT | `/api/schedules/{id}/enable` | Ein-/ausschalten `{enabled:bool}` |
| GET | `/api/schedules/next` | Nächste geplante Starts (max 8) |

### WebSocket

| Pfad | Format |
|------|--------|
| `/ws/telemetry` | `{"type":"state", "op":"Mow", "x":12.3, …}` oder `{"type":"ping"}` bei Timeout |

---

## 3. Datenmodell (SQLAlchemy → SQLite)

**Datenbankpfad:** `sqlite+aiosqlite:////var/lib/sunray/mission.db`

### Tabelle `maps`

```
id            String PK
name          String
description   Text
created_at    DateTime
updated_at    DateTime
is_active     Boolean (default False)
origin_lon    Float nullable  ← WGS84-Koordinate für AT+P-Befehl
origin_lat    Float nullable
perimeter     Text  ← GeoJSON Polygon (lokale Meter)
dock_point    Text  ← GeoJSON Point
dock_path     Text  ← GeoJSON LineString
crs           String (default "local")
```

Beziehungen: `zones` (1:N, cascade), `schedules` (1:N, cascade)
Dateiref: `models/map.py:13-36`

### Tabelle `zones`

```
id            String PK
map_id        FK → maps.id
name          String
type          String  ← 'mow' | 'nogo' | 'slow'
geometry      Text    ← GeoJSON Polygon (lokale Meter)
priority      Integer (default 0)  ← höher = zuerst verarbeitet
pattern       String  (default "lines")
strip_width   Float   (default 0.20 m)
angle_deg     Float   (default 0.0°)
border_passes Integer (default 1)
speed         Float   (default 0.08 m/s)
mow_edge_first Boolean (default True)
```

Dateiref: `models/map.py:39-57`

### Tabelle `schedules`

```
id              String PK
name            String
map_id          FK → maps.id
enabled         Boolean
zone_ids        Text  ← JSON Array
rain_delay_min  Integer (default 60)
min_battery_pct Float   (default 20.0)
min_gps_sol     String  (default "SOL_FIXED")  ← SOL_FIXED | SOL_FLOAT | SOL_NONE
```

### Tabelle `time_windows`

```
id                   String PK
schedule_id          FK → schedules.id
days                 Text ← JSON Array [1..7] = Mo-So
start_time           String "HH:MM"
end_time             String "HH:MM"
resume_after_rain    Boolean
resume_after_battery Boolean
```

**Cron-Mapping:** Model-Tage [1-7 = Mo-So] → APScheduler day_of_week [0-6]
Dateiref: `models/map.py:76-87`, `core/scheduler.py:55`

### Tabellen `coverage_sessions` und `coverage_current`

```
coverage_sessions:
  id, map_id, zone_id, started_at, ended_at, end_reason,
  mowed_pct, coverage_geojson (GeoJSON MultiPolygon)

coverage_current (persistenter State, überlebt Neustart):
  zone_id PK, last_x, last_y, waypoint_idx, mowed_area (GeoJSON), updated_at
```

Dateiref: `models/coverage.py:13-36`

---

## 4. Pfadplanung (coverage_path_planner)

### Zwei-Ebenen-Ansatz

**1. C++ Backend (wenn installiert):**
```python
planner = CoveragePathPlanner()
planner.setWorkingArea(coords, holes)   # Polygon-Exterior + Löcher (WGS84)
planner.setStripWidth(strip_w)          # Bahnenbreite in Metern
planner.setAngle(math.radians(angle_deg))
planner.setPattern("boustrophedon" | "boustrophedon_squares" | "spiral" | "rings")
planner.plan()
waypoints = planner.getWaypoints()      # [(lon, lat), …]
```
Dateiref: `core/path_planner.py:90-127`

**2. Python-Fallback (Shapely-basiert):**
- Boustrophedon in WGS84-Grad
- Polygon rotieren → parallele Linien generieren → clipping → Richtung alternieren
- Nur für Vorschau; Produktiv-Einsatz erfordert C++ Bibliothek
Dateiref: `core/path_planner.py:132-188`

### No-Go Zone Handling

```python
zone_poly = shape(zone_geojson)
for ng in nogo_geojsons:
    zone_poly = zone_poly.difference(shape(ng))  # Shapely Boolean
```

No-Go Zonen werden nur bei der Pfadberechnung ausgeschnitten. **Sunray-Core kennt sie nicht.**

### Entry Point

```python
async def compute_path(zone_geojson, nogo_geojsons, settings) → dict
# Rückgabe: {type:"LineString", coordinates:[[lon,lat],…]} oder {error:"msg"}
```

**Dauerschätzung:** `length_m / speed_ms / 60` → Minuten

---

## 5. AT-Protokoll-Integration

### WICHTIG: CaSSAndRA-Protokoll wird NICHT verwendet

Der Mission Service nutzt **kein** AT+W, AT+N, AT+X (CaSSAndRA-Befehle).
Stattdessen: **neues JSON MissionAPI-Protokoll** über Unix-Socket.

### Commands Mission Service → Sunray C++

| Befehl | JSON | Effekt in Sunray |
|--------|------|-----------------|
| start | `{"cmd":"start"}` | `changeOperationTypeByOperator(OP_MOW)` |
| stop | `{"cmd":"stop"}` | `changeOperationTypeByOperator(OP_IDLE)` |
| dock | `{"cmd":"dock"}` | `changeOperationTypeByOperator(OP_DOCK)` |
| charge | `{"cmd":"charge"}` | `changeOperationTypeByOperator(OP_CHARGE)` |
| setpos | `{"cmd":"setpos","lon":8.1234,"lat":48.5}` | `absolutePosSource=true`, aktiviert AT+P,1 Modus |

Dateiref: `sunray/mission_api.cpp:195-225`

### Telemetrie Sunray → Mission Service (10 Hz)

```json
{
  "type": "state",
  "op": "Mow",
  "x": 12.3,
  "y": 5.6,
  "heading": 1.57,
  "battery_v": 23.1,
  "charge_v": 0.0,
  "gps_sol": 3,
  "gps_text": "RTK-Fix",
  "gps_lat": 48.5001,
  "gps_lon": 8.1235,
  "bumper_l": false,
  "bumper_r": false,
  "motor_err": false,
  "uptime_s": 3600
}
```

Dateiref: `sunray/mission_api.cpp:254-274`

### Timing

| Parameter | Wert |
|-----------|------|
| Telemetrie-Rate | 100 ms (10 Hz), hardcoded `mission_api.cpp:44` |
| Reconnect-Delay (Python) | 3 Sekunden |
| Socket-Buffer | 256 Bytes read/iter, max 2048 Bytes Zeilenpuffer |

---

## 6. Frontend

### Technologien

| Technologie | Version | Zweck |
|-------------|---------|-------|
| Leaflet.js | 1.9.4 | Interaktive Karte, Roboter-Position |
| Leaflet.draw | 1.0.4 | Polygon-Zeichenwerkzeuge |
| Alpine.js | 3.x | Reaktives UI (kein Build-Step) |
| Tailwind CSS | – | Styling |
| Fetch API | nativ | REST-Calls |
| WebSocket | nativ | Live-Telemetrie |

### Seiten / Tabs

| Tab | Zeilen in index.html | Funktion |
|-----|---------------------|----------|
| Dashboard | 71–147 | Live Robot-State, Mission-Steuerung, Coverage-Display |
| Karten-Editor | 149–301 | Map/Zonen zeichnen, Robot-Assisted Capture |
| Zeitpläne | 303–368 | Schedule CRUD, nächste Starts |
| Einstellungen | 370–414 | System-Info, API-Docs-Links |

### PWA / Offline

- `manifest.json`: Standalone-Modus, App-Name, Theme-Color
- `sw.js`: Caching-Strategie (minimal implementiert)
- Offline-Banner: index.html:39 — "Kein Netzwerk — App läuft im Offline-Modus"
- Vendor-Bundles: Leaflet, Alpine, Tailwind lokal gecacht + CDN-Fallback

**Datei:** `alfred/mission/static/index.html` (1247 Zeilen)

---

## 7. Implementierungsstatus

### ✅ Vollständig implementiert

| Feature | Dateien |
|---------|---------|
| Maps CRUD | `api/maps.py` |
| Zones CRUD | `api/zones.py` |
| Pfadplaner-Wrapper (C++ + Python-Fallback) | `core/path_planner.py` |
| Mission Control (start/stop/dock/pause/resume) | `api/mission_control.py`, `core/mission_runner.py` |
| MissionAPI Socket | `sunray/mission_api.cpp/h` |
| TelemetryHub (pub-sub) | `core/sunray_client.py` |
| Coverage-Tracking (local→WGS84, Shapely, DB) | `core/coverage_tracker.py` |
| Zeitpläne + Timetable | `api/schedules.py`, `core/scheduler.py` |
| Batterie/GPS-Prüfung vor Start | `core/scheduler.py:80-100` |
| Frontend Dashboard, Editor, Schedules | `static/index.html` |
| WebSocket Telemetrie | `api/ws.py` |
| Datenbank (async ORM, alle Tabellen) | `database.py`, `models/` |

### ⚠️ Fehlend / Stub / Unvollständig

| Feature | Problem | Dateiref |
|---------|---------|----------|
| Coverage Session History | Modell vorhanden, keine API-Endpoints | `models/coverage.py:13-24` |
| No-Go Zone Enforcement | Planer schneidet aus, Sunray-Core kennt sie nicht | `core/path_planner.py:69-73` |
| Slow Zones | Typ definiert, weder Planer noch Firmware nutzt ihn | `models/map.py:45` |
| Border Passes | Parameter definiert, Planer ignoriert ihn | `models/map.py:53` |
| Mow Edge First | Parameter definiert, nicht umgesetzt | `models/map.py:55` |
| Waypoint-Index-Tracking | `waypoint_idx` in DB, wird nicht aktualisiert | `models/coverage.py:34` |
| Service Worker | Minimal-Implementierung, kein API-Cache | `static/sw.js` |
| Fehlerbehandlung Frontend | Nur Toast, keine Recovery | `static/index.html:687+` |
| Sunray-Disconnect UI | Auto-Reconnect nach 3s, aber keine Nutzerbenachrichtigung | `core/sunray_client.py:69-79` |
| Resume After Rain/Battery | DB-Felder vorhanden, Sunray-Core fehlt Auto-Resume | `models/map.py:84-85` |

---

## 8. Kritische Befunde für den Sunray-Core Rewrite

### Telemetrie-Format (muss exakt so bleiben)

```json
{
  "type": "state",
  "op": "Mow|Idle|Dock|Charge|Error|GpsWait|Escape",
  "x": 12.34,
  "y": 56.78,
  "heading": 1.5708,
  "battery_v": 23.5,
  "charge_v": 0.0,
  "gps_sol": 4,
  "gps_text": "RTK-Fix|RTK-Float|SPS|No-Fix",
  "gps_lat": 48.50012345,
  "gps_lon": 8.12345678,
  "bumper_l": false,
  "bumper_r": false,
  "motor_err": false,
  "uptime_s": 3600
}
```

**Must-Have-Felder (Mission Service nutzt diese aktiv):**

| Feld | Genutzt von |
|------|------------|
| `op` | MissionRunner state transitions |
| `x`, `y` | CoverageTracker (lokale Meter relativ zu Origin) |
| `battery_v` | Scheduler: min_battery_pct-Prüfung |
| `gps_text` | Scheduler: min_gps_sol-Prüfung |
| `bumper_l`, `bumper_r` | Dashboard Sensor-Display |
| `motor_err` | Error-Erkennung |

### Op-Namen: Exakte Strings

```
"op":"Idle"    → mission.state = IDLE
"op":"Mow"     → mission.state = RUNNING
"op":"Dock"    → mission.state = DOCKING
"op":"Charge"  → mission.state = CHARGING
"op":"Error"   → mission.state = ERROR
```

### Koordinatensystem: Handshake via setpos

1. Map origin (WGS84) wird in DB gespeichert (`origin_lon`, `origin_lat`)
2. Bei Missionsstart sendet Mission Service: `{"cmd":"setpos","lon":8.1234,"lat":48.5}`
3. Sunray setzt `absolutePosSource=true` → AT+P,1 Modus aktiv
4. Robot meldet lokale (x, y) Meter relativ zum Origin
5. CoverageTracker konvertiert zurück zu WGS84: `lon = origin_lon + x / 111111`

**Kritisch:** Wenn sich das Koordinatensystem mid-Mission ändert, ist die Coverage-Datenbank korrupt.

### No-Go Zones: Lücke in der Architektur

- **Aktuell:** Pfadplaner schneidet No-Go-Zonen aus dem Pfad heraus
- **Problem:** Sunray-Core kennt No-Go-Zonen nicht → kann nicht eigenständig ausweichen
- **Optionen:**
  - A: No-Go-Zonen als JSON-Command an Sunray senden (neues Protokoll)
  - B: No-Go-Polygone in LineTracker einbauen (Sonst-Check)
  - C: Nur dem geplanten Pfad vertrauen (aktueller Stand)

### WebSocket für neuen C++ Server (A.6)

Wenn der WebSocket-Server von Python auf C++ umgestellt wird:
- Gleiches JSON-Format beibehalten (kein Breaking Change im Frontend)
- Endpoint-Pfad `/ws/telemetry` beibehalten
- 10 Hz Cadence (100 ms interval) beibehalten
- Keepalive `{"type":"ping"}` bei Datentimeout senden

---

## 9. Dateiverzeichnis (alfred/mission/)

```
alfred/mission/
├── main.py                    FastAPI App-Entry, Lifespan-Handler
├── config.py                  Pfade, Defaults (SOCKET_PATH, DB_URL, etc.)
├── database.py                SQLAlchemy async setup
├── requirements.txt           Python-Dependencies
├── mission.service            systemd Unit-File
├── install_assets.sh          Frontend-Asset-Downloader
│
├── api/
│   ├── maps.py                /api/maps/* CRUD + GeoJSON-Export
│   ├── zones.py               /api/maps/{}/zones/* CRUD + preview
│   ├── mission_control.py     /api/mission/* start/stop/dock/status
│   ├── coverage.py            /api/mission/coverage/* query/reset
│   ├── schedules.py           /api/schedules/* CRUD
│   └── ws.py                  /ws/telemetry WebSocket
│
├── core/
│   ├── sunray_client.py       Unix-Socket-Client, TelemetryHub (pub-sub)
│   ├── mission_runner.py      Mission State Machine
│   ├── coverage_tracker.py    Gemähte Fläche akkumulieren (local→WGS84)
│   ├── path_planner.py        C++/Python Pfadplaner-Wrapper
│   └── scheduler.py           APScheduler + Zeitfenster-Logik
│
├── models/
│   ├── map.py                 Map, Zone, Schedule, TimeWindow ORM
│   └── coverage.py            CoverageSession, CoverageCurrent ORM
│
└── static/
    ├── index.html             Single-Page App (1247 Zeilen)
    ├── sw.js                  Service Worker (minimal)
    ├── manifest.json          PWA Manifest
    └── vendor/                Offline Leaflet, Alpine, Tailwind
```
