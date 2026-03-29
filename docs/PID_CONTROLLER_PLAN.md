# PID_CONTROLLER_PLAN

Last updated: 2026-03-29

## Zweck

Dieses Dokument beschreibt, wie ein PID-basierter Radgeschwindigkeitsregler in
`sunray-core` eingefuehrt werden kann, ohne den bestehenden Stanley-/Op-/HAL-
Aufbau zu zerstoeren.

## Kurzfazit

Aktuell ist `sunray-core` auf Fahrmotorseite im Kern `open-loop`:

- [`LineTracker.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)
  berechnet `linear` und `angular`
- [`Op.h`](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h) rechnet das direkt
  in linke/rechte PWM um
- [`HardwareInterface.h`](/mnt/LappiDaten/Projekte/sunray-core/hal/HardwareInterface.h)
  kennt nur `setMotorPwm(int left, int right, int mow)`

Dadurch gibt es keinen geschlossenen Regelkreis pro Rad:

- Lastaenderungen
- Steigungen
- Reibungsunterschiede links/rechts
- schwankende Batteriespannung

werden nicht aktiv ueber Encoderfeedback ausgeregelt.

Ein PID pro Antriebsrad wuerde genau diese Luecke schliessen.

## Problem heute

## Aktuelle Kette

Heute ist die Fahrkommandokette:

1. Navigation / Op bestimmt Sollbewegung:
   - `linear` in m/s
   - `angular` in rad/s
2. `OpContext::setLinearAngularSpeed()` rechnet diese Sollbewegung sofort in
   PWM um
3. der Treiber setzt die PWM direkt an die Hardware
4. Encoderwerte gehen nur in die Zustandsschaetzung ein, nicht in die
   Motornachregelung

## Folge

Das Fahrzeug kann sich in der Pose noch halbwegs korrigieren, aber nur indirekt:

- Stanley sieht Abweichung auf der Bahn
- erzeugt neues globales Fahrkommando
- setzt wieder nur neue PWM

Was fehlt:

- "linkes Rad sollte 0.22 m/s fahren, faehrt aber real nur 0.14 m/s"
- "rechtes Rad ist schneller als gewollt"
- "erhoehe/reduziere PWM lokal bis das Rad das Soll erreicht"

## Zielbild

Zielarchitektur:

- Stanley bleibt der Bahnregler
- darunter entsteht ein lokaler Geschwindigkeitsregler pro Rad

Regelungsebenen:

1. **Pfadregelung**
   - Eingang: Pose + Zielpfad
   - Ausgang: Soll-Linear- und Soll-Angulargeschwindigkeit

2. **Kinematische Aufteilung**
   - Eingang: `v`, `omega`
   - Ausgang: Soll-Radgeschwindigkeiten links/rechts in m/s oder ticks/s

3. **Motorregelung per PID**
   - Eingang: Soll-Radgeschwindigkeit vs. Ist-Radgeschwindigkeit
   - Ausgang: PWM links/rechts

4. **HAL / Treiber**
   - setzt PWM
   - liefert Encoderdeltas

## Was nicht der PID macht

Der PID ersetzt nicht:

- Stanley
- Dock-/Undock-Logik
- Mow-/Escape-Op-Logik
- StateEstimator

Der PID ist nur die fehlende lokale Motorregelung unterhalb der Navigation.

## Empfohlene Einbauposition

## Nicht in den Treiber vergraben

Der PID sollte nicht direkt in
[SerialRobotDriver.cpp](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
landen, weil:

- dann waere er Alfred-spezifisch
- Simulation und kuenftige Pico-Hardware wuerden eine zweite Regelung brauchen
- der Core verlöre die Kontrolle ueber Sollwerte und Testbarkeit

## Empfohlene Position: Core-/Control-Layer

Empfehlung:

- neuer Core-Baustein, z. B. `core/control/DifferentialDriveController`
- dieser lebt zwischen `OpContext::setLinearAngularSpeed()` und `hw.setMotorPwm()`

Neue Kette:

1. `LineTracker` / Ops liefern `v`, `omega`
2. `DifferentialDriveController` berechnet daraus Soll-Radgeschwindigkeiten
3. Controller misst Ist-Geschwindigkeit aus Odometry
4. zwei PIDs erzeugen PWM links/rechts
5. `HardwareInterface::setMotorPwm()` bekommt das Ergebnis

## Ziel-API

Mittel- bis langfristig sollte `OpContext` nicht mehr direkt PWM aus
`v`/`omega` berechnen.

Sinnvoll waere z. B.:

```cpp
ctx.setLinearAngularSpeed(v, omega);
```

bleibt als API erhalten, macht intern aber nicht mehr:

- `m/s -> PWM`

sondern:

- `m/s -> wheel_speed_targets`

und der Controller setzt spaeter die PWM.

## Benoetigte neue Bausteine

## 1. `PidController`

Kleine wiederverwendbare Klasse:

- `kp`, `ki`, `kd`
- Integrator
- letzter Fehler
- Anti-Windup
- Ausgangslimit

Dateivorschlag:

- `core/control/PidController.h`
- `core/control/PidController.cpp`

## 2. `DifferentialDriveController`

Verantwortung:

- Sollwert `v`, `omega` annehmen
- in Soll-Radgeschwindigkeit links/rechts umrechnen
- Ist-Radgeschwindigkeit aus Tickdeltas und `dt` berechnen
- zwei PIDs ausfuehren
- PWM links/rechts ausgeben

Dateivorschlag:

- `core/control/DifferentialDriveController.h`
- `core/control/DifferentialDriveController.cpp`

## 3. `DriveCommand`

Optionale kleine Struktur:

```cpp
struct DriveCommand {
    float linear_ms;
    float angular_radps;
};
```

Hilft, die Sollseite vom PWM-Ausgang zu trennen.

## 4. Telemetrie/Diagnose

Fuer Tuning sehr wichtig:

- Soll-Radgeschwindigkeit links/rechts
- Ist-Radgeschwindigkeit links/rechts
- PWM links/rechts
- PID-Fehler links/rechts
- optional Integratorstand

Sonst ist spaeteres Tuning auf echter Hardware blind.

## Neue oder angepasste Config-Parameter

Vorhanden, aber heute ungenutzt:

- `motor_pid_kp`
- `motor_pid_ki`
- `motor_pid_kd`
- `motor_pid_lp`

Ergaenzen oder klar definieren:

- `motor_pid_output_limit`
- `motor_pid_integral_limit`
- `motor_pid_deadband_ticks_s`
- `motor_pid_enable`
- `motor_pid_feedforward_gain`
- `motor_pid_min_pwm`

Wichtiger Punkt:

- die existierenden PID-Keys sollten auf diese neue Verwendung bezogen werden
- nicht zwei konkurrierende Bedeutungen einfuehren

## Regelgroesse: m/s oder ticks/s?

Empfehlung:

- intern fuer die Regelung `ticks/s`

Warum:

- Encoder liefern direkt Ticks
- weniger Umrechnungsrauschen
- einfacher fuer Test und Diagnose

Alternative:

- Soll in m/s, intern sofort in ticks/s umrechnen

Das Ziel sollte in jedem Fall sein:

- Regler arbeitet auf einer Groesse, die direkt zur Encoder-Messung passt

## Umgang mit verschiedenen Betriebsarten

## 1. Normale Navigation

PID aktiv:

- `NavToStart`
- `Mow`
- `Dock`
- `Undock`
- `EscapeReverse`
- `EscapeForward`

## 2. Manual Drive

PID ebenfalls sinnvoll aktiv.

Sonst bleibt genau dort das heutige Open-Loop-Verhalten bestehen.

## 3. Diagnosebetrieb

Sonderfall:

- `diagRunMotor()` will eventuell explizit rohe PWM setzen

Empfehlung:

- Diagnoserohmodus beibehalten
- Diagnosepfade duerfen den PID bewusst umgehen

## 4. Simulation

Die Simulation braucht klare Entscheidung:

- entweder PID auch in der Simulation aktiv
- oder bewusst bypassen und nur Kinematik testen

Empfehlung:

- PID in Simulation aktivierbar machen
- aber separat toggelbar fuer Tests

## Phasenplan

## Phase 0: Vertrag einfrieren

Vor Codeaenderungen dokumentieren:

- heutige Call Chain
- direkte PWM-Mapping-Stelle in `OpContext::setLinearAngularSpeed()`
- bestehende Config-Keys
- relevante Tests fuer Navigation und Odometry

## Phase 1: PWM-Mapping aus `OpContext` herausziehen

Heute sitzt die Logik inline in
[Op.h](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h).

Erster Schritt:

- Mapping in eine eigene Komponente verlagern
- Verhalten noch gleich lassen
- noch kein PID

Ziel:

- Architektur oeffnen, ohne Verhalten zu aendern

## Phase 2: `DifferentialDriveController` ohne geschlossenen Regelkreis

Der neue Controller uebernimmt zunaechst nur:

- `v`, `omega` -> Soll-Radgeschwindigkeit
- Soll-Radgeschwindigkeit -> bisheriges Open-Loop-PWM

Also:

- gleiche Funktion
- neue Struktur

## Phase 3: Ist-Geschwindigkeit aus Odometry berechnen

Neue Logik:

- aus `leftTicks`, `rightTicks`, `dt`, `ticks_per_revolution`, `wheel_diameter_m`
- resultierende Ist-Geschwindigkeit pro Rad berechnen

Wichtig:

- Aussetzer bei `mcuConnected=false` robust behandeln
- erste Zyklen nach Reconnect nicht in den PID laufen lassen

## Phase 4: PID geschlossen aktivieren

Dann erst:

- zwei PIDs aktivieren
- PWM aus PID-Ausgang erzeugen
- Ausgang begrenzen
- Integrator resetten bei Stop / Fault / Idle / no MCU

## Phase 5: Telemetrie und Tuning

Danach:

- Debug-/Telemetryfelder ergaenzen
- Hardwaretuning auf Pi/Alfred
- realen Geradeauslauf, Steigung, Docking und Escape pruefen

## Testplan

## Unit-Tests

Neue Tests fuer:

- `PidController` Grundverhalten
- Sättigung / Anti-Windup
- Reglerreset
- `DifferentialDriveController`:
  - `v=0`, `omega=0`
  - Vorwaertsfahrt
  - Rotation
  - links/rechts asymmetrische Istwerte

## Integrations-Tests

Bestehende Navigationstests erweitern:

- `LineTracker`-Kommandos fuehren weiter zu plausiblen PWM-Richtungen
- `Robot`-Run integriert Odometry in den neuen Controller
- Fault-/Stop-Pfade resetten den PID sauber

## Hardwaretests

Auf Alfred spaeter pruefen:

- Geradeausfahrt auf ebener Strecke
- Geradeausfahrt mit asymmetrischer Last
- leichtes Gefaelle/Steigung
- Docking-Anfahrt langsam
- EscapeReverse/Forward
- Manual Drive

## Hauptrisiken

## Risiko 1: Zwei geschlossene Kreise beeinflussen sich

Stanley oben und PID unten koennen schwingen, wenn:

- der PID zu aggressiv ist
- `v`/`omega` zu sprunghaft sind
- Messrauschen hoch ist

Deshalb:

- zuerst konservativ tunen
- eventuell kleinen Sollwertfilter einbauen

## Risiko 2: Falsche Tick-/Geschwindigkeitsberechnung

Wenn die Umrechnung falsch ist, regelt der PID sauber auf die falsche Groesse.

## Risiko 3: Fault-/Stop-Reset vergessen

Integrator muss sauber geloescht werden bei:

- `Idle`
- `Error`
- `GpsWait`
- `keepPowerOn(false)`
- `mcuConnected=false`
- Stop-Button / Lift / Bumper / MotorFault

## Risiko 4: Alfred-STM32 macht intern bereits nichtlineare Dinge

Die MCU-Firmware hat PWM-Besonderheiten und Low-PWM-Korrekturen.
Deshalb:

- Feedforward plus PID ist wahrscheinlich besser als "nur PID"

## Empfohlene Umsetzung in Commits

### Commit 1

- neues Dokument erstellen
- `OpContext::setLinearAngularSpeed()` entkoppeln
- neues Open-Loop-Drive-Mapping als eigene Klasse

### Commit 2

- `PidController`
- `DifferentialDriveController` mit Istgeschwindigkeit
- noch per Featureflag deaktiviert

### Commit 3

- PID per Config aktivierbar
- Unit-Tests und Integrations-Tests

### Commit 4

- Telemetrie fuer Tuning
- Alfred-Hardwaretest und Parameteranpassung

## Empfehlung

Ja, ein PID-Controller ist fachlich sinnvoll und wahrscheinlich der naechste
groesse fahrdynamische Qualitaetssprung nach der jetzigen Architekturarbeit.

Aber:

- nicht direkt in den Treiber einbauen
- nicht sofort "scharf" schalten
- erst die Struktur oeffnen, dann den geschlossenen Kreis aktivieren

Kurz gesagt:

- Stanley bleibt
- PWM-Open-Loop verschwindet
- darunter kommt ein sauber testbarer Radgeschwindigkeitsregler
