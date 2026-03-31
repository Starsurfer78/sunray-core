# Sunray-Core Analyse

**Datum:** 31. März 2026

---

## **SUNRAY-CORE COMPREHENSIVE ANALYSIS**

### **1) CODEBASE OVERVIEW**

**sunray-core** ist eine modernisierte, Linux-zentrierte Neufassung des Ardumower Sunray-Projekts, entwickelt als vollständiger Roboter-Controller für RTK-GPS-autonome Rasenmäher auf Raspberry Pi 4B. Der Codebase ist in einer sauberen Schichtenarchitektur organisiert mit klarer Trennung der Verantwortlichkeiten:

**Architektur-Schichten:**

| Schicht | Zweck | Schlüsselkomponenten |
|---------|-------|----------------------|
| **platform/** | POSIX/Linux-Abstraktionen | Serielle Ports, I2C, Port-Expander (PCA9555) |
| **hal/** | Hardware-Abstraktion & Treiber | `HardwareInterface`, `SerialRobotDriver`, `SimulationDriver`, GPS/IMU-Treiber |
| **core/** | Plattform-unabhängige Logik | `Robot`, Op-Zustandsmaschine, Navigation, WebSocket/MQTT, Speicher |
| **webui/** | Vue 3 + TypeScript Frontend | Dashboard, Karten/Missions-Editoren, Diagnose, Historie |
| **tests/** | Native & Frontend-Tests | Catch2 (C++) + Vitest (Vue), 175 Tests bestanden |

#### **1.2 Kontrollfluss-Architektur**

**Haupteinstiegspunkt:** [main.cpp](/mnt/LappiDaten/Projekte/sunray-core/main.cpp)
- Lädt Konfiguration aus JSON
- Instanziiert Hardware-Treiber (serial für echte Hardware, Simulation für Dev)
- Erstellt `Robot`-Instanz mit Dependency Injection
- Optional: GPS-Treiber, WebSocket-Server, MQTT-Client anhängen
- Startet 50 Hz (LOOP_PERIOD_MS = 20ms) Kontrollschleife

**Robot-Kontrollschleife (Robot::run()):**

[Robot::run()](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp#L100-L150) führt ~50 Iterationen pro Sekunde aus:

1. **tickHardware()** - Treiber-Tick (UART-Frames, Motor-PWM, Sensor-Lesen)
2. **tickSchedule()** - Wöchentlicher Zeitplan-Check (60s-Intervalle)
3. **tickObstacleCleanup()** - Alte virtuelle Hindernisse ablaufen lassen (1s-Intervalle)
4. **tickStateEstimation()** - EKF-Sensor-Fusion (Odometrie + GPS + IMU)
5. **assembleOpContext()** - Zustandssnapshot für Zustandsmaschine bauen
6. **tickSafetyGuards()** - Batterie, IMU, Perimeter-Checks
7. **tickStateMachine()** - OpManager-Tick (Übergänge prüfen + aktive Op ausführen)
8. **tickDiag()** - Integrierte Telemetrie-Aufzeichnung
9. **tickButtonControl()** - Physische Tasten-Druck-Sequenzen handhaben
10. **tickManualDrive()** - Override für Joystick/API-Befehle
11. **tickSafetyStop()** - Lift-Sensor, Bumper-Reaktion
12. **tickSessionTracking()** - Missions-Session-Metriken
13. **updateStatusLeds()** - Visuelles Feedback (LED_2 zeigt Op-Status)
14. **tickTelemetry()** - Zu WebSocket / MQTT pushen

#### **1.3 Kern-Runtime-Klassen**

**[Robot](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.h)** (Haupt-Orchestrator)
- 1 Instanz pro Prozess
- Besitzt Hardware-Treiber, Config, Logger, alle Subsysteme
- Thread-sicher `stop()` / `loop()` für graceful Shutdown
- Stellt Befehle bereit: `startMowing()`, `startDocking()`, `emergencyStop()`

**[OpManager](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.cpp)** (Zustandsmaschinen-Engine)
- 11 Op-Zustände (siehe §2.1 unten)
- Verwaltet Übergänge via `requestOp()` mit Prioritätsstufen
- Jede Op ist ein Objekt mit `begin()`, `run()`, `end()`, und Event-Handlern
- Beispiel Event-Handler: `onObstacle()`, `onGpsNoSignal()`, `onBatteryLow()`, etc.

**[StateEstimator](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.h)** (Sensor-Fusion)
- Extended Kalman Filter kombiniert:
  - Odometrie (Hall-Sensoren via ticks_per_revolution, Rad-Durchmesser)
  - GPS (RTK fixed/float von u-blox ZED-F9P)
  - IMU (Mpu6050 Heading, Roll, Pitch)
- Ausgabe: Kontinuierliche (x, y, Heading) Pose-Schätzung

**[Map](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)** (Waypoint-Management)
- Perimeter-Polygon + Ausschlusszonen
- Mow-Waypoints (mit reverse/slow-Flags für K-Turns)
- Dock-Ein-/Ausfahrt-Waypoints
- On-the-fly Hindernisliste mit Ablauf-Timern
- A* Pfadfindung auf lokaler Grid-Map

**[LineTracker](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.h)** (Pfad-Kontrolle)
- Stanley Heading-Controller für Pfad-Folgen
- Berechnet lateralen Fehler von Pose zu Waypoint-Linie
- Gibt lineare Geschwindigkeit + Winkelgeschwindigkeit zu Differential-Drive aus
- Feuert Events: `onTargetReached()`, `onNoFurtherWaypoints()`, `onKidnapped()`

#### **1.4 Daten-Persistenz & Historie**

**[EventRecorder](/mnt/LappiDaten/Projekte/sunray-core/core/storage/EventRecorder.h)** → **[HistoryDatabase](/mnt/LappiDaten/Projekte/sunray-core/core/storage/HistoryDatabase.h)** (SQLite-Backend)

Erfasst strukturierte Events:
- Zustandsmaschinen-Übergänge + Event-Gründe
- Batterie-Warnungen, GPS-Verlust, Hindernisse, Motor-Fehler, Lift-Triggers
- Session-Records: Start, Ende, Dauer, gemähte Distanz, Batterie-Delta
- Metadaten: Fix-Reliabilität (Sample-Varianz, Genauigkeit), Timestamps

API-Endpunkte:
- `/api/history/events`
- `/api/history/sessions`
- `/api/statistics/summary`

#### **1.5 WebUI-Stack**

**Framework:** Vue 3 + TypeScript + Vite + Tailwind CSS

**Seiten:**
- **Dashboard.vue** - Echtzeit-Status (Op-Name, Batterie, GPS, Odometrie), Steuerungen (Start/Stop/Dock), RTK-Punkt-Aufnahme
- **MapEditor.vue** - Perimeter/Ausschluss-Polygon-Bearbeitung, Zonen-Management, Dock-Pfad-Setup
- **Scheduler.vue** - Wöchentlicher Mähschicht-Plan mit Regenverzögerung
- **History.vue** - Event-Log mit Filtern (Zeit, Typ, Fehler, Phase)
- **Statistics.vue** - Session-Zusammenfassungen, Laufzeit-Metriken
- **Diagnostics.vue** - Hardware-Tests, IMU-Kalibrierung, Motor-Tests, Konfigurations-Editor
- **Simulator.vue** - Simulierte GPS/Sensor-Daten für Tests ohne Hardware injizieren

**Server:** Crow-basierter HTTP/WebSocket-Server (C++)
- Statische Datei-Bereitstellung (webui/dist)
- REST API: `/api/version`, `/api/config`, `/api/map`, `/api/map/geojson`, `/api/schedule`, `/api/sim/*`, `/api/diag/*`
- WebSocket: `/ws/telemetry` (Echtzeit-Status alle 50ms)
- Token-basierte Auth: `api_token` Config-Parameter

---

### **2) ROBOT BEHAVIOR ANALYSIS**

#### **2.1 Op State Machine (When/What/Why)**

Das autonome Verhalten des Roboters wird vollständig von der 11-Zustände Op-Maschine gesteuert. Alle Übergänge sind definiert in [OP_STATE_MACHINE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/OP_STATE_MACHINE.md):

```
IDLE ← → UNDOCK ← → NAVTOSTART ← → MOW
                                      ↓
                                    DOCK ↔ CHARGE
                                   ↗ ↑ ↖
                  WAITRAIN, GPSWAIT, ESCAPE, ERROR (recovery routes)
```

| Zustand | Dauer | Eintritts-Trigger | Austritts-Trigger | Was Roboter tut | Warum |
|---------|-------|-------------------|-------------------|-----------------|-------|
| **IDLE** | Bis Befehl | Power-on, Op stop, Fehler behoben | Operator startet, Zeitplan, Ladegerät erkannt | Motoren stoppen, auf Input warten | Sicherer Idle bei eingeschaltet; sofortiger Start bereit |
| **CHARGE** | Stunden | Ladegerät erkannt (während IDLE oder Docking) | Batterie voll ODER Operator-Befehl | Motoren stoppen, Position auf Dock halten | Kontakt halten + laden bis voll (erkennen via Spannungsanstieg + niedriger Strom) |
| **UNDOCK** | ~10s | Operator: start (während Laden) | Distanz erreicht + Heading OK | Rückwärts fahren @ 0.08 m/s, dann drehen zu Start-Heading | Ladeanschlüsse sicher lösen; ausrichten auf initialen Mähschicht-Kurs |
| **NAVTOSTART** | Variiert | UNDOCK fertig ODER Operator navtostart | Startpunkt erreicht ODER keine Route ODER GPS verloren | Dock-zu-Start-Waypoint-Pfad folgen (Stanley) | Nach Hause navigieren → Mähschicht-Zonen-Grenze | Deterministisch, wiederholbarer Eintritt in Mähschicht-Muster |
| **MOW** | Stunden | NAVTOSTART fertig ODER Operator start | Batterie niedrig ODER Zeitplan stop ODER keine Waypoints mehr ODER GPS verloren | Mähschicht-Waypoint-Muster folgen (Stanley), Klinge drehend (PWM 200) | Systematische Rasenabdeckung; Klinge stoppen beim Austritt |
| **DOCK** | ~5-10s | Mow: Batterie niedrig / GPS-Timeout / Regen / keine Punkte mehr; Operator: dock | Ladegerät erkannt (→ CHARGE) ODER GPS verloren (→ GPSWAIT) ODER max Retries | Dock-Eintritts-Pfad folgen (Stanley @ langsamer Geschwindigkeit 0.1 m/s) | Lade-Station mit präzisem Kontakt annähern; bis zu 3x wiederholen wenn verfehlt |
| **WAITRAIN** | ~1 Stunde | Regen-Sensor an (während MOW/NAVTOSTART) | Regen vorbei ODER Batterie niedrig ODER Timeout | Motoren stoppen, an aktueller Position warten ODER zu Dock bewegen | Klinge vor Verstopfung schützen; Mähen fortsetzen wenn Regen vorbei |
| **GPSWAIT** | ~10 min | GPS-Signal verloren (Entführungs-Erkennung, <3s RTK-Timeout) | GPS erholt (>3s fixed) ODER Fix-Timeout | Motoren stoppen, auf Signal warten; Rückkehr-Ziel aufzeichnen | Sicherheit: Kann nicht blind navigieren; zu vorherigem Zustand (MOW/DOCK/UNDOCK) zurückkehren sobald fixed |
| **ESCAPEREVERSE** | ~5s | Hindernis erkannt (Bumper/Lift/Grid) | Escape fertig ODER noch außerhalb Perimeter ODER Entführung | Rückwärts @ 0.08 m/s für 2s, pausieren, Vorwärts versuchen | Roboter von blockierendem Hindernis entfernen; wenn noch stecken → DOCK (sicherer Rückzug) |
| **ESCAPEFORWARD** | ~3s | (Selten, interne Erholung) | Vorwärts fertig | Vorwärts fahren @ 0.08 m/s | Alternative Escape wenn Rückwärts fehlgeschlagen (experimentell) |
| **ERROR** | Bis Erholung | Lift (Neigung >15°), Motor-Fehler, Lade-Fehler, max Retries in DOCK | Operator stop/dock/charge | Motoren stoppen, Fehlercode + Nachricht + Timestamp loggen | Verriegelter Sicherheitszustand; Operator muss eingreifen (manuell oder remote Befehl) |

#### **2.2 Safety & Fallback Logic**

**Batterie-Monitoring (tickSafetyGuards → checkBattery):**
- **Niedrig** (25.5V Standard): nächstes Dock ASAP
- **Kritisch** (18.9V Standard): Notfall-Dock (höchste Priorität)
- **Voll** (30V, beim Laden): Laden stoppen wenn Ampere fallen UND Spannungs-Slope < 2mV/Zyklus

**GPS-Resilienz (tickStateEstimation → StateEstimator::updateGps):**
- **RTK Fixed:** als primäre Pose verwenden
- **RTK Float:** akzeptieren, aber weniger vertrauensvoll
- **Kein Fix (>3s):** KidnapDetect-Check; wenn außer Pfad >1m → GpsWait triggern
- **Fix-Timeout (>10min):** sofortiges Dock erzwingen

**Hindernis-Reaktion (LineTracker::track → onObstacle):**
- Bumper-Hit ODER Grid-Map-Vorhersage → EscapeReverse
- Wenn noch außerhalb Perimeter nach Escape → Dock erzwingen (nicht wandern)
- Loop-Erkennung: >5 Hindernisse in 30s-Fenster → ERROR (in Ecke stecken)

**Lift-Sensor (Mpu6050 Beschleunigungsmesser + Config-Schwellwert):**
- Neigung >15° (üblicherweise manuelles Heben) → ERROR (Sicherheitsverriegelung)
- Persistenter ERROR bis Operator eingreift

**Motor-Fehler-Erkennung:**
- Aktuell von STM32 MCU über UART (Legacy Pi-seitige Stromerfassung deaktiviert aber Config behalten)
- Fehler → sofortiger ERROR, Mission stoppen

#### **2.3 Navigation Behavior**

**Pfad-Folgen (LineTracker.cpp):**

Stanley-Controller-Formel:
```
angular_velocity = Kp × heading_error + atan2( Ky × lateral_error, speed )
```

Config-Tuning (von [config.example.json](/mnt/LappiDaten/Projekte/sunray-core/config.example.json#L200)):
- `stanley_k_normal`: lateraler Fehler-Gain (normal mow: 0.1)
- `stanley_p_normal`: Heading-Fehler-Gain (normal mow: 1.1)
- `stanley_k_slow`: lateraler Gain (Docking: 0.1)
- `stanley_p_slow`: Heading-Gain (Docking: 1.1)
- `dock_linear_speed_ms`: Annäherungs-Geschwindigkeit (0.1 m/s = schleichen)
- `motor_set_speed_ms`: Mähschicht-Geschwindigkeit (0.3 m/s typisch)

**Waypoint-Vorrücken:**
- Wenn Roboter innerhalb `target_reached_tolerance_m` (0.1m) von Waypoint → zu nächstem vorrücken
- Keine Waypoints übrig → aktuellen Op verlassen (Übergang zu DOCK oder IDLE)
- Jeder Waypoint trägt optionale Flags:
  - **rev=true**: rückwärts zu diesem Punkt fahren (K-Turn-Manöver)
  - **slow=true**: für enge Kurven verlangsamen

**Replanning (on-the-fly Hindernisse):**
- Bei Bumper-Hit → virtuelles Hindernis hinzufügen (Kreis, 1.2m Durchmesser Standard)
- A* Grid-Map berechnet Dock/Mow-Route neu, neue Hindernisse vermeidend
- Hindernisse laufen nach 60s ab (Auto-Cleanup) oder bei manuellem Clear

#### **2.4 Schedule Integration**

**Wöchentlicher Mähschicht-Plan** ([Schedule.h](/mnt/LappiDaten/Projekte/sunray-core/core/Schedule.h)):
- 7-Tage-Tabelle von Start/End-Zeiten + Regenverzögerung
- Alle 60s von tickSchedule() geprüft
- Wenn Plan aktiv und Op erlaubt → feuert `onTimetableStartMowing()` (z.B., von IDLE → UNDOCK)
- Wenn Plan inaktiv wird → feuert `onTimetableStopMowing()` (z.B., MOW → DOCK)

**Beispiel Plan-Eintrag:**
```json
{
  "dow": 2,           // Dienstag (0=Sonntag)
  "startTime": "08:00",
  "endTime": "18:00",
  "rainDelay": 60,    // 60 min warten nach Regen bevor Fortsetzen
  "active": true
}
```

#### **2.5 User Button Sequences**

**Physische Tasten-Halt-Timings** ([ButtonActions.h](/mnt/LappiDaten/Projekte/sunray-core/core/ButtonActions.h)):

| Halt-Dauer | Op-Effekt | Grund |
|------------|-----------|-------|
| **1–2s** | STOP (aktueller Op → IDLE via opMgr Befehle) | Schnelle Deaktivierung |
| **5–6s** | DOCK (MOW → DOCK via Operator-Befehl) | Nach Hause zurück (z.B., Ende der Schicht) |
| **6–9s** | UNDOCK + MOW (von CHARGE/IDLE) | Mähen starten |
| **9+s** | SHUTDOWN (Robot::stop() → graceful Halt) | Sicheres Ausschalten (Watchdog-Neustart verhindern) |

LED + Buzzer-Sequenz verstärkt Halt-Dauer:
- 1s Piep bei 2s Marke → "stop verfügbar"
- 2 Pieps bei 5s Marke → "dock verfügbar"
- 3 Pieps bei 6s Marke → "mow verfügbar"
- 4 Pieps + längerer Ton bei 9s → "shutdown verfügbar"

---

### **3) MISSING FEATURES**

Verglichen mit typischen professionellen Smart-Rasenmähern (Husqvarna Automower, Worx Landroid) und den erklärten architektonischen Zielen des Projekts, sind die folgenden Features **dokumentiert als unvollständig oder nicht implementiert** (per [TODO.md](/mnt/LappiDaten/Projekte/sunray-core/TODO.md) und [TASKS.md](/mnt/LappiDaten/Projekte/sunray-core/TASKS.md)):

#### **3.1 PRIORITY 1: Real Hardware Validation** (Blocking)

**Status:** ⏸ Wartet auf Raspberry Pi Zugriff

| Feature | Aktueller Zustand | Warum fehlend | Business Impact |
|---------|-------------------|---------------|-----------------|
| **A.9-a: Pi 4B Compilation** | Noch nicht auf echtem Pi getestet | Build-Ziel nur auf Dev Linux verifiziert | Kann UART-Timing, GPIO-Stabilität, thermisches Verhalten nicht verifizieren |
| **A.9-b: Alfred compatibility** | Code portiert, nicht auf echter Hardware bewiesen | Motor-Kontroll-Latenz, Lade-Protokoll, Tasten-Reaktion unbekannt | Risiko: Reintroduzierte Bugs nur im Feld sichtbar |
| **A.9-c: Tests on Pi** | Nicht ausgeführt | 175 Tests bestehen auf Dev PC aber Pi Threading/Timing kann differieren | Kann Performance unter realer Last nicht zertifizieren |
| **Motor PID control** | Open-loop only (PWM → speed mapping) | PID-Loop für closed-loop Geschwindigkeits-Regulierung noch nicht implementiert | Inkonsistente Mähschicht-Abdeckung wenn Motor-Last variiert |
| **UART reliability** | Simuliert, nicht Feld-getestet | Serial/STM32 Handshake, Baud Sync, CRC, Watchdog in realen Bedingungen unbekannt | Potenzielle zufällige Disconnects im Feld |
| **IMU calibration** | Tools existieren, nicht auf Deployment validiert | Mpu6050 Offset/Scale nicht auf tatsächlicher Roboter-Plattform gemessen | Navigation Heading-Drift möglich im Feld |

#### **3.2 PRIORITY 2: User Experience & Onboarding** (High Impact, Low Urgency)

**Status:** ⏹ Geplant (C.16: "Benutzererlebnis / Nutzerführung")

| Feature | Aktueller Zustand | Warum fehlend | User Impact |
|---------|-------------------|---------------|-------------|
| **C.16-a: Pre-flight readiness check** | Partial (Batterie, GPS gezeigt) | Kein vereinheitlichter "kann starten" Indikator Dashboard | Operator muss aus mehreren Gaugen inferieren; kein klares "GO" Signal |
| **C.16-b: User-friendly Op names** | Interne Op-Namen gezeigt (z.B., "Mow", "Charge") | UI-Strings nicht lokalisiert oder mit Business-Terminologie vereinheitlicht | Nicht-technische User verwirrt von "NavToStart" vs "home" |
| **C.16-c: Action suggestions for blockers** | Nur Fehlercodes geloggt | Keine Remediation-Hints z.B. "GPS schwach → Rover zu offenem Himmel bewegen" | Operator rät, wie Situationen zu fixen |
| **C.16-d: Recovery state clarity** | Zustand gezeigt aber Grund in Logs vergraben | Kein vereinheitlichtes Recovery-Card (Error, GpsWait, WaitRain separat gruppiert) | Operator weiß nicht, ob wartend (behebbar) oder fehlgeschlagen (braucht Hilfe) |
| **C.16-e: Guided setup flow** | Keiner; manuelle Config erforderlich | Kein Onboarding-Wizard (WiFi, GPS, Map-Erstellung, Dock-Ausrichtung) | 15–30 min Erstmal-Setup; hohe Reibung für Kunden |
| **C.16-f: Map creation as assistant** | Map-Editor existiert aber erfordert manuelle Schritte | Kein Schritt-für-Schritt: erfassen → validieren → Dock-Ausrichtung → veröffentlichen | User muss Polygon-Regeln, Punkt-Dichte, Dock-Geometrie kennen |
| **C.16-g: Map validation before use** | Keine; invalide Maps verursachen Runtime-Fehler | Keine Pre-flight Checks: Perimeter geschlossen? Punkte dicht genug? Dock viable? | Schlechte Maps → Missions-Fehler mitten im Rasen; Kunden-Frustration |
| **C.16-h: Mission history with context** | Events geloggt, aber Rohdaten nicht narrativ | Keine menschenlesbare Missions-Zusammenfassung: "2 Stunden gemäht, 80% Abdeckung, durch Regen gestoppt, wird um 16:00 fortsetzen" | Support-Team muss Logs parsen; langsames Troubleshooting |
| **C.16-i: Primary vs diagnostic views** | Einzelnes Dashboard mischt Operator-Kontrollen + Roh-Telemetrie | Kein klares "einfacher Modus" (Operator-View) vs "Experten-Modus" (Debug-Panel) | Kognitive Überlastung für nicht-technische User |
| **C.16-j: Event timeline** | Historie gespeichert, nicht visualisiert | Kein chronologisches Event-Card (Mission gestartet → GPS fixed → Dock erreicht → Laden) | User muss durch Roh-Logs klicken; kein narrativer Fluss |

#### **3.3 PRIORITY 3: Multi-Zone Missions & Advanced Scheduling** (Medium Urgency)

**Status:** 🗓 Geplant (C.18: "Missions-Redesign") aber nicht gestartet

| Feature | Aktueller Zustand | Warum fehlend | Functional Gap |
|---------|-------------------|---------------|----------------|
| **C.18-a: Zone settings per mission** | Zonen existieren, aber keine per-Zone Overrides | Alle Zonen verwenden gleiche Geschwindigkeit, Stripe-Breite, Muster | Kann nicht "zähes hohes Gras" Zone mit langsamerer Geschwindigkeit vs "schneller Trim" Zone designieren |
| **C.18-b: Mission abstraction** | Map hat Zonen, kein Mission-Container | Kein Weg, "Morgen mähen Zonen 1+2" vs "Abend mähen Zonen 3+4" zu definieren | Operator muss manuell Map editieren oder separate Maps erstellen |
| **C.18-c: Mission persistence** | Keine | Keine `/api/missions` Endpunkte | Missions verloren bei Reboot; kein wiederkehrender Mission-Scheduling |
| **C.18-d: Full mission UI overhaul** | Zonen-Editor existiert aber limitiert | Geplantes Redesign: Zonen-Selektor, Settings-Panel, Pfad-Vorschau, Mission-Sidebar | UI fragmentiert; schlechte Entdeckung für Mission-Settings |
| **C.18-e: Live path preview** | Dashboard zeigt live Roboter-Pfad aber nicht geplante Pfade | Canvas-basierte Mähschicht-Muster-Visualisierung nicht implementiert | Operator kann Mähschicht-Strategie nicht vor Ausführung verifizieren |
| **C.18-f: Zone settings widget** | Keine | Geplant: Winkel-Slider, Stripe-Breite, Roundtrip/Spiral-Wahl, Edge-Mow-Toggle | Kann Mähschicht-Strategie nicht ohne Code-Edit anpassen |
| **C.18-g: Per-mission schedule** | Nur globaler Plan | Keine unabhängigen Pläne pro Mission | Kann nicht "Wochentag schneller Mähen" um 8am und "Wochenende voller Mähen" um 10am auf gleichem Roboter haben |
| **C.18-h: Dashboard mission indicator** | Nur Operator-Status | Geplant: "Nächste Mission: Nord-Rasen @ 14:00" mit Countdown + Schnell-Start | Kein Mission-Kontext auf Dashboard |

#### **3.4 PRIORITY 4: Energy Planning & Safety** (Long-term Reliability)

**Status:** 📋 Geplant (C.8-b), nicht implementiert

| Feature | Aktueller Zustand | Warum fehlend | Safety/Reliability Gap |
|---------|-------------------|---------------|-------------------------|
| **C.8-b: Energy budget calculation** | Keine | Kein Math-Modell für: gemähte Fläche → erwartete Energie → Batterie-Reserve | Kann nicht vorhersagen, ob Batterie für Mission + Return-to-Dock ausreicht |
| **Return-to-dock range** | Einfacher Batterie-Schwellwert (25.5V) | Keine Pfad-Distanz × Load-Modell-Schätzung | Kann weit von Zuhause docken wenn Batterie mitten im Mähen depletiert; in Ecke gestrandet |
| **Battery-aware zone sequencing** | Zonen in fester Reihenfolge gemäht | Könnte "high-value" Zonen priorisieren wenn niedrige Batterie | Bedeckt low-priority Edges zuletzt, verpasst Abdeckung wenn Batterie stirbt |

#### **3.5 PRIORITY 5: Platform Expansion**

**Status:** 🚫 Blocked (B.1–B.5: "Pico-Driver")

| Feature | Aktueller Zustand | Warum fehlend | Platform Gap |
|---------|-------------------|---------------|--------------|
| **B.1–B.5: Raspberry Pico motor controller** | Keine (Spec only) | Erfordert separates Pico-Firmware (SDK-Projekt) | Auf STM32/Alfred Hardware gesperrt; keine BLDC oder direkte PWM-Alternative |
| **Direct GPIO motor control** | Nicht verfügbar | Keine Abstraktionsschicht für alternative Motor-Treiber | Kann nicht günstige DC/BLDC + Standard Motor-Controller (ESC) verwenden |
| **Hall sensor integration on Pico** | Nicht implementiert | Pico-Odometrie würde von Firmware kommen, nicht Pi GPIO | Odometrie nicht portabel zu anderen Plattformen |

#### **3.6 Summary Table: Features vs Standard Automower**

| Capability | Sunray-Core | Husqvarna Automower | Worx Landroid | Gap |
|------------|-------------|---------------------|--------------|-----|
| Base navigation (RTK + IMU + odometry) | ✅ | ✅ | ✅ | ✅ At parity |
| Perimeter + exclusion zones | ✅ | ✅ | ✅ | ✅ At parity |
| Edge mowing | ❌ | ✅ | ✅ | Missing (planned for edge-mow flag) |
| Multi-zone with zone speed/pattern | ⚠️ (partial) | ✅ | ✅ | Partial (zones exist, no per-zone overrides) |
| Weekly schedule + rain delay | ✅ | ✅ | ✅ | ✅ At parity |
| Mobile/remote UI | ✅ (web; not native app) | ✅ (native) | ✅ (native) | Partial (web works, no native mobile) |
| Obstacle avoidance (bumper + grid map) | ✅ | ✅ | ✅ | ✅ At parity |
| GPS RTK over NTRIP | ✅ | ❌ | ✅ | ✅ Above parity |
| Blade oscillation + blade control | ✅ | ✅ | ✅ | ✅ At parity (via PWM 200) |
| Event/mission history + stats | ✅ | ✅ | ✅ | ✅ At parity |
| Battery management | ✅ | ✅ | ✅ | ✅ At parity (basic thresholds; no advanced modeling) |
| Setup wizard / first-time onboarding | ❌ | ✅ | ✅ | Missing |
| Energy budget / range estimation | ❌ | ✅ (implied) | ✅ (implied) | Missing |
| Locked-in blade cycling | ⚠️ (manual override) | ✅ | ✅ | Partial (works but not automatic) |
| Field diagnostics / support logs | ✅ | ✅ | ✅ | ✅ At parity |
| Multi-robot swarm coordination | ❌ | ❌ | ❌ | Missing (out of scope) |

---

## **CONCLUSION**

**Sunray-core is a well-architected, modernized RTK-GPS lawn mower controller that successfully implements core autonomous mowing logic and navigation.** The C++17/Vue 3 stack is clean, the state machine is comprehensive, and the 175-test suite validates functionality locally.

**The project is currently at a **0.8 maturity threshold**:**
- ✅ Architecture solid
- ✅ Core features present (navigation, zones, schedule, history, API)
- ⏳ **BLOCKING:** Real hardware validation pending (Pi/Alfred field tests)
- 🗓 User experience enhancements planned but not urgent (C.16)
- 📋 Long-term features deferred (energy planning, mission redesign, Pico driver)

**Next immediate steps (critical path):**
1. **Real Pi hardware build + motor test** (A.9) — unblocks field trials
2. **UART stability confirmation** — most likely failure point
3. **GPS/IMU tuning on actual robot** — field precision validation
4. **UX enhancements** (C.16) — post-hardware, before public release

Once A.9 passes, the project is realistically 3-4 months from production-ready, pending only user-experience polish and edge-case testing on real lawns.</content>
<parameter name="filePath">/mnt/LappiDaten/Projekte/sunray-core/Analyse.md