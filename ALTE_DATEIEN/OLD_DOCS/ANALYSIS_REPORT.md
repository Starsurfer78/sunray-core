# ANALYSIS_REPORT.md — sunray-core Projektzustand

Erstellt: 2026-03-23
Basis: CLAUDE.md, .memory/module_index.md, .memory/decisions.md,
       TODO.md, docs/BUG_REPORT.md, Code-Analyse aller 5 Kerndateien
       + Op-State-Machine, StateEstimator, Map, GpsDriver

---

## 1. ARCHITEKTUR-KONSISTENZ

**Status:** gut — mit zwei gezielten Ausnahmen

**Befund:**

### HardwareInterface-Vertrag
Eingehalten. `core/` enthält keinerlei `#include <Arduino.h>`, keinen
direkten I2C/UART/GPIO-Zugriff. Alle Sensor- und Aktuator-Operationen
laufen ausschließlich über die 13 Interface-Methoden. Die Entscheidung,
BUG-07 (PWM/Encoder-Swap) im Treiber zu kapseln, ist korrekt umgesetzt —
`HardwareInterface` bleibt sauber.

### Dependency Injection
Konsequent durchgezogen: `Robot`, `OpManager`, `StateEstimator`, `Map`,
`LineTracker`, `WebSocketServer`, `UbloxGpsDriver` — alle DI via
Constructor oder Setter. Keine versteckten `getInstance()` Aufrufe.

### Globale Objekte
Einzige akzeptierte Ausnahme: `g_robot` (Signal-Handler-Pointer) in
`main.cpp` — dokumentiert und bewusst. Keine weiteren globalen
Objekte gefunden.

### Inkonsistenz #1 — `activeOp_` Null-Guard
`OpManager::tick()` (Op.cpp:116) enthält einen Null-Guard:
`if (activeOp_ == nullptr) activeOp_ = idle_.get()`.
Dieser Guard impliziert, dass die Entwickler `nullptr` für möglich halten.
`Robot::checkBattery()` ruft `opMgr_.activeOp()->onBattery...()` direkt
auf — ohne Guard, und es wird **vor** `opMgr_.tick()` aufgerufen (Step 6
vs. Step 7). Da der Konstruktor `activeOp_ = idle_.get()` setzt, ist das
Risiko gering, aber die Inkonsistenz ist ein latentes Problem (→ BUG-001).

### Inkonsistenz #2 — `ctx.hw.setMowMotor()` in GpsWaitFixOp
`GpsWaitFixOp::begin()` ruft `ctx.hw.setMowMotor(0, 0, 0)` auf —
eine Methode, die in `HardwareInterface` nicht existiert (`setMotorPwm`
ist korrekt). Das ist entweder ein Compile-Fehler der unentdeckt blieb
(wäre mit echtem Build sichtbar) oder ein Tipp-Fehler.

**Empfehlung:**
- Null-Guard in `checkBattery()` ergänzen (2 Zeilen, → BUG-001-Fix)
- `setMowMotor` → `setMotorPwm` in GpsWaitFixOp.cpp korrigieren
- Ansonsten keine Architektur-Änderungen nötig

---

## 2. TESTABDECKUNG

**Status:** akzeptabel für die Infrastruktur — problematisch für Navigation und WebUI

**Befund:**

### Was getestet ist (≈ 87 Unit-Tests)

| Modul                  | Tests | Tiefe                                      |
|------------------------|-------|--------------------------------------------|
| Config                 | 8     | gut — Fehlerbehandlung, Typen, Save/Reload |
| Serial (POSIX)         | 4     | gut — Fehlerfall, Move-Semantik            |
| I2C                    | 4 SW  | gut (3 HW-Tests skip per default)          |
| PortExpander           | mock  | oberflächlich                              |
| SerialRobotDriver      | mock  | AT-Protokoll, BUG-07/05 Regression        |
| Robot                  | 21    | gut — DI, Lifecycle, Safety-Stop, Battery |
| SimulationDriver       | 22    | sehr gut — Kinematik, Sensoren, Threads    |
| WebSocketServer        | 8     | Telemetrie-Format, API-Surface             |
| GpsDriver (Mock)       | 9     | Daten-Structs, Qualitäts-Transitionen     |
| MqttClient             | 6     | Connection-Lifecycle                       |

