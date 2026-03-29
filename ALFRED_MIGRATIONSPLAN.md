# ALFRED_MIGRATIONSPLAN

Last updated: 2026-03-29

## Zweck

Dieses Dokument vergleicht den Alfred/Linux-Teil der Original-Sunray-Firmware aus
[SUNRAY_FIRMWARE_ANALYSE_2.md](/mnt/LappiDaten/Projekte/sunray-core/ALTE_DATEIEN/Sunray/SUNRAY_FIRMWARE_ANALYSE_2.md)
mit dem aktuellen aktiven Stand von `sunray-core` und leitet daraus eine
priorisierte Migrationsliste ab.

## Kurzfazit

`sunray-core` ist im Core heute klar besser:

- modernere Architektur
- sauberere Hardwaregrenze
- explizites Zustandsmodell
- deutlich stärkerer Test- und Telemetrievertrag
- bessere WebUI-/API-Struktur

Der Original-Alfred-Stack ist auf Plattformebene aber noch breiter:

- BLE unter Linux
- Audio/TTS und Startsound
- WPS-/Button-Bedienpfade
- CAN/SLCAN-Setup
- Kamera-/`motion`-Umgebung
- NTRIP als echter Plattformpfad

Die richtige strategische Schlussfolgerung ist deshalb nicht "alles portieren",
sondern:

1. sicherheits- und betriebsrelevante Alfred-Funktionen zuerst schließen
2. danach Hardwareparität absichern
3. Komfort- und Plattformextras zuletzt übernehmen

## Vergleich Auf Einen Blick

| Bereich | Original Alfred/Linux | `sunray-core` heute | Bewertung |
|---|---|---|---|
| Laufzeitarchitektur | Arduino-Sketch + Linux-Shims | Linux-native C++-Core mit klaren Modulen | `sunray-core` besser |
| Hardwareanbindung | Pi + STM32 + I2C-Peripherie eng gekoppelt | klarere HAL, Alfred aktiv unterstützt | `sunray-core` klarer |
| State Machine | funktional vorhanden | explizite Ops, Telemetrievertrag, Regressionsschutz | `sunray-core` besser |
| Tests | wenig isoliert | starker Unit-/Szenario-/Contract-Testbestand | `sunray-core` besser |
| Web / Bedienung | historischer HTTP-/Comm-Layer | Crow + WebSocket + Vue 3 | `sunray-core` besser |
| Gerätetaster | vollständig mit Hold-Durations | eingelesen, aber funktional offen | Original besser |
| Buzzer-UX | Pattern-basiert | aktuell primär Ein/Aus | Original besser |
| Linux-BLE | vorhanden | fehlt | Original besser |
| Linux-Audio/TTS | vorhanden | fehlt | Original besser |
| CAN / SLCAN | vorhanden | fehlt | Original besser |
| Kamera-/Motion-Umfeld | vorhanden | fehlt | Original besser |
| NTRIP-Laufzeitpfad | vorhanden/experimentell | Konfig vorbereitet, Runtime fehlt | Original besser |

## Was Nicht Portiert Werden Muss

Nicht alles aus dem Original ist automatisch Zielbild.

Bewusst wahrscheinlich nicht priorisieren:

- Root-zentrierte Startlogik aus `start_sunray.sh`
- monolithische Shell-gesteuerte Serviceumgebung
- historische Multi-Plattform-Pfade für Due/GCM4/ESP
- direkte 1:1-Übernahme des alten HTTP-/Comm-Protokolls

`sunray-core` sollte seine bessere Architektur behalten und nur die Alfred-
relevanten fachlichen und hardwarebezogenen Lücken schließen.

## Priorisierte Migrationsliste

## Priorität A: Betriebs- und Sicherheitslücken schließen

Diese Punkte betreffen reale Bedienbarkeit oder sichere Maschinenfunktion.

### A1. Physische Button-Hold-Logik portieren

Original vorhanden:

- 1s Stop/Idle
- 5s Dock
- 6s Start Mow
- 9s Shutdown
- 12s WPS
- akustisches Sekunden-Feedback

`sunray-core` heute:

- `stopButton` wird übertragen
- aber im `Robot` nicht ausgewertet

Warum Priorität A:

- ist direkt am Gerät relevant
- reduziert Abhängigkeit von WebUI bei Feldbetrieb
- ist sicherheits- und UX-relevant

Empfohlene Umsetzung:

- neue klar isolierte `tickButtonControl()`-Logik im `Robot`
- Hold-Duration als monotone Zeitbasis, nicht mit Sleeps
- Tests für 1s/5s/6s/9s-Pfade

### A2. Battery-Fallbacks korrigieren

`sunray-core` nutzt laut Bestand noch zu niedrige Fallback-Werte in
`Robot::checkBattery()` falls Konfig fehlt.

Soll:

- `battery_low_v = 25.5`
- `battery_critical_v = 18.9`

Warum Priorität A:

- kleiner Fix
- verhindert falsches Verhalten in Minimal- oder Fehlerkonfigurationen

### A3. Motorstrom-/Fault-Pfad fachlich entscheiden

Im Original existieren Motorstrom-/Fehlerpfade deutlich sichtbarer.
Im aktuellen Stand werden Stromwerte zwar aus Frames gewonnen, aber nicht als
vollwertige Pi-seitige Schutzlogik genutzt.

Entscheidung nötig:

- entweder bewusst weglassen und dokumentieren, dass MCU-only genügt
- oder Strom-/Überlastlogik im Core sauber ergänzen

Warum Priorität A:

- betrifft Schutz- und Diagnoseverhalten
- sollte bewusst entschieden werden, nicht implizit offen bleiben

## Priorität B: Alfred-Hardwareparität absichern

### B1. TCA9548A-/ADC-/EEPROM-Pfad auf echter Hardware verifizieren

Der Original-Alfred-Stack schaltet den I2C-Multiplexer explizit und bedient ADC
und EEPROM.

`sunray-core` heute:

- Port-Expander, LEDs, Buzzer, Fan, IMU sind sichtbar
- Multiplexer-/ADC-/EEPROM-Pfad ist nicht gleich deutlich belegt

Warum Priorität B:

- das ist der wichtigste Restpunkt bei der Frage "ist die Hardware richtig eingebunden?"
- ohne Hardwaretest bleibt nur Code-Nähe, keine echte Parität

Ergebnis soll sein:

- dokumentiert, welche Alfred-Hardware heute wirklich aktiv benutzt wird
- dokumentiert, was absichtlich entfällt

### B2. Alfred-Button, LEDs, Buzzer als Hardware-Abnahmepaket testen

Ein kleiner Hardware-Abnahmetest sollte zusammen prüfen:

- Button erkannt
- Buzzer aktiv
- LEDs korrekt
- Fan/IMU-Power korrekt

Warum Priorität B:

- sehr hoher Nutzwert
- geringe Komplexität
- macht reale Pi-/Alfred-Validierung reproduzierbar

### B3. `NAV-VELNED` und `gps_speed_detection` bereinigen

Originalpfad:

- GPS-Speed/Heading ist vorgesehen

`sunray-core` heute:

- Nachricht kommt an
- Nutzung ist unvollständig

Saubere Optionen:

- vollständig verdrahten
- oder Featureflag und tote Konfiguration entfernen

## Priorität C: Linux-Plattformfunktionen nachziehen

Diese Punkte sind funktional nützlich, aber nicht blockierend für sicheren
Grundbetrieb.

### C1. NTRIP-Client als echter Runtime-Pfad

Heute vorbereitet:

- Konfigfelder
- UI-Felder
- RTCM-/DGPS-Signale

Fehlt:

- TCP-Verbindung zum Caster
- GGA-/Credential-Handling
- Weiterleitung an GPS
- Fehler-/Statusdarstellung

Warum C1:

- fachlich sehr relevant für echten RTK-Betrieb
- aber komplexer als die A-/B-Punkte

### C2. BLE unter Linux

Original:

- eigener BlueZ-/GATT-UART-Server

`sunray-core`:

- aktuell kein entsprechender Stack

Empfehlung:

- nur übernehmen, wenn ein echter Bedien- oder Servicefall existiert
- sonst bewusst als nicht Zielbestand markieren

### C3. Audio/TTS

Original:

- D-Bus-Bridge
- MP3/TTS-Dateien
- Startsound

Empfehlung:

- wenn überhaupt, als lose Plattformintegration
- nicht in den Core ziehen

### C4. CAN / SLCAN

Original:

- Setup in `start_sunray.sh`

Empfehlung:

- nur portieren, wenn reale Hardware oder Folgeprojekt das benötigt
- sonst aus dem Zielbild streichen

### C5. Kamera-/`motion`-Umfeld

Original:

- service-seitig vorhanden

Empfehlung:

- getrennt vom Core betrachten
- eher optionales Deployment-Paket als Core-Feature

## Konkrete Empfehlung Für Die Nächsten Schritte

## Phase 1: Kleine harte Lücken schließen

1. Button-Hold-Logic implementieren und testen.
2. Battery-Fallback-Werte korrigieren.
3. Motorstrom-/Fault-Verhalten entscheiden und dokumentieren.

## Phase 2: Hardwareabnahme Alfred

1. echter Pi-/Alfred-Test für LEDs, Buzzer, Button, Fan, IMU
2. TCA9548A-/ADC-/EEPROM-Nutzung explizit prüfen
3. Ergebnis in Doku und Installationspfad festhalten

## Phase 3: RTK-/Linux-Plattformpfade

1. NTRIP priorisiert prüfen und ggf. implementieren
2. danach BLE oder Audio nur nach echtem Bedarf
3. CAN/Kamera nur bei nachgewiesenem Produktbedarf

## Empfehlung Für `TASKS.md`

Wenn der Backlog daran ausgerichtet werden soll, sollten die offenen Alfred-
Themen in dieser Reihenfolge geführt werden:

1. Button-Hold-Logic
2. Battery-Fallbacks
3. Motorstrom-/Fault-Entscheidung
4. Alfred-Hardwareabnahme inklusive Multiplexer/ADC/EEPROM
5. NTRIP
6. optionale Linux-Extras: BLE, Audio, CAN, Kamera

## Abschlussbewertung

`sunray-core` muss nicht zur alten Alfred-Plattform "zurückgebaut" werden.

Der richtige Weg ist:

- Core-Architektur behalten
- fehlende Maschinen- und Alfred-Bedienfunktionen gezielt ergänzen
- Hardwareparität auf echter Plattform belegen
- Linux-Extras nur nach echtem Nutzen übernehmen

Damit bleibt `sunray-core` technisch besser als das Original und schließt
trotzdem die für Alfred wirklich relevanten Lücken.
