# Architectural Decisions — sunray-core

Last updated: 2026-03-24

---

## 2026-03-21 Decision: Flat HardwareInterface

**Choice:** One interface class with all hardware methods
**Reason:** The 8 sub-driver classes in Sunray all share the same SerialRobotDriver instance — the split is artificial and makes testing harder
**Rejected:** Keeping the RobotDriver/MotorDriver/BatteryDriver split — unnecessary complexity with no benefit

---

## 2026-03-21 Decision: BUG-07 stays in SerialRobotDriver

**Choice:** Left/Right PWM and encoder ticks intentionally swapped in SerialRobotDriver only
**Reason:** Physical wiring compensation on Alfred PCB — not a logic issue, not visible to Core
**Rejected:** Fixing in HardwareInterface or Core — would break Alfred hardware

---

## 2026-03-21 Decision: nearObstacle defaults false

**Choice:** `SensorData.nearObstacle = false` always for Alfred
**Reason:** Alfred AT-protocol has no sonar — interface must support optional sensors cleanly
**Rejected:** Removing nearObstacle from interface — Pico-Driver may support sonar in Phase 2

---

## 2026-03-21 Decision: No Singleton for Config

**Choice:** Config passed as `std::shared_ptr<Config>` via constructor
**Reason:** Testability — unit tests can inject a Config with test values without touching filesystem
**Rejected:** Global Config instance — makes unit testing difficult, hidden dependencies

---

## 2026-03-21 Decision: New repository sunray-core/

**Choice:** Separate repo E:\TRAE\sunray-core\ independent of existing Sunray repo
**Reason:** Alfred runs unmodified during the rewrite — no risk of breaking a working system
**Rejected:** Rewriting in-place in the Sunray repo — too risky, Alfred must stay operational

---

## 2026-03-21 Decision: AT-frame protocol unchanged (Phase 1)

**Choice:** Existing AT+M / AT+S / AT+V protocol kept as-is
**Reason:** Proven, working, Alfred STM32 firmware doesn't change in Phase 1
**Rejected:** New binary protocol — unnecessary risk, deferred to Phase 2 if needed

---

## 2026-03-21 Decision: Python Mission Service stays for now

**Choice:** Existing FastAPI/Python Mission Service continues to run independently
**Reason:** Core Rewrite is Phase 1 priority — Mission Service already works and can be migrated later
**Rejected:** Stopping Mission Service development — it runs Alfred today and provides value now

---

## 2026-03-22 Decision: Telemetrie-Format eingefroren

**Choice:** Das JSON-Telemetrie-Format des Unix-Socket-Protokolls (Mission Service ↔ Sunray-Core) bleibt byte-for-byte identisch — auch wenn der WebSocket-Server in A.6 auf C++ umgestellt wird
**Reason:** Das Frontend (`static/index.html`) und der Python MissionRunner parsen dieses Format direkt. Op-Namen (`"op":"Mow"`, `"Idle"`, `"Dock"`, `"Charge"`, `"Error"`, `"GpsWait"`, `"EscapeReverse"`) steuern den Mission State. Jede Abweichung bricht das laufende System.
**Must-Have-Felder:** `op`, `x`, `y`, `battery_v`, `gps_text`, `bumper_l`, `bumper_r`, `motor_err`
**Rejected:** Umbenennung von Feldern oder Op-Strings — kein Breaking Change ohne gleichzeitiges Frontend-Update

---

## 2026-03-22 Decision: No-Go Zones — Option C für Phase 1

**Choice:** No-Go-Zonen werden ausschließlich im Python-Pfadplaner aus dem Mähpfad herausgeschnitten (Shapely boolean difference). Sunray-Core kennt No-Go-Polygone nicht und prüft sie nicht zur Laufzeit.
**Reason:** Option A (neues JSON-Command) und Option B (Polygon-Check im LineTracker) erfordern A.5-Navigation als Voraussetzung. Phase 1 soll Alfred identisch zum heutigen Stand betreiben. Der geplante Pfad enthält bereits keine No-Go-Punkte.
**Rejected:** Option A + B für Phase 1 — zu viel Scope; wird in Phase 2 (nach A.5 Navigation) neu bewertet
**Risiko:** Wenn der Robot vom Pfad abweicht (Hindernis-Umfahrung), kann er in eine No-Go-Zone geraten. Akzeptabel für Phase 1.