### Was NICHT getestet ist — kritische Lücken

**Navigation (0 Tests!):**
- `StateEstimator` — kein Test für Odometrie-Integration, GPS-Update,
  Sanity-Guard (>0.5 m/frame), Heading-Normalisierung, ±10 km Reset
- `LineTracker` — Stanley-Controller-Logik komplett ungetestet. Kein Test
  für `onTargetReached`, `onKidnapped`, Winkelberechnung bei
  Rückwärtsfahrt
- `Map` — kein Test für `isInsideAllowedArea()`, `startMowing()`,
  `nextPoint()`, JSON-Load mit Zones/MowPoints

**Op State Machine (0 direkter Op-Tests!):**
- Keine Tests für einzelne Ops (MowOp, DockOp, ChargeOp, EscapeReverseOp)
- `GpsWaitFixOp` 2-Minuten-Timeout: ungetestet
- `checkBattery()` + Op-Event-Routing: ungetestet
- Prioritäts-Mechanismus in `OpManager::setPending()`: implizit in
  Robot-Tests, aber nicht direkt

**WebUI-Algorithmen (0 Tests!):**
- `useMowPath.ts / computeMowPath()` — kein automatisierter Test.
  Eine Kommentar-Sektion am Ende der Datei beschreibt einen
  "BUG-3-Verifikationstest" als Text, aber er wird nirgendwo aufgerufen
- K-Turn, routeAroundExclusions, inwardOffsetPolygon — ungetestet
- GeoJSON Import/Export Round-trip — ungetestet

**Hardware-Protokoll:**
- CRC-Algorithmus (Summe vs. XOR, → BUG-004): kann nur auf echter
  Hardware verifiziert werden — kritisch vor A.9

**Empfehlung (Priorität):**
1. `tests/test_navigation.cpp` — StateEstimator + Map + LineTracker
   (blockiert A.9-Vertrauen)
2. `tests/test_op_machine.cpp` — checkBattery-Path, Priority-Guard,
   GpsWait-Timeout
3. `useMowPath.test.ts` (Vitest) — computeMowPath für Rechteck, L-Form,
   mit und ohne Exclusions, K-Turn Overshoot-Grenze

---

## 3. OFFENE BAUSTELLEN

**Status:** akzeptabel für Phase 1 — drei Items mit konkretem Risiko

**Befund:**

### Phase-2-TODOs im Code

| Ort                          | TODO                                             | Risiko   |
|------------------------------|--------------------------------------------------|----------|
| `Robot.cpp:135,228`          | `gpsFixAge_ms = 9999999` (BUG-006)              | mittel   |
| `StateEstimator.cpp:107`     | GPS-Fusion (Komplementärfilter) fehlt           | niedrig* |
| `Map.cpp` (Kommentar)        | A*-Pfadplanung ist Placeholder                   | niedrig* |
| `useMowPath.ts:696-703`      | BUG-3-Verifikationstest ist totes Kommentar     | niedrig  |
| `UbloxGpsDriver` (baudRate)  | Nur 9600–115200 unterstützt (ZED-F9P macht 460k)| niedrig  |

*akzeptabel für Phase 1, aber vor Phase 2 zwingend

### Stubs ohne Implementierung

- **Spiral-Pattern** (`zone.settings.pattern = 'spiral'`): Im UI wählbar,
  in `computeMowPath()` vollständig ignoriert → BUG-012. Benutzer kann
  Spiral-Zonen erstellen, erhält aber Streifenpfad.
- **AT+W Waypoint-Batches** (TODO C.2b): Der berechnete Mähpfad kann
  bisher nicht an den Robot übertragen werden. Phase-2-Blocker für
  autonomes Mähen.
