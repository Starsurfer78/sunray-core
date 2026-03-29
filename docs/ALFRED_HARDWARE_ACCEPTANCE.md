# ALFRED_HARDWARE_ACCEPTANCE

Last updated: 2026-03-29

## Zweck

Diese Datei ist das Hardware-Abnahmepaket fuer Alfred auf Raspberry Pi.

Ziel:

- den aktiven `sunray-core`-Stand gegen die reale Alfred-Hardware pruefen
- explizit festhalten, welche Hardware heute im Code aktiv eingebunden ist
- klar markieren, was aktuell nur historisch dokumentiert, aber nicht aktiv verdrahtet ist

Diese Datei ersetzt keine echte Feldvalidierung. Sie macht sie nur reproduzierbar.

Automatisierbarer Vorcheck:

- [`scripts/check_alfred_hw.sh`](/mnt/LappiDaten/Projekte/sunray-core/scripts/check_alfred_hw.sh)

Aufruf auf dem Pi:

```bash
cd ~/sunray-core
bash scripts/check_alfred_hw.sh
```

Optional mit abweichender Konfig:

```bash
CONFIG_PATH=/pfad/zur/config.json bash scripts/check_alfred_hw.sh
```

Das Script ist absichtlich read-only:

- prueft Rechte, Device-Nodes und I2C-Sichtbarkeit
- schaltet aber keine LEDs, Motoren oder Multiplexer aktiv um

## Kurzfazit zum aktuellen Code-Stand

Im aktiven `sunray-core` klar sichtbar und runtime-relevant:

- UART zur Alfred-STM32
- PCA9555 EX1 `0x21`
- PCA9555 EX2 `0x20`
- PCA9555 EX3 `0x22`
- Panel-LEDs
- Buzzer
- Fan-Power
- IMU-Power
- MPU6050-Initialisierung direkt auf `/dev/i2c-1`

Aktuell nicht als aktiver Runtime-Pfad sichtbar:

- TCA9548A-Multiplexer `0x70`
- MCP3421-ADC `0x68`
- EEPROM `0x50`

Wichtige Folge:

- `Alfred-4` ist erst dann wirklich abgeschlossen, wenn die On-Hardware-Pruefung
  durchgefuehrt und hier dokumentiert ist.
- Aus Code-Sicht ist aktuell eher von "Port-Expander-/IMU-Paritaet" als von
  "vollstaendiger Original-I2C-Paritaet" zu sprechen.

## Aktive Hardwareanbindung im aktuellen Core

Quelle:

- [`hal/SerialRobotDriver/SerialRobotDriver.cpp`](/mnt/LappiDaten/Projekte/sunray-core/hal/SerialRobotDriver/SerialRobotDriver.cpp)
- [`platform/I2C.h`](/mnt/LappiDaten/Projekte/sunray-core/platform/I2C.h)
- [`platform/PortExpander.h`](/mnt/LappiDaten/Projekte/sunray-core/platform/PortExpander.h)

### Sicher aktiv

| Hardware | Adresse / Pfad | Status im Code | Erwartete Wirkung |
|---|---|---|---|
| STM32 Alfred MCU | `driver_port` default `/dev/ttyS0` | aktiv | AT-Frames, Odometry, Sensoren, Battery |
| PCA9555 EX1 | `0x21` | aktiv | IMU-Power IO1.6, Fan IO1.7 |
| PCA9555 EX2 | `0x20` | aktiv | Buzzer IO1.1 |
| PCA9555 EX3 | `0x22` | aktiv | LED1..LED3 |
| MPU6050 | direkt via `/dev/i2c-1` | aktiv | IMU-Init, IMU-Updates |

### Im Device-Map dokumentiert, aber derzeit nicht aktiv im Treiber genutzt

| Hardware | Adresse | Status im Code | Bemerkung |
|---|---|---|---|
| TCA9548A | `0x70` | nicht sichtbar genutzt | keine aktive Channel-Selektion im Treiber |
| MCP3421 ADC | `0x68` | nicht sichtbar genutzt | Batteriewerte kommen aktuell aus MCU-Frames |
| EEPROM | `0x50` | nicht sichtbar genutzt | kein aktiver Read/Write-Pfad im Treiber |