---

## 2026-03-22 Decision: WebSocket-Server A.6 — Interface-Kompatibilität

**Choice:** Der C++ WebSocket-Server (A.6) implementiert exakt denselben Endpoint `/ws/telemetry`, dieselbe 10 Hz Cadence (100 ms Intervall) und dasselbe Keepalive-Format `{"type":"ping"}` bei Datentimeout
**Reason:** Das Frontend verbindet sich per `ws://host/ws/telemetry` ohne Konfiguration. Die Python-Seite (`api/ws.py`) und das Frontend (`index.html:connectWs()`) erwarten diesen Pfad und das ping-Format hart kodiert.
**Rejected:** Anderer Endpoint-Pfad oder anderes Keepalive-Format — würde Frontend-Änderung erfordern; kein Benefit

---

## 2026-03-22 Decision: WebUI Design — Dark Blue abgenommen

**Choice:** Farbschema "Dark Blue" für die gesamte WebUI verbindlich festgelegt
**Reason:** Abgenommenes Design-Mockup liegt als unveränderliche Referenz in `webui/design/dashboard_reference.html`. Alle künftigen Views folgen diesem Schema (Farben, Typografie, Layout-Prinzipien).
**Regeln (aus Referenz-Datei):**
- Alle Farben mit `!important` erzwingen (Widget-Frame überschreibt sonst)
- NOTAUS immer in der Topbar sichtbar
- GPS: Rover + Base Satelliten getrennt anzeigen
- Nav auf dunklem Streifen (`#060c17`) mit hellem Text (`#e2e8f0`)
- Kartenhintergrund: SVG/Canvas Raster-Grid — kein externer Kartendienst
- Cyan (`#22d3ee`) für Mähpfad, Blau (`#2563eb`) für Perimeter, Rot gestrichelt (`#dc2626`) für Exclusion-Zonen
- Roboter-Marker: Kreis mit Richtungspfeil; Dock-Marker: Amber (`#d97706`) Rechteck mit D
**Rejected:** Änderungen am Design ohne explizite Freigabe — Referenzdatei nicht anfassen
**Referenz:** `webui/design/dashboard_reference.html` — nicht verändern ohne explizite Freigabe

---

## 2026-03-24 Decision: Dokumentation in separate Dateien ausgelagert

**Choice:** `docs/ARCHITECTURE.md` (C++-Architektur) und `docs/WEBUI_ARCHITECTURE.md` (Vue-Architektur) als zentrale Referenz, nicht im CLAUDE.md
**Reason:** CLAUDE.md kompakt halten, Architektur-Details erfordern zu viel Platz — separate Docs für zukünftige Entwickler leichter zu durchsuchen
**Rejected:** Alles in CLAUDE.md — würde 2000+ Zeilen, unübersichtlich; oder inline-Kommentare — schwer zu finden

---

## 2026-03-24 Decision: MQTT Client und GpsDriver als optionale Feature

**Choice:** MqttClient und UbloxGpsDriver werden via Setter `Robot::setMqttClient()` und `Robot::setGpsDriver()` angehängt (nicht im Konstruktor)
**Reason:** Phase 1 fokussiert auf SerialRobotDriver + WebSocket; MQTT/GPS sind Add-ons — WebSocket bleibt primär für Mission Service; Testbarkeit: MockHardware braucht keine MqttClient-Dependencies
**Rejected:** In Konstruktor erzwingen — würde Boilerplate in Tests erhöhen, MQTT-Infrastruktur voraussetzen

---

## Template for new decisions:

```
## [date] Decision: <title>
**Choice:** <what was chosen>
**Reason:** <one sentence why>
**Rejected:** <alternative and why not>
```
