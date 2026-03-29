# IST_SOLL_ANALYSE

Last updated: 2026-03-29

## Zweck

Diese Analyse vergleicht den dokumentierten Funktions- und Hardwareumfang der
originalen Sunray/Ardumower-Software in
[`ALTE_DATEIEN/Sunray/SUNRAY_FIRMWARE_ANALYSE.md`](/mnt/LappiDaten/Projekte/sunray-core/ALTE_DATEIEN/Sunray/SUNRAY_FIRMWARE_ANALYSE.md)
mit dem aktuellen aktiven Stand von `sunray-core`.

Ziel:

- Was ist in `sunray-core` bereits gleichwertig oder besser?
- Was fehlt gegenüber dem Original noch?
- Ist die Alfred-Hardware heute fachlich richtig eingebunden?

## Kurzfazit

`sunray-core` ist heute in Architektur, Testbarkeit, Deploybarkeit und
Web-/API-Modell klar moderner als die Originalstruktur.

Besonders besser:

- saubere C++-Modulstruktur statt Arduino-Sketch + Linux-Shim-Mischsystem
- klarer `HardwareInterface`-Boundary
- deutlich stärkerer Testbestand für Core-, Telemetrie- und
  State-Machine-Verhalten
- moderne WebUI mit explizitem Backend-Vertrag
- klarere Op-/Recovery-Modellierung (`Undock`, `NavToStart`, `GpsWait`,
  `WaitRain`, Telemetrie-Vertrag)

Besonders noch nicht auf Original-Niveau:

- einige Linux-Plattformdienste des Originals fehlen komplett:
  - BLE
  - CAN/SLCAN
  - Audio/TTS/Startsound
  - Kamera-/`motion`-Service
- NTRIP ist konfigurierbar, aber derzeit nicht als aktiver Client im Core
  umgesetzt
- die Alfred-I2C-Topologie ist nur teilweise übernommen; insbesondere ist die
  Nutzung des TCA9548A-Multiplexers im aktiven Treiber nicht sichtbar
- der physische Alfred-Stop-Button ist zwar im Datenmodell vorhanden, aber
  aktuell ohne Bedienlogik wirkungslos

Gesamtbewertung:

- Core- und Missionslogik: besser als Original
- Linux-Plattformintegration: noch unvollständig gegenüber Original
- Alfred-Hardwareeinbindung: für die Grundfunktionen vorhanden, aber noch nicht
  vollständig auf echte Hardwareparität verifiziert

## Vergleich Auf Einen Blick

| Bereich | Original Sunray | Aktueller `sunray-core` Stand | Bewertung |
|---|---|---|---|
| Architektur | Arduino-Sketch + Linux-Shims | Linux-native C++-Runtime mit klaren Modulen | `sunray-core` besser |
| Hardware-Abstraktion | `RobotDriver`-Familie, viele Plattformpfade | ein zentrales `HardwareInterface`, aktuell v. a. Alfred + Sim | `sunray-core` klarer, aber weniger Plattformen |
| State-Machine | vorhanden, aber historisch enger verdrahtet | explizite Ops + dokumentierte Recovery-Pfade + Tests | `sunray-core` besser |
| Web/API | historisch anders und weniger klar getrennt | Crow HTTP/WS + Vue 3 WebUI + Contract-Tests | `sunray-core` besser |
| Telemetrievertrag | impliziter | explizit dokumentiert und getestet | `sunray-core` besser |
| GPS/RTK | F9P-orientiert, RTCM/NTRIP-Ökosystem vorhanden | u-blox-Driver gut portiert, RTCM-Erkennung vorhanden, NTRIP-Client fehlt | teilweise gleich, teilweise offen |
| Alfred Bedienhardware | Taster- und Buzzer-Interaktion mit Hold-Durations und Soundmustern | Taster wird eingelesen, aber nicht ausgewertet; Buzzer nur Ein/Aus | Original vollständiger |
| Alfred Linux-Dienste | BLE, Audio, WiFi-Hooks, CAN/SLCAN, Kamera | nur Teilmenge im aktiven Core | Original vollständiger |
| Tests | historisch schwächer bzw. weniger isoliert | 179 native Testfälle, Frontend-Build/Test vorhanden | `sunray-core` besser |

## Was `sunray-core` Bereits Besser Macht

### 1. Architektur

Das Original ist funktional stark, aber strukturell ein Hybrid aus:

- Arduino-Anwendungslogik
- Linux-Shims
- plattformabhängigen Treibern
- Service-/Shell-Skripten

`sunray-core` trennt diese Ebenen heute klarer:

- [`core/`](/mnt/LappiDaten/Projekte/sunray-core/core) für Robotiklogik
- [`hal/`](/mnt/LappiDaten/Projekte/sunray-core/hal) für Hardwaregrenze
- [`platform/`](/mnt/LappiDaten/Projekte/sunray-core/platform) für Linux-nahe Adapter
- [`webui/`](/mnt/LappiDaten/Projekte/sunray-core/webui) für UI

Das ist für Wartbarkeit und gezielte Änderungen deutlich besser als das
Originalmodell.

### 2. Explizites Zustandsmodell

Im aktuellen Core sind zentrale Missionsphasen ausdrücklich modelliert:

- `Idle`
- `Undock`
- `NavToStart`
- `Mow`
- `Dock`
- `Charge`
- `WaitRain`
- `GpsWait`
- `EscapeReverse`
- `EscapeForward`
- `Error`

Zusätzlich sind Telemetrie-Felder wie:

- `state_phase`
- `resume_target`
- `event_reason`
- `error_code`

heute dokumentiert und getestet. Das ist fachlich klarer als eine nur implizit
lesbare Zustandslogik.

### 3. Testbarkeit und Regressionsschutz

Der Originalstand war stark funktionsgetrieben. Der aktuelle Stand hat heute:

- isolierte Unit-Tests für `Robot`
- Op-/State-Machine-Szenariotests
- WebSocket-/Telemetrie-Contract-Tests
- Frontend-Typecheck und Build

Die zuletzt eingezogenen Regressionstests für:

- GPS-Verlust
- Resume-Pfade
- Escape-Recovery
- Docking-Retry

sind ein echter qualitativer Fortschritt gegenüber dem Original.

### 4. WebUI und API

Das aktuelle System hat eine moderne, klar angebundene UI mit:

- Vue 3 + TypeScript
- Crow-HTTP/WS-Backend
- dokumentiertem Telemetrievertrag
- Diagnose-, Mapping-, Scheduler- und Settings-Oberfläche

Für Bedienbarkeit, Remote-Zugriff und langfristige Weiterentwicklung ist das
dem Original klar überlegen.

## Was Gegenüber Dem Original Noch Fehlt

### 1. Linux-Plattformdienste

Im Original waren im Alfred-/Linux-Paket zusätzlich vorhanden:

- BLE-Advertising / BLE-UART-Server
- Audio/TTS/Startsound
- Kamera-Streaming via `motion`
- CAN- und SLCAN-Fallback
- systemnahe Startlogik mit Linux-Service-Hooks

Im aktuellen aktiven `sunray-core` sind diese Bereiche entweder gar nicht
vorhanden oder nur als Randthema erwähnt.

Fehlend oder nicht aktiv umgesetzt:

- BLE/BlueZ-Stack
- Audio/TTS
- Kamera-Service
- CAN/SLCAN

### 2. NTRIP als echter Laufzeitpfad

Aktuell gibt es:

- Konfig-Felder in [`config.example.json`](/mnt/LappiDaten/Projekte/sunray-core/config.example.json)
- UI-Felder in [`webui/src/views/Settings.vue`](/mnt/LappiDaten/Projekte/sunray-core/webui/src/views/Settings.vue)
- RTCM-/DGPS-Alter im u-blox-Driver

Was fehlt:

- ein aktiver NTRIP-Client im Runtime-Startpfad
- Einspeisung der Korrekturdaten zum GPS-Empfänger aus dem Core heraus

Das heißt:

- Konfigurierbarkeit ist vorbereitet
- Laufzeitfunktionalität ist noch nicht vollständig vorhanden

### 3. Breitere Plattformparität

Das Original unterstützte mehrere Pfade:

- Arduino Due
- Adafruit GCM4
- ESP32
- Alfred/Linux
- CAN-/owlRobotics-Varianten

`sunray-core` ist bewusst Linux-first. Das ist architektonisch sinnvoll, aber
kein 1:1-Funktionsersatz für alle historischen Plattformen.

Praktisch aktiv today:

- Alfred per `SerialRobotDriver`
- Simulation

Geplant, aber offen:

- Pico-Driver

### 4. Alfred-Bedienpfade und einige Detailfunktionen

Im Original gehören zur Alfred-Gesamtfunktion nicht nur Fahr- und
Navigationslogik, sondern auch direkte Bedienpfade am Gerät.

Aktuell offen oder nur teilweise umgesetzt:

- physischer Stop-/Bedienbutton:
  - `stopButton` wird aus dem STM32-Frame gelesen
  - aber in `Robot` aktuell nicht ausgewertet
- Button-Hold-Aktionen des Originals fehlen:
  - Stop
  - Dock
  - Mow-Start
  - Shutdown
