# ALFRED_STM32_REFACTOR_PLAN

Last updated: 2026-03-29

## Zweck

Diese Datei analysiert die aktive Alfred-STM32-Firmware
[rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino)
und leitet daraus einen konkreten Refactoring-Plan ab.

Ziel des Refactorings ist nicht, das Verhalten neu zu erfinden, sondern:

- Verantwortlichkeiten sauber zu trennen
- sicherheitskritische Pfade besser lesbar zu machen
- die serielle Pi-Schnittstelle stabil zu halten
- spaetere Hardwareanpassungen und Tests zu erleichtern

## Kurzfazit

Die Firmware funktioniert heute als kompakter, historisch gewachsener
Board-Controller, ist aber strukturell stark verdichtet:

- ca. 956 Zeilen in einer einzelnen `.ino`
- viele globale Variablen
- ISR-Logik, Hardwarezugriff, Scheduler, Fehlerlogik und Protokoll in einer Datei
- serielle API und Sensor-/Motorpfade sind eng miteinander verschraubt

Der richtige Weg ist ein konservatives Refactoring in mehreren Schritten:

1. zuerst Struktur ohne Verhaltensaenderung
2. dann Modulgrenzen fuer Protocol / Sensors / Actuators / Safety
3. erst spaeter fachliche Korrekturen oder neue Features

## Bestandsaufnahme

## 1. Datei- und Strukturzustand

Datei:

- [rm18.ino](/mnt/LappiDaten/Projekte/sunray-core/alfred/firmware/rm18.ino)

Groesse:

- `956` Zeilen

Hauptcharakteristik:

- klassische Arduino-/STM32-Sketch-Struktur mit `setup()` und `loop()`
- keine klaren Moduldateien
- globaler Zustand fuer nahezu alle Runtime-Bereiche

## 2. Fachliche Verantwortlichkeiten in der Datei

Die Datei deckt mindestens diese Verantwortungsbereiche gleichzeitig ab:

### A. Board- und Pin-Definition

- fast komplette Pinbelegung des Alfred-Main-MCU
- gemischte Dokumentation fuer Board, Display, SWD, Motoren und Kommunikation

### B. Odometrie-Interrupts

- `OdometryLeftISR()`
- `OdometryRightISR()`
- `OdometryMowISR()`

inklusive Spike-Eliminierung und Tickzaehlung

### C. Stop-Button-Interrupt

- `stopButtonISR()`

### D. Aktorsteuerung

- `motor()`
- `mower()`
- `power(bool)`

### E. Sensoraufnahme

- `readSensorsHighFrequency()`
- `readSensors()`
- `readMotorCurrent()`

### F. Fault-/Overload-Logik

- `motorOverload`
- `motorPermanentFault`
- `motorOverloadCount`
- `motorOverloadTimeout`
- `ovCheck`
- `motorMowFault`

### G. AT-Protokoll / UART-Kommunikation

- `cmdAnswer()`
- `cmdMotor()`
- `cmdSummary()`
- `cmdVersion()`
- `cmdTriggerWatchdog()`
- `processCmd()`
- `processConsole()`

### H. Diagnose / Debug

- `printInfo()`
- `testPins()`
- `writePulse()`

### I. Hauptscheduler

- `loop()`

mit mehreren zeitscheibenbasierten Aufgaben:

- 20 ms Motor-/Mower-/HF-Sensor-Tick
- 100 ms Sensor-Lowrate
- 100 ms Current-Sampling
- 1000 ms Debug/Info
- 3000 ms Motor-Timeout-Schutz
- Watchdog-Reload

## 3. Kritische externe Schnittstellen

## 3.1 Pi-Protokoll

Die wichtigste externe Vertragsflaeche ist die serielle AT-Schnittstelle zum Pi.

Aktive Kommandos:

- `AT+M`
- `AT+S`
- `AT+V`
- `AT+Y`

Wichtige Stabilitaetsbedingung:

- Feldreihenfolge und Semantik von `AT+M` und `AT+S` duerfen bei strukturellem
  Refactoring nicht unbemerkt driftieren

Besonders sensibel:

- BUG-07 Links/Rechts-Tausch im PWM-/Encoder-Mapping
- BUG-06 `motorOverload` im 50-Hz-`AT+M`
- Summary-Felder fuer Lift, Rain, Fault, Currents, Temp, OV und Bumper

## 3.2 Echtzeit- und Sicherheitsverhalten

Kritisch:

- ISR muessen leichtgewichtig bleiben
- `loop()` muss den Watchdog sicher bedienen
- Motor-Timeout darf nie verloren gehen
- Overload-/Permanent-Fault-Logik darf durch Refactoring nicht entschärft werden

## 4. Hauptprobleme der aktuellen Struktur

## 4.1 Globaler Zustand ohne Grenzen

Nahezu alle Laufzeitdaten liegen als globale Variablen in einer Datei:

- Sensorwerte
- Low-Pass-Werte
- Faultflags
- PWM-Sollwerte
- Schedulerzeiten
- Kommunikationsbuffer