- **Energie-Budget/Rückkehr** (TODO C.6): Kein Schutz vor leerem Akku
  weit vom Dock entfernt (nur Spannungsmessung, keine Streckenplanung).
- **IMU** (StateEstimator): Alle IMU-Pfade (Tilt, Kalibrierung, Heading-
  Fusion) aus dem Original entfernt. `onImuTilt()` und `onImuError()` in
  MowOp sind vorhanden, werden aber nie aufgerufen (kein Sensor → kein
  Event). Das ist für Phase 1 korrekt, muss aber vor Phase 2 dokumentiert
  werden.

### Offene Fragen aus TODO.md (Stand heute ungelöst)

- **Q7 — Pi-Watchdog (15s) und STM32-Watchdog (6s) koordiniert?**
  Bei `keepPowerOn(false)` gibt es eine 5s Grace-Period, dann
  `shutdown now`. Der STM32-Watchdog wartet 6s auf Motor-Frames.
  Lücke: Wenn Pi-Shutdown >6s dauert, könnte STM32 in Failsafe gehen.
  Unkritisch für Phase 1 wenn Motoren vorher gestoppt werden — aber
  nicht verifiziert.

- **Q1 — StateEstimator Fehler-Propagation:**
  Wenn GPS dauerhaft ungültig, läuft StateEstimator im reinen
  Odometrie-Modus. Drift akkumuliert sich ohne Korrektur. Kein Flag
  exportiert "odometry-only mode" → MowOp weiß nicht, ob Positions-
  daten noch vertrauenswürdig sind.

**Empfehlung:**
- Spiral-Pattern entweder implementieren oder das UI-Dropdown
  deaktivieren/entfernen (verhindert Benutzerverwirrung)
- `gpsFixAge_ms` vor A.9-Test auf echte Berechnung umstellen
- Q7 (Watchdog-Koordination) vor Hardware-Test dokumentieren/testen

---

## 4. WEBUI-STABILITÄT

**Status:** problematisch — Production-ready nur für einfache Szenarien

**Befund:**

### useMowPath.ts — Algorithmus-Bewertung

**Funktioniert zuverlässig:**
- Konvexe Polygone ohne Exclusions (häufigster Testfall)
- Boustrophedon-Sortierung (Nearest-Neighbor greedy — korrekt)
- Zero-Turn-Übergänge (geometrisch korrekt)
- Winkelrotation (BUG-3 korrekt verifiziert in Kommentar)
- Clipping gegen Perimeter via Intervall-Schnitt (BUG-4 Fix)

**Nicht zuverlässig für Produktion:**

| Problem | Auswirkung | Häufigkeit |
|---------|------------|------------|
| BUG-008: Bypass nur oben/unten | Robot fährt durch No-Go-Zone beim Transit | häufig bei realen Gärten |
| BUG-009: K-Turn Overshoot Formel | Wendemanöver verlässt Zone an Ecken | bei konvexen kurzen Randstreifen |
| BUG-010: Doppelklick-Vertex | Jedes Polygon enthält Defekt-Punkt | bei jedem per Dblclick geschlossenem Polygon |
| inwardOffsetPolygon Konkav | Randstreifen-Offset ungenau an konkaven Ecken | jedes nicht-konvexe Polygon |
| Spiral-Pattern fehlt | Silent mismatch zwischen UI und Algorithmus | jede Zone mit pattern='spiral' |

### K-Turn Wurzel-Analyse (BUG-009)

Der eigentliche Fehler liegt in einem Kommentar-zu-Code-Mismatch:

```
generateStrips():  safeInset = Math.min(inset, segLen × 0.4)
kTurnTransition(): stripSafeInset = Math.min(inset, stripLen × 2)
```

`stripLen` ist die Länge des fertig ingesetzten Streifens.
`segLen` ist die Länge des noch nicht ingesetzten Rohsegments.
Relation: `stripLen ≈ segLen - 2 × safeInset`

Für normale (lange) Streifen wo `safeInset = inset`:
beide Formeln ergeben dasselbe (= inset). Kein Problem.

