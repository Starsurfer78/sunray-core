# Robot Button, Buzzer, Error

Diese Uebersicht beschreibt den aktuellen Ist-Zustand fuer

- den Stop-/Start-Button am Roboter
- die Buzzer-Signale
- den `Error`-Zustand

Die Datei ist bewusst praxisnah gehalten und soll bei Betrieb, Fehlersuche und spaeteren Anpassungen als schnelle Referenz dienen.

## Kurzfassung fuer den Betrieb

Wenn es schnell gehen soll, reichen meist diese Punkte:

| Situation | Bedeutung / Aktion |
|---|---|
| Button ca. `6 s` halten und loslassen | Maehen starten |
| Button ca. `5 s` halten und loslassen | Docking starten |
| Button ca. `1 s` halten und loslassen | Not-Stopp / zurueck nach `Idle` |
| Button ca. `9 s` halten und loslassen | Shutdown |
| einzelner kurzer Piep pro Sekunde beim Halten | Button-Hold wird gezaehlt |
| Doppel-Piep | Start wurde abgewiesen, z. B. keine Karte / keine Mähpunkte |
| LED rot + periodischer Alarm | Roboter ist in `Error` |
| Aus `Error` heraus | Ursache beheben, dann `stop` / `Idle` quittieren |

Empfohlene Praxis im Feld:

1. Bei `Error` zuerst die Ursache suchen, nicht nur quittieren.
2. Wenn Lift, Motorfehler oder Dockproblem weiter anliegen, kommt `Error` sofort wieder.
3. Wenn `Start` abgewiesen wird, zuerst Karte und Mähpunkte pruefen.

## Geltungsbereich

Die beschriebenen Verhaltensweisen kommen aktuell aus diesen Stellen:

- `core/Robot.cpp`
- `core/ButtonActions.cpp`
- `core/BuzzerSequencer.cpp`
- `core/op/ErrorOp.cpp`

## Button

Der physische Stop-Button wird im Core ausgewertet. Die eigentliche Zuordnung von Haltedauer zu Aktion liegt in `ButtonActions.cpp`.

Aktuelle Schaltschwellen:

| Haltedauer | Aktion |
|---|---|
| `< 1 s` | keine Aktion |
| `>= 1 s` und `< 5 s` | `EmergencyStop` |
| `>= 5 s` und `< 6 s` | `StartDocking` |
| `>= 6 s` und `< 9 s` | `StartMowing` |
| `>= 9 s` | `Shutdown` |

Wichtige Details:

- Die Aktion wird beim Loslassen ausgewertet.
- Die Hold-Aktionen `Dock`, `Start` und `Shutdown` sind nur in `Idle` oder `Charge` vorgesehen.
- Wenn der Button in anderen Zustaenden neu gedrueckt wird, fuehrt das sofort zu `emergencyStop()`.

## Buzzer

Die Buzzer-Muster sind zentral im `BuzzerSequencer` definiert. `Robot.cpp` ruft nur noch ein benanntes Muster auf.

Aktuelle Muster:

| Pattern | Bedeutung | Aktuelles Muster |
|---|---|---|
| `ButtonTick` | Sekunden-Feedback waehrend der Button gehalten wird | 100 ms an |
| `StartRejected` | Start wurde abgewiesen, z. B. keine Karte | 120 ms an, 120 ms aus, 120 ms an |
| `StartMowingAccepted` | Button-Start fuer Maehen erkannt | 90 ms an, 60 ms aus, 160 ms an |
| `StartDockingAccepted` | Button-Start fuer Dock erkannt | 180 ms an |
| `ShutdownRequested` | langer Shutdown-Hold erkannt | 220 ms an, 80 ms aus, 220 ms an |
| `EmergencyStop` | kurzer Hold fuehrt zu Not-Stopp | 250 ms an |

Zusaetzlich gibt es einen getrennten periodischen Fehleralarm im `Error`-Zustand:

- Start nach 1 s im `Error`
- dann alle 5 s ein kurzer Alarmton
- aktuell 500 ms an, Rest aus

Der Startup-Buzzer des Hardwaretreibers ist davon getrennt und liegt im `SerialRobotDriver`.

## Startablehnung

Ein Mähstart wird aktuell vor dem eigentlichen Missionsbeginn abgewiesen, wenn:

- keine aktive Karte geladen ist
- die Karte keine Mähpunkte enthaelt

Verhalten bei Ablehnung:

- kein Wechsel nach `Undock`
- kein Wechsel nach `NavToStart`
- Warnung im Log
- temporäre UI-Meldung
- Buzzer-Muster `StartRejected`

## Error

`Error` ist der harte Sicherheitszustand.

Verhalten im `Error`:

- Motoren bleiben gestoppt
- Status-LED wird rot
- periodischer Buzzer-Alarm
- der Roboter bleibt in `Error`, bis der Operator quittiert

### Typische Uebergaenge nach `Error`

Der Roboter geht in `Error`, wenn ein Zustand einen Fehler als nicht sicher recoverbar behandelt.

Typische Ausloeser:

| Zustand | Ausloeser |
|---|---|
| `Mow` | Lift, Motorfehler, IMU-Tilt, IMU-Fehler |
| `NavToStart` | Lift, Motorfehler, IMU-Tilt, IMU-Fehler |
| `Undock` | Hindernis, Lift, Motorfehler, Ladegeraet bleibt verbunden, Positions-Timeout |
| `Dock` | Lift, Motorfehler, GPS-Fix-Timeout, Dock-Route fehlgeschlagen, Dock-Kontakt nach Retries fehlgeschlagen |
| `Charge` | wiederholter schlechter Ladekontakt, Unterspannung waehrend Laden |
| `EscapeReverse` / `EscapeForward` | verschiedene Folgezustaende nach fehlgeschlagener Recovery, z. B. Lift oder IMU-Fehler |

Nicht jeder Stoerfall geht direkt in `Error`.

Beispiele fuer andere Reaktionen:

- GPS-Verlust kann zuerst nach `GpsWait` fuehren
- Hindernisse fuehren oft zuerst nach `EscapeReverse`
- niedriger Akku fuehrt typischerweise Richtung `Dock`

## Aus Error heraus

Aktuell gibt es keinen automatischen Reset aus `Error`.

Moegliche Rueckwege:

- WebUI- oder API-Kommando `stop`
- Operator-Kommando `Idle`
- Stop-Button, wenn dadurch `emergencyStop()` nach `Idle` ausgeloest wird

Wichtig:

- Erst die Ursache beheben, dann quittieren.
- Wenn die Ursache weiter anliegt, geht der Roboter sofort wieder in `Error`.

Empfohlene Reihenfolge im Feld:

1. Ursache pruefen und beheben
2. `Error` quittieren
3. auf `Idle` achten
4. neu starten

## UI / Telemetrie

Fuer den Bediener sind aktuell diese Dinge relevant:

- `state_phase = "fault"` bei `Error`
- `event_reason` fuer dominante Ursache bzw. Rueckmeldegrund
- `error_code` fuer wichtige Fehlerkennungen
- `ui_message` und `ui_severity` fuer temporaere Hinweise, z. B. Startablehnung

## Spaetere sinnvolle Erweiterungen

- Button-Schwellen aus `config.json`
- konfigurierbare Buzzer-Muster
- explizite Fehlerquittierung statt indirekt ueber `Idle`
- eigene Dokumentation fuer LED-Signale
- Tabelle `event_reason`/`error_code` mit Operator-Bedeutung