- Buzzer-Muster des Originals fehlen:
  - aktuell nur `setBuzzer(bool)`
  - keine strukturierten Beep-Sequenzen
- Motorstrom-/Überlast-Konfig existiert, wird im Core aber derzeit nicht
  sichtbar ausgewertet
- `NAV-VELNED` wird empfangen, aber Geschwindigkeit/Kurs werden derzeit nicht
  weiter genutzt

## Hardware-Einbindung: Ist Sie "Richtig"?

### Kurzantwort

Für die Kernfunktionen des Alfred-Roboters: weitgehend ja.

Für vollständige Hardwareparität mit der Originalplattform: noch nicht sicher.

### Was Heute Korrekt Eingebunden Ist

Im aktiven Alfred-Treiber vorhanden:

- UART zur STM32-MCU
  - `driver_port`, Standard `/dev/ttyS0`
- AT-Kommunikation für:
  - Motor-PWM
  - Encoder
  - Sensorstatus
  - Batteriesummary
  - Firmwareversion
- I2C-Bus `/dev/i2c-1`
- PCA9555-Portexpander:
  - EX1 `0x21`
  - EX2 `0x20`
  - EX3 `0x22`
- Panel-LEDs
- Buzzer
- Fan/IMU-Power-Schaltung
- MPU6050 als IMU
- WiFi-Status via `wpa_cli` für LED_1
- GPS via separatem u-blox-Driver
- Shutdown-Sequenz über `keepPowerOn(false)`

Damit ist die Grundlinie des Alfred-Betriebs fachlich klar abgebildet:

- Pi läuft Linux-Core
- STM32 liefert Motor-/Sensor-/Battery-Daten
- Pi übernimmt Mission, Navigation, Web/API und höhere Logik

Positiv hervorzuheben:

- die Panel-LED-Einbindung ist im heutigen Stand praktisch vollständig:
  - Startup grün
  - LED_2 für Status/Error
  - LED_3 für GPS
  - LED_1 für WiFi
  - Shutdown schaltet alle LEDs ab
- der Motor-Swap des Alfred-PCB-Fehlers ist im `SerialRobotDriver`
  ausdrücklich kompensiert

### Was Hardwareseitig Noch Unscharf Oder Unvollständig Ist

#### 1. TCA9548A-Multiplexer wird nicht sichtbar benutzt

Die Originalanalyse beschreibt einen Alfred-I2C-Multiplexer:

- TCA9548A auf `0x70`

Im heutigen aktiven Treiber sehe ich:

- Kommentarwissen über `0x70` in [`platform/I2C.h`](/mnt/LappiDaten/Projekte/sunray-core/platform/I2C.h)
- aber keine sichtbare Mux-Kanalwahl im aktiven `SerialRobotDriver` oder `Mpu6050Driver`

Risiko:

- Falls die echte Alfred-Revision den IMU-/ADC-/EEPROM-Zugriff nur über einen
  gewählten Mux-Kanal erlaubt, ist die aktuelle Einbindung unvollständig

Bewertung:

- auf manchen Revisionen kann es zufällig funktionieren
- als verifizierte vollständige Alfred-Hardwareparität reicht das noch nicht

#### 2. ADC/EEPROM-Pfad des Originals ist nicht sichtbar nachgebaut

Das Original nennt:

- MCP3421 ADC
- DG408 ADC-Mux
- BL24C256A EEPROM

Im aktiven `sunray-core` werden Batterie- und Sensorsummen primär über die
STM32-AT-Frames gelesen. Das ist für den aktuellen Kernbetrieb plausibel, aber
nicht dasselbe wie eine vollständige Linux-seitige Nachbildung aller
Original-Hardwarepfade.

Bewertung:

- für `sunray-core` nicht zwingend falsch
- aber es ist eine bewusste funktionale Reduktion gegenüber dem Original

#### 3. Der physische Stop-Button ist derzeit fachlich nicht eingebunden

Im aktuellen Code ist `SensorData.stopButton` vorhanden und wird vom
`SerialRobotDriver` befüllt.

Aber:

- `Robot::run()` bzw. seine Hilfsschritte reagieren aktuell nicht darauf
- es gibt keine Hold-Duration-Logik
- es gibt keine lokalen Mow-/Dock-/Shutdown-Aktionen über den Gerätetaster

Bewertung:

- hardwareseitig angeschlossen: ja
- fachlich als Bedienpfad implementiert: nein

Das ist gegenüber der Original-Alfred-Bedienlogik eine echte Lücke.

#### 4. BLE/CAN/Audio/Kamera sind hardwareseitig nicht nachgezogen

Diese Teile gehören im Original klar zur Gesamtplattform, im aktiven Core aber
nicht mehr zum echten Laufzeitumfang.