Für kurze Rand-Streifen an Polygon-Ecken wo `segLen × 0.4 < inset`:
- `generateStrips` begrenzt auf `segLen × 0.4` (klein, korrekt)
- `kTurnTransition` begrenzt auf `stripLen × 2` (viel größer → Bug)

Ergebnis: Der Overshoot-Punkt P2 liegt bei kurzen Streifen außerhalb
der Zone → der Rückwärts-Punkt P3 ebenfalls → Robot verlässt
Mähbereich.

**Root Cause:** `stripLen × 2` wurde als "großzügige Obergrenze"
eingesetzt, aber sie ist geometrisch falsch. Der Kommentar beschreibt
die Intention ("exakte Rekonstruktion"), die Implementierung setzt sie
nicht um.

### Edge-Cases ohne Abdeckung

1. Leeres Perimeter-Polygon (< 3 Punkte) → abgefangen
2. Alle Streifen kürzer als Mindestlänge → leeres Result (korrekt)
3. Zone vollständig in Exclusion → ungetestet
4. Überlappende Exclusions → ungetestet
5. Winkel = 90° (vertikale Streifen) → mathematisch korrekt, aber
   Nearest-Neighbor-Sortierung kann suboptimal sein
6. Sehr schmale Zonen (Breite < 2 × stripWidth) → keine Streifen,
   aber Randstreifen könnten sich überschneiden

**Empfehlung:**
1. BUG-010 (Doppelklick) sofort fixen — blockiert alle Polygon-Eingaben
2. BUG-008 (Transit durch No-Go) fixen vor erstem echten Test
3. Vitest-Suite mit 5–6 Testfällen (Rechteck, L-Form, Exclusion)
   anlegen bevor weitere Algorithmus-Arbeit

---

## 5. RISIKEN VOR ALFRED-TEST (A.9)

**Status:** 2 kritische / 3 hohe Risiken — Test kann scheitern

**Befund:**

### RISIKO-1 (kritisch) — CRC-Algorithmus ungeklärt (BUG-004)

Wahrscheinlichkeit: **mittel-hoch**

Der Sunray-Alfred-STM32-Code (Original in `e:/TRAE/Sunray/`) verwendet
für das AT-Protokoll eine bestimmte CRC-Methode. Die Pi-Seite verwendet
Byte-**Summe** (`crc += c`), die Dokumentation sagt **XOR** (`crc ^= c`).
Wenn Alfred XOR nutzt: alle eingehenden AT+M/S/V-Frames werden mit
"CRC error" verworfen. Der Robot verhält sich sofort wie "MCU not
responding" — `battery_.voltage = 28V` Fallback bleibt, aber alle Sensoren
bleiben auf Default. **Der Test würde komplett fehlschlagen.**

**Vor A.9 zwingend:** Alten Alfred-Code in `e:/TRAE/Sunray/` auf
CRC-Algorithmus prüfen und angleichen.

### RISIKO-2 (kritisch) — setMowMotor() in GpsWaitFixOp

`GpsWaitFixOp::begin()` ruft `ctx.hw.setMowMotor(0, 0, 0)` auf. Diese
Methode existiert in `HardwareInterface` nicht. Wenn der GpsWait-Op
jemals triggered (GPS-Verlust während Mähen), **kompiliert der Code
nicht** bzw. linkedet nicht.

**Vor A.9 zwingend:** Compile-Test auf dem Pi selbst (`make -j4`), der
diesen Linker-Fehler aufdeckt.

### RISIKO-3 (hoch) — Buzzer-Inversion bei kritischer Batterie (BUG-002)

`setBuzzer(false)` bei Critical-Battery: Benutzer hört keinen Alarm,
Robot fährt weiter bis `running_.store(false)` greift. Akku könnte
Tiefentladung erleiden wenn der Shutdown-Befehl nicht schnell genug
durchläuft.

### RISIKO-4 (hoch) — GPS-Port Pfad nicht garantiert verfügbar