Folgen:

- hohe Kopplung
- schwer nachvollziehbare Seiteneffekte
- geringe lokale Testbarkeit

## 4.2 Vermischung von Hardwarezugriff und Fachlogik

Beispiele:

- `readMotorCurrent()` liest ADCs und entscheidet direkt ueber Fault-Eskalation
- `cmdMotor()` parst ein Protokoll und setzt direkt die Motor-Sollwerte
- `loop()` ist sowohl Scheduler als auch Integrationsort fuer alle Features

## 4.3 Implizite Zeitsteuerung

Zeitverhalten ist verteilt ueber viele `next...Time`-Variablen:

- `nextMotorControlTime`
- `nextBatTime`
- `nextMotorSenseTime`
- `motorTimeout`
- `motorOverloadTimeout`
- `nextInfoTime`

Das ist funktional okay, aber schwer zu pruefen und leicht versehentlich zu verschieben.

## 4.4 Protokoll und Business-Logik sind verklebt

`cmdMotor()` ist nicht nur Parser, sondern auch:

- Command-Dispatcher
- Sollwert-Uebergabe
- Motor-Timeout-Arming
- Telemetrie-Antwortgenerator

Das gleiche gilt fuer `cmdSummary()`.

## 4.5 Hardwarekalibrierung ist nicht isoliert

Beispiele:

- Spannungsformeln
- Current-Kennlinien
- Lift-/Bumper-Schwellen
- Regen-Schwelle

Diese Kalibrierwerte sind direkt im Laufcode verteilt statt in einer klaren
Board-/Calibration-Schicht zu liegen.

## 5. Refactoring-Ziele

Der Zielzustand sollte sein:

- identisches serielles Verhalten zum Pi
- identisches Sicherheitsverhalten
- kleinere, klar benannte Module
- globale Variablen deutlich reduziert
- Scheduler sichtbar und pruefbar
- Kalibrier- und Mapping-Werte zentralisiert

Nicht Ziel:

- sofortiger Port auf moderne HAL-Abstraktionen
- Protokollneudesign
- Funktionsaenderung am Fahr- oder Faultverhalten in Commit 1

## 6. Empfohlene Zielstruktur

Empfohlene Modultrennung innerhalb `alfred/firmware/`:

### 1. `board_pins.h`

Inhalt:

- alle Pin-Defines
- Board-/Revision-Kommentare

### 2. `board_state.h`

Inhalt:

- zentrale Runtime-Structs statt loser Globals
- z. B.:
  - `OdomState`
  - `SensorState`
  - `MotorState`
  - `FaultState`
  - `ProtocolState`
  - `SchedulerState`

### 3. `isr_handlers.h/.cpp`

Inhalt:

- Odometry-ISR
- Stop-Button-ISR

### 4. `actuators.h/.cpp`

Inhalt:

- `motor()`
- `mower()`
- `power()`

### 5. `sensors.h/.cpp`

Inhalt:

- `readSensorsHighFrequency()`
- `readSensors()`
- `readMotorCurrent()`

### 6. `faults.h/.cpp`

Inhalt:

- Overload-Eskalation
- Permanent-Fault
- Fault-Latch-Clear-Regeln

### 7. `protocol.h/.cpp`

Inhalt:

- CRC
- `cmdAnswer()`
- Parser
- `cmdMotor()`
- `cmdSummary()`
- `cmdVersion()`
- `cmdTriggerWatchdog()`
- `processCmd()`
- `processConsole()`

### 8. `scheduler.h/.cpp`

Inhalt:

- zyklische Aufrufreihenfolge aus `loop()`
- Tickfrequenzen und Timeout-Handling

### 9. `debug_tools.h/.cpp`

Inhalt:

- `printInfo()`
- `testPins()`
- `writePulse()`

## 7. Refactoring-Regeln

Diese Regeln sollten waehrend des Umbaus strikt gelten.

### Regel 1: Protokoll einfrieren

Vor jeder strukturellen Aenderung muessen diese Antworten bytegenau eingefroren
oder mindestens dokumentiert werden:

- `AT+M`
- `AT+S`
- `AT+V`

### Regel 2: ISR zuerst nicht fachlich aendern

`Odometry*ISR()` und `stopButtonISR()` duerfen in der ersten Phase nur verlagert,
nicht neu interpretiert werden.

### Regel 3: Schedulerreihenfolge nicht veraendern

Die aktuelle Reihenfolge in `loop()` ist Teil des Verhaltens.

Erst nach Baseline-Absicherung aendern:

- 20 ms Aktor-/HF-Sensorpfad
- `motorTimeout`
- `processConsole()`
- 1 s Info
- 100 ms Sensoren
- 100 ms Currents
- Overload-Reset
- Watchdog reload

### Regel 4: BUG-/IMP-/TASK-Kommentare erhalten

Die Kommentare in der Datei tragen echte Hardwaregeschichte:

- `BUG-*`
- `IMP-*`
- `TASK-*`

