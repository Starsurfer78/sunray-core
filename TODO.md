# Sunray TODO-Liste

Generiert aus: Sunray_Rewrite_Konzept_v2.docx + Sunray_Mission_Service_Architektur.docx
Stand: März 2026

---

## A — sunray-core (C++ Rewrite) — Priorität 1

### A.1 Fundament ✅

- [x] Repo-Struktur, CMakeLists.txt, HardwareInterface.h
- [x] Config.h/cpp, Serial, I2C, PortExpander (PCA9555)

### A.2 SerialRobotDriver ✅

- [x] AT+M / AT+S / AT+V Protokoll, BUG-07/05, Battery-Fallback, Fan, Tests

### A.3 Robot-Klasse + DI ✅

- [x] Logger, Robot.h/cpp, main.cpp DI-Wiring, 21 Tests mit MockHardware

### A.3b SimulationDriver ✅

- [x] Kinematisches Modell (Differentialantrieb)
- [x] Bumper L/R + Lift manuell auslösbar
- [x] GPS-Qualität umschaltbar (Fix / Float / No-Fix)
- [x] Hindernisse als Polygon-Liste → Bumper auslösen
- [x] 22 Tests, --sim Flag in main.cpp

### A.4 Op State Machine ✅

- [x] Op.h/cpp — OpContext, OpManager, Priority-Guard, Transition-Mechanismus
- [x] IdleOp, MowOp, DockOp, ChargeOp, EscapeReverseOp, GpsWaitFixOp, ErrorOp
- [x] Alle Ops via DI (OpContext), Op-Dispatch in Robot::run()

### A.5 Navigation ✅

- [x] StateEstimator — Dead-Reckoning (GPS-Update Phase 2)
- [x] LineTracker — Stanley Controller
- [x] Map — Polygon, Waypoint-Navigation (A\* Phase 2)
- [x] MowOp + DockOp Navigation gefüllt

### A.6 WebSocket-Server ✅

- [x] Crow + Asio via FetchContent
- [x] WebSocketServer.h/cpp — Pimpl, 10 Hz Push, keepalive ping
- [x] Telemetrie-Format identisch zu mission_api.cpp
- [x] Commands: start / stop / dock / charge / setpos
- [x] REST /api/sim/\* (bumper/gps/lift → SimulationDriver)
- [x] REST /api/config GET+PUT (live Config-Änderung)
- [x] Static file serving (webui/dist)
- [x] REST /api/map GET/POST
- [x] 8 Tests

### A.7 Konfiguration ✅

- [x] RobotConstants.h (alle Magic Numbers)
- [x] config.example.json

### A.8 GPS-Treiber (ZED-F9P via USB) 🔲 Nächster Schritt

Referenz: `e:/TRAE/Sunray/sunray/src/ublox/ublox.cpp` (alter Code), Port: `/dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_GNSS_receiver-if00`

- [ ] `hal/GpsDriver/GpsDriver.h` — Interface: `GpsData` Struct (lat, lon, relPosN/E, solution, numSV, hAccuracy, dgpsAge, nmeaGGA)
- [ ] `hal/GpsDriver/UbloxGpsDriver.cpp` — Eigener Thread, UBX-State-Machine (Parser aus altem ublox.cpp portieren)
  - [ ] UBX-NAV-RELPOSNED → RTK-Solution (0=invalid/1=float/2=fixed), relPosN/E
  - [ ] UBX-NAV-HPPOSLLH → Lat/Lon (1e-7°), hAccuracy
  - [ ] UBX-NAV-VELNED → Groundspeed, Heading
  - [ ] UBX-RXM-RTCM → Korrekturen-Eingangsstatus
  - [ ] NMEA GGA → Roher String (→ WebSocket-Stream)
- [ ] Optionale F9P-Konfiguration beim Start (config-Key `gps_configure: true/false`) — UBX-CFG Pakete wie im alten Code
- [ ] `Robot::run()` — `StateEstimator::updateGps()` aufrufen → echte Koordinaten in Telemetrie
- [ ] `WebSocketServer` — NMEA-Zeilen als `{"type":"nmea","line":"..."}` pushen → LogPanel GPS-Tab
- [ ] `config.example.json` — `gps_port`, `gps_baud`, `gps_configure` Schlüssel ergänzen
- [ ] Tests: MockGpsDriver, GPS-Qualitätswechsel (Fix→Float→NoFix)

### A.9 Alfred Build-Test ⏸ Hardware nötig

- [ ] Kompilieren auf Raspberry Pi 4B
- [ ] Alfred fährt mit neuem Core identisch wie vorher
- [ ] Alle Unit Tests grün
- [ ] Kein Arduino-Include mehr im Core

---

## B — Pico-Driver (Phase 2 — nach A.9)