## Abnahme auf echter Hardware

## Voraussetzungen

- Raspberry Pi mit Alfred verbunden
- STM32-Firmware auf Alfred passend geflasht
- `sunray-core` auf dem Pi gebaut
- Zugriff auf:
  - `driver_port` typischerweise `/dev/ttyS0`
  - `i2c_bus` typischerweise `/dev/i2c-1`
- Benutzer hat Zugriff auf `dialout` und `i2c`

Empfohlene Vorpruefung:

```bash
groups
ls -l /dev/ttyS0
ls -l /dev/i2c-1
```

## 1. UART-/MCU-Abnahme

Ziel:

- der Pi bekommt AT-Frames
- Odometry, Sensoren und Battery aktualisieren sich

Pruefung:

1. `sunray-core` starten.
2. auf Logs achten:
   - UART erfolgreich geoeffnet
   - MCU antwortet
   - Firmware-Version kommt an
3. in der WebUI oder Telemetrie pruefen:
   - Batteriespannung ist plausibel
   - `mcu_v` ist gesetzt
   - Sensorwerte veraendern sich bei realer Betaetigung

Pass-Kriterien:

- keine dauerhafte Meldung "MCU not responding"
- keine dauerhaft auf 0 eingefrorene Odometry
- Sensor- und Batteriesnapshots veraendern sich plausibel

## 2. Panel-LED-Abnahme

Ziel:

- EX3 funktioniert
- LED-Pinzuordnung stimmt

Codebasis:

- LED_1 = WiFi
- LED_2 = Status/Error
- LED_3 = GPS

Erwartetes Verhalten:

- beim Init alle drei LEDs kurz gruen
- LED_2:
  - gruen im Normalbetrieb
  - rot in `Error`
- LED_3:
  - gruen bei RTK Fix
  - rot bei Float
  - aus ohne GPS
- LED_1:
  - gruen bei `wpa_state=COMPLETED`
  - rot bei `INACTIVE`
  - aus sonst

Pass-Kriterien:

- alle drei LEDs sind ansteuerbar
- Farben stimmen mit den Rollen ueberein
- kein vertauschtes Mapping

## 3. Buzzer-Abnahme

Ziel:

- EX2 funktioniert
- Buzzer laesst sich vom Core aus ansteuern

Pruefpfade im aktuellen Stand:

- kritische Batterie
- ErrorOp-Blink-/Alarmverhalten
- Button-Hold-Feedback jede Sekunde

Minimaltest:

1. Stop-Button auf Alfred gedrueckt halten
2. nach jeweils ca. 1 s sollte ein kurzer Beep kommen

Pass-Kriterien:

- Buzzer hoerbar
- Sekunden-Feedback bei Button-Hold vorhanden

## 4. Button-Abnahme

Ziel:

- physischer Button wird aus STM32-Frame gelesen
- neue Hold-Logic funktioniert auf echter Hardware

Aktuell implementiert:

- `>=1s` Stop / Idle
- `>=5s` Dock
- `>=6s` Mow
- `>=9s` Shutdown

Noch nicht wiederhergestellt:

- `3s` R/C-Sonderpfad
- `12s` WPS

Pruefung:

1. in `Mow`: Button kurz gedrueckt halten
   Erwartung: Stop / `Idle`
2. in `Idle`: 5 s halten und loslassen
   Erwartung: `Dock`
3. in `Idle`: 6 s halten und loslassen
   Erwartung: `NavToStart` bzw. Start der Mission
4. in `Idle`: 9 s halten und loslassen
   Erwartung: Shutdown-Sequenz wird angestossen

Pass-Kriterien:

- alle vier Pfade reagieren reproduzierbar

## 5. Fan-Abnahme

Ziel:

- EX1 IO1.7 schaltet den Luefter

Aktueller Code:

- Fan beim Init an
- spaeter temperaturabhaengig:
  - aus unter 60 C
  - an ueber 65 C

Pruefung:

1. Start beobachten: Fan sollte anlaufen
2. laengeren Lauf oder kuenstliche Temperaturerhoehung simulieren
3. optional Logausgaben oder Stromaufnahme pruefen

Pass-Kriterien:

- Fan schaltet physisch
- keine invertierte Logik

## 6. IMU-Abnahme

Ziel:

- EX1 IO1.6 versorgt die IMU
- MPU6050 antwortet
- IMU-Werte sind plausibel

Wichtiger Stand heute:

- `sunray-core` spricht die IMU direkt auf `/dev/i2c-1` an
- eine explizite TCA9548A-Selektion ist derzeit nicht im aktiven Treiber sichtbar

Das muss auf echter Hardware beantwortet werden:

1. ist die IMU auf Alfred ohne Mux-Umschaltung direkt erreichbar?
2. oder funktioniert der aktuelle Stand nur auf einer bestimmten Board-Revision?

Pruefung:

1. Robot starten
2. auf Logs achten:
   - kein `IMU init failed`
3. Lage des Roboters veraendern
4. Telemetrie fuer `imu_h`, `imu_r`, `imu_p` beobachten

Pass-Kriterien:

- IMU initialisiert
- Roll/Pitch reagieren plausibel

Offene Architekturfrage:

- falls die IMU nur ueber TCA9548A erreichbar ist, muss der aktive Treiber noch
  um Mux-Selektion erweitert werden

## 7. TCA9548A-/ADC-/EEPROM-Abnahme

Das ist der derzeit wichtigste Restblock.

### TCA9548A

Soll Original:

- aktive Busselektion fuer IMU / ADC / EEPROM

Ist aktueller Treiber:

- keine sichtbare Runtime-Nutzung

Pruefung:

```bash
i2cdetect -y 1
```

Erwartung:

- `0x70` sichtbar, falls TCA9548A bestueckt

Bewertung:

- sichtbar allein reicht nicht
- entscheidend ist, ob ohne explizite Channel-Selektion alle benoetigten
  Geraete erreichbar sind

### ADC

Soll Original:

- ADC-Messung auf Linux-Seite moeglich

Ist aktueller Treiber:

- keine aktive Nutzung im `SerialRobotDriver`

Pruefung:

```bash
i2cdetect -y 1
```

Erwartung:

- `0x68` sichtbar, falls MCP3421 bestueckt

Bewertung:

- wenn vorhanden, aber ungenutzt: dokumentieren als "bestueckt, aktuell nicht
  vom Core verwendet"
- wenn fuer echte Alfred-Funktion noetig: Folgearbeit am Treiber

### EEPROM

Soll Original:

- EEPROM grundsaetzlich erreichbar

Ist aktueller Treiber:

- keine aktive Nutzung

Pruefung:

```bash
i2cdetect -y 1
```

Erwartung:

- `0x50` sichtbar, falls EEPROM bestueckt

Bewertung:

- aktuell eher Hardware-Paritaetscheck als Produktblocker

## Dokumentationspflicht nach echter Abnahme

Nach Hardwarelauf diese Tabelle ausfuellen:

| Pruefpunkt | Ergebnis | Notiz |
|---|---|---|
| UART/MCU | offen | |
| Panel-LEDs | offen | |
| Buzzer | offen | |
| Button-Hold | offen | |
| Fan | offen | |
| IMU | offen | |
| TCA9548A sichtbar | offen | |
| ADC sichtbar | offen | |
| EEPROM sichtbar | offen | |

Danach Abschlussbewertung:

- `Alfred-4` abgeschlossen
- oder:
  - Multiplexer nachziehen
  - ADC bewusst ungenutzt belassen
  - EEPROM bewusst ungenutzt belassen

## Empfehlung fuer den echten Hardwaretermin

Abnahmereihenfolge:

1. UART/MCU
2. LEDs
3. Button + Buzzer
4. Fan
5. IMU
6. I2C-Bestandsaufnahme mit `i2cdetect`
7. Entscheidung zu TCA9548A/ADC/EEPROM dokumentieren

Damit wird zuerst geprueft, was den realen Betrieb direkt blockiert, und erst
dann die tiefere Original-Hardwareparitaet.