Diese sollten nicht beim ersten Refactoring verlorengehen, sondern in neue Module
uebernommen werden.

## 8. Empfohlene Refactoring-Phasen

## Phase 0: Baseline sichern

Vor dem Umbau dokumentieren:

- Protokollfelder `AT+M`, `AT+S`, `AT+V`
- aktuelle Schedulerfrequenzen
- Pinmapping
- Overload- und Timeout-Pfade

Optional sehr sinnvoll:

- kleine Host-seitige Parser-/Golden-Frame-Tests fuer die Stringantworten
- Testnotiz fuer reale Stop-Button-, Bumper-, Lift- und Overload-Szenarien

## Phase 1: Rein strukturelle Extraktion

Ohne Verhaltensaenderung:

1. Pin-Defines nach `board_pins.h`
2. globale Variablen thematisch in State-Structs gruppieren
3. `cmdAnswer()` und CRC-Helfer isolieren
4. `cmdVersion()`, `cmdSummary()`, `cmdMotor()` in `protocol.*`
5. `printInfo()`, `testPins()` in `debug_tools.*`

Erfolgskriterium:

- Sketch baut unveraendert
- Pi-Protokoll bleibt identisch

## Phase 2: Sensorik und Aktorik entkoppeln

1. `motor()` und `mower()` in `actuators.*`
2. `readSensors*()` und `readMotorCurrent()` in `sensors.*`
3. Kalibrierwerte sichtbar zusammenziehen

Erfolgskriterium:

- gleiche `AT+S`-/`AT+M`-Antworten
- keine Timingregression im 20-ms-Pfad

## Phase 3: Fault- und Safety-Logik explizit machen

1. `motorOverload`, `motorPermanentFault`, Recovery-Timer in `faults.*`
2. reine Zustandsfunktion fuer:
   - overload erkannt
   - timeout aktiv
   - permanent fault gesetzt
   - recovery freigeben

Erfolgskriterium:

- Fault-Eskalation bleibt gleich
- Safety-Verhalten ist in einem eigenen Modul lesbar

## Phase 4: Scheduler lesbar machen

`loop()` nur noch als orchestrierender Scheduler:

- `tick20ms()`
- `tick100msSensors()`
- `tick100msCurrent()`
- `tick1sInfo()`
- `tickTimeouts()`
- `tickProtocol()`
- `tickWatchdog()`

Erfolgskriterium:

- Reihenfolge bleibt gleich
- `loop()` wird kurz und auditierbar

## Phase 5: Optional spaetere fachliche Verbesserungen

Erst nach strukturellem Umbau:

- kalibrierbare Schwellen in zentrale Config/Calibration
- robustere String-/Command-Verarbeitung
- getrennte RX-Buffer pro UART statt gemeinsames `cmd`
- bessere Typisierung fuer Protokollframes
- explizite Tests fuer BUG-07/BUG-06/IMP-07/TASK-BUMPER-01

## 9. Groesste Refactoring-Risiken

## Risiko 1: Pi-Protokoll driftet still

Schon kleine Feldverschiebungen in `cmdMotor()` oder `cmdSummary()` koennen den
Linux-Treiber brechen.

## Risiko 2: Zeitverhalten verschiebt sich

Wenn Sampling- oder Control-Takte versehentlich anders laufen:

- Odometrie kann driften
- Overload-Recovery kann zu spaet/zu frueh greifen
- Watchdog kann triggern

## Risiko 3: ISR-Verhalten veraendert sich

Schon kleine Aenderungen an Debounce-/Spike-Logik koennen Ticks verfälschen.

## Risiko 4: Fault-Latches werden versehentlich entschärft

`motorPermanentFault` und `motorOverloadTimeout` sind sicherheitsrelevant.

## 10. Konkreter Plan fuer die naechsten Commits

### Commit 1: Struktur-Baseline

- `board_pins.h`
- `board_state.h`
- `protocol_crc.h/.cpp`
- keine Fachlogikaenderung

### Commit 2: Protokollmodul

- `protocol.h/.cpp`
- `cmdMotor()`, `cmdSummary()`, `cmdVersion()`, `processCmd()`, `processConsole()`
- gleiche Antwortframes beibehalten

### Commit 3: Sensor-/Aktor-Module

- `actuators.*`
- `sensors.*`
- `debug_tools.*`

### Commit 4: Fault- und Scheduler-Modul

- `faults.*`
- `scheduler.*`
- `loop()` nur noch orchestration

## 11. Empfehlung

Die Firmware sollte refaktoriert werden, aber konservativ.

Die Prioritaet ist nicht "modernisieren um jeden Preis", sondern:

- Protokollstabilitaet
- Sicherheitsverhalten
- Hardwareverhalten
- bessere Lesbarkeit und Wartbarkeit

Kurzempfehlung:

1. erst Struktur ohne Verhaltensaenderung
2. dann Protokoll und Sensorik entkoppeln
3. Fault-/Scheduler-Logik explizit machen
4. fachliche Verbesserungen erst danach