`DEFAULT_PORT` in `UbloxGpsDriver` ist ein fester `/dev/serial/by-id/...`
Pfad. Dieser Pfad ist gerätespezifisch (USB-ID des ZED-F9P). Falls
der Dongle nicht erkannt wird oder ein anderer Port zugewiesen wird,
scheitert `init()` ohne Retry-Logik. Robot startet ohne GPS, kein
Fehler-Feedback außer einem `logger->error(TAG, "Cannot open GPS port")`.

**Vor A.9:** Config-Wert `gps_port` in `config.json` korrekt setzen und
mit `ls /dev/serial/by-id/` verifizieren.

### RISIKO-5 (hoch) — Keine Navigation-Tests auf echter Hardware

Die gesamte Navigation-Pipeline (StateEstimator → LineTracker → Map →
MowOp) ist ausschließlich mit MockHardware getestet. Auf echter Hardware
kommen hinzu:
- Encoder-Rauschen → mehr >0.5m/frame Verwürfe?
- AT+M-Frequenz-Jitter (50Hz nominal, real 45–55Hz) → dt_ms-Variation
- GPS-RTK-Float → direkte Positions-Überschreibung in StateEstimator
  könnte zu Heading-Diskontinuitäten führen (Phase-2-Fusion fehlt)

**Annahme nicht verifiziert:** Die Stanley-Konstanten (`stanley_k=0.5`,
`stanley_p=1.0`) aus `config.example.json` sind für Alfred-Geometrie
(wheelbase 285mm) kalibriert. Nie auf echtem Gras getestet.

### RISIKO-6 (mittel) — keepPowerOn(false) → shutdown now blockiert run()-Loop

Bei Critical-Battery: `running_.store(false)` + `keepPowerOn(false)`.
Danach in `SerialRobotDriver::run()`: `std::system("shutdown now")` —
dieser Aufruf **blockiert den aufrufenden Thread** bis die Shell
abgeschlossen ist. In dieser Zeit werden keine AT+M Frames gesendet.
STM32-Watchdog (6s) kann auslösen und Motoren unkontrolliert starten.
Praktisch unkritisch wenn Motoren vorher gestoppt — aber nicht garantiert.

**Empfehlung — Prioritäts-Checkliste vor A.9:**

| Priorität | Aktion                                                        |
|-----------|---------------------------------------------------------------|
| P0        | CRC-Algorithmus in Alfred-Original verifizieren + angleichen  |
| P0        | `setMowMotor` → `setMotorPwm` in GpsWaitFixOp fixen + Build  |
| P1        | `setBuzzer(true)` bei Critical-Battery                        |
| P1        | `gps_port` in config.json auf Pi korrekt setzen               |
| P1        | Null-Guard in `checkBattery()` ergänzen                       |
| P2        | `gpsFixAge_ms` mit echtem Timestamp versehen                  |
| P2        | Manuelle Validierung: AT+M Frames auf Serial-Monitor prüfen   |

---

## Gesamtbewertung

| Bereich                         | Status         |
|---------------------------------|----------------|
| C++ Architektur (DI, HAL)       | ✅ gut          |
| Phase-1 Funktionsumfang         | ✅ vollständig  |
| Infrastruktur-Testabdeckung     | ✅ akzeptabel   |
| Navigation-Testabdeckung        | ❌ problematisch|
| WebUI-Algorithmen (Produktion)  | ⚠️ eingeschränkt|
| A.9-Hardware-Readiness          | ⚠️ 2 Blocker    |
| Dokumentationsqualität          | ✅ gut          |

**Fazit:** Die Grundarchitektur ist solide und konsequent umgesetzt. Vor
dem Alfred-Hardware-Test (A.9) müssen zwei P0-Items behoben werden (CRC +
setMowMotor). Die WebUI-Algorithmen (useMowPath.ts) sind für einfache
rechteckige Gärten einsatzbereit — für komplexe Geometrien mit
Exclusion-Zonen sollten BUG-008 und BUG-010 zuerst behoben werden.