Bewertung:

- für den Mähkern nicht blockierend
- für volle Original-Plattformparität fehlend

## Was Fachlich Fehlt

Die wichtigsten inhaltlichen Lücken gegenüber dem Original oder seiner
Plattformbreite sind:

1. echte Pi-/Alfred-Hardwarevalidierung bleibt offen
2. vollständige Alfred-I2C-Topologie inklusive TCA9548A muss verifiziert werden
3. physischer Button ist funktional noch nicht implementiert
4. NTRIP-Laufzeitpfad fehlt
5. BLE/CAN/Audio/Kamera-Services fehlen
6. Pico-Driver ist noch offen
7. Battery-Fallback-Defaults in `Robot` liegen aktuell unter den
   Alfred-Standardwerten
8. Motorstrom-/Überlast-Konfig ist vorbereitet, aber im Core nicht sichtbar
   verdrahtet
9. `NAV-VELNED` wird empfangen, aber derzeit nicht genutzt

## Was Fachlich Heute Schon Besser Ist

Die wichtigsten echten Fortschritte sind:

1. klarer Linux-first Core statt Sketch-/Shim-Hybrid
2. explizites, getestetes Zustands- und Recovery-Modell
3. dokumentierter Telemetrievertrag
4. moderne WebUI mit Settings/Diagnostics/Map/Scheduler
5. deutlich stärkerer Regressionstest-Schutz

## Empfohlene Priorität

Wenn `sunray-core` die Originalplattform funktional wirklich ersetzen soll,
ist die sinnvollste Reihenfolge:

1. echte Alfred/Pi-Hardwarevalidierung abschließen
2. TCA9548A-/I2C-Topologie gegen reale Hardware prüfen
3. physische Button-Logik für Alfred ergänzen
4. Battery-Fallbacks im Core auf Alfred-Sollwerte anheben
5. NTRIP als echten Runtime-Client ergänzen
6. danach entscheiden, welche Originaldienste wirklich zurück sollen:
   - BLE
   - CAN
   - Audio
   - Kamera

## Präzisierte Einzelbefunde Aus Der Zusatzanalyse

Die zusätzliche Analyse in
[`SOLL_IST.md`](/mnt/LappiDaten/Projekte/sunray-core/SOLL_IST.md) bestätigt und
schärft einige Punkte:

- korrekt:
  - Panel-LEDs sind heute tatsächlich gut und weitgehend vollständig
    eingebunden
  - MPU6050 ist funktional sauber eingebunden
  - Motor-Swap-/Encoder-Swap-Kompensation ist vorhanden
  - `stopButton` wird gelesen, aber aktuell nicht verwendet
  - `NAV-VELNED` wird empfangen, aber im aktiven Verhalten nicht weiter genutzt
  - `Robot::checkBattery()` nutzt aktuell zu niedrige Fallback-Werte
- teilweise korrekt, aber einzuordnen:
  - "Alfred Custom-PCB vollständig":
    - für Grundbetrieb ja
    - für vollständige Original-Hardwareparität wegen TCA9548A/ADC/EEPROM noch
      nicht belastbar genug
  - "Motrostrom fehlt":
    - als Pi-seitige Fachlogik ja
    - nicht als Gesamtsicherheitssystem, weil `motorFault` aus dem MCU-Pfad
      bereits verwendet wird

## Konkrete Soll-Punkte Für Den Backlog

Aus dem Vergleich ergeben sich diese sinnvollen Backlog-Punkte:

1. Alfred-Button-Hold-Logic implementieren
2. Battery-Fallback-Defaults in [`core/Robot.cpp`](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) auf Alfred-Werte korrigieren
3. prüfen, ob Pi-seitiges Motorstrom-/Überlast-Monitoring überhaupt gewünscht
   ist oder bewusst im STM32 verbleibt
4. entscheiden, ob `gps_speed_detection` wirklich genutzt werden soll; falls ja,
   `NAV-VELNED` sauber verdrahten
5. TCA9548A-Mux-Nutzung gegen echte Alfred-Hardware verifizieren

## Endbewertung

`sunray-core` ist nicht einfach ein abgespeckter Port, sondern in mehreren
zentralen Punkten bereits die bessere Softwarebasis:

- sauberer aufgebaut
- besser testbar
- klarer dokumentiert
- UI-/API-seitig moderner

Aber:

Als vollständiger Ersatz der gesamten historischen Sunray/Alfred-Plattform ist
es noch nicht fertig.

Der wichtigste offene Punkt ist nicht mehr die Core-Architektur, sondern die
letzte Meile der echten Hardware- und Plattformparität.