- [ ] `hal/PicoRobotDriver/PicoRobotDriver.h + .cpp`
- [ ] PWM-Ausgabe (1–20 kHz, 5V Amplitude) für BLDC-Controller
- [ ] Hall-Sensor Odometrie (Ha/Hb GPIO-Interrupt)
- [ ] `platform/INA226.h + INA226.cpp` — Strommessung I2C
- [ ] Pico-Firmware (separates Projekt, Pico SDK C++)

---

## C — WebUI — Priorität 2

### C.1 Einstellungen-Screen

- [x] Sektion 2 — Roboter-Konfiguration (Schnittbreite, Speed, Akku-Grenzwerte, GPS, Regen)
- [x] PUT /api/config Endpoint (Crow-Seite)
- [x] Sektion 3 — Diagnose (Motor-Tests, IMU-Kalibrierung, Log-Stream)

### C.2 GeoJSON Import/Export ✅

- [x] GET /api/map/geojson — Export als FeatureCollection (WGS84, download)
- [x] POST /api/map/geojson — Import, WGS84→lokal, origin-Ableitung, map.json reload
- [x] MapEditor: Import-Button, Export-Button, Origin-Zeile (lat/lon + Von GPS)

### C.2b Mission Service (Python — Phase 2)

- [ ] MissionRunner — Waypoint-Sending (AT+W Batches à 30 Punkte)
- [ ] Dynamisches Nachladen wenn Buffer leer

### C.3 WebUI (Vue 3) ✅ Grundstruktur fertig

- [x] Vue 3 + Vite + Tailwind + Vue Router (6 Views)
- [x] WebSocket Composable (useTelemetry) — Auto-reconnect
- [x] Dashboard — Telemetrie-Cards, Canvas-Karte, Control-Buttons
- [x] RobotMap — Canvas, Grid, Roboter-Icon, Pan/Zoom
- [x] MapEditor.vue — Polygon-Editor: Perimeter, No-Go, Dock
  - [x] 5 Werkzeuge, Snap-to-first-point, Doppelklick-Close, Zoom/Pan
  - [x] Fit-View, Laden/Speichern via /api/map
- [x] Simulator-View — Bumper/GPS/Lift Steuerung
- [x] Coverage-Overlay — WGS84→lokal, Leaflet MultiPolygon (grün α=0.35)
- [x] Zonen-Einstellungen Modal — Edit-Button, PUT zones/{id}
- [x] Pfad-Vorschau — Leaflet-Polyline Overlay
- [x] Hindernis-Platzierung auf Karte (MapEditor)
- [x] Dashboard Design-Referenz umsetzen (webui/design/dashboard_reference.html)

### C.4 Verlauf & Statistiken (neu)

- [ ] Verlauf-View — Session-History, gemähte Fläche %, Coverage-Overlay
- [ ] Statistiken-View — Gesamtstunden, km, Akku-Zyklen, Trends

### C.4b Mähzonen (mehrere Zonen im Perimeter) 🔲 Konzept fehlt noch

- [ ] MapEditor: Zone-Werkzeug — Polygon innerhalb des Perimeters zeichnen
- [ ] MapData: `zones: { id, name, polygon: Pt[], settings: ZoneSettings }[]`
- [ ] Zonen-Einstellungen pro Zone: Schnittbreite, Fahrgeschwindigkeit, Muster (Streifen/Spirale)
- [ ] Reihenfolge der Zonen (Drag-Drop oder Nummerierung)
- [ ] C++: Map.h zones[] → Robot priorisiert Zonen bei MowOp

### C.4c Mähbahnen-Berechnung 🔲 Konzept fehlt noch

- [ ] Konzept: Bahnberechnungs-Algorithmus (Streifen-Rotation, Startpunkt, Überlappung)
- [ ] Einstellungen: Bahnbreite, Winkel, Überlappung %, Startseite
- [ ] Kantenmähen: erste Runde am Perimeter / No-Go-Rand, konfigurierbar (Ja/Nein, Anzahl Runden)
- [ ] Vorschau der berechneten Bahnen im MapEditor (read-only overlay)
- [ ] Pfad-Export an Robot (AT+W Batches)

### C.5 Später (Phase C)

- [ ] Energie-Budget + Rückkehr-Berechnung
- [ ] Dynamisches Replanning bei Hindernissen
- [ ] WebUI auf C++ WebSocket-Server umstellen (nach A.8)

---

## D — Offene Fragen

- [ ] Q1: StateEstimator Fehler-Propagation (globale Flag?)
- [ ] Q3: GPS no-motion Schwellenwert 0.05m — ausreichend für RTK-Float?
- [ ] Q5: missionAPI.run() Unix-Socket wirklich non-blocking?
- [ ] Q7: Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert?

---

## Legende

- [x] Erledigt
- [ ] Offen
- Priorität: A → B → C → D
