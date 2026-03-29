# Alfred Pi Switchover Guide

Stand: 2026-03-29

Ziel dieser Anleitung ist ein sicherer, nachvollziehbarer erster Test von
`sunray-core` auf echter Alfred-Hardware, ohne den aktuell laufenden
Originalstand unkontrolliert zu verlieren.

Die Anleitung folgt vier Schritten:

1. Originalstand dokumentieren
2. `sunray-core` parallel vorbereiten
3. `sunray-core` gezielt manuell starten
4. UART, IMU, LEDs, Button und Buzzer vergleichen

Diese Datei ist bewusst operativ gehalten. Die eigentliche Hardware-Abnahme
steht in
[`docs/ALFRED_HARDWARE_ACCEPTANCE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_HARDWARE_ACCEPTANCE.md).

## Voraussetzungen

- Raspberry Pi im Alfred ist erreichbar
- der bisherige Originalstand funktioniert noch
- das Repo liegt auf dem Pi unter `~/sunray-core`
- der Originalstand liegt auf dem Pi parallel unter `~/Sunray`
- die Alfred-Firmware auf dem STM32 ist vorhanden
- der Benutzer ist mindestens in `dialout` und `i2c`

Vorab sinnvoll:

```bash
cd ~/sunray-core
bash scripts/check_alfred_hw.sh
```

Wenn dieser Check grundlegende Bus- oder Rechteprobleme meldet, erst diese
beheben und noch nicht auf `sunray-core` umstellen.

## 1. Originalstand dokumentieren

Bevor etwas gestoppt oder ersetzt wird, sollte der laufende Zustand einmal
sichtbar gesichert werden.

Empfohlene Kommandos:

```bash
systemctl list-units --type=service | grep -i sunray
systemctl status sunray --no-pager
ps aux | grep -i sunray
ls -l /etc/systemd/system /lib/systemd/system | grep -i sunray
ls -l /etc/sunray
```

Zusätzlich sinnvoll:

```bash
journalctl -u sunray -n 100 --no-pager
```

Dokumentieren:

- Name und Pfad des aktuell laufenden Dienstes
- welche Binary oder welches Script gestartet wird
- welche Config-Dateien verwendet werden
- ob zusätzliche Startskripte, Cronjobs oder lokale Anpassungen existieren

Aktueller bekannter Ist-Stand auf Alfred:

- Dienstname: `sunray.service`
- Service-Datei: `/etc/systemd/system/sunray.service`
- Startscript: `/home/Sascha/Sunray/alfred/start_sunray.sh`
- Binary: `/home/Sascha/Sunray/alfred/build/sunray`
- Zusatzprozess: `dbus_monitor.sh` bzw. `dbus-monitor`
- es gibt aktuell kein `/etc/sunray`

Wenn möglich, den aktuellen Diensttext sichern:

```bash
systemctl cat sunray
```

Das ist der wichtigste Rollback-Anker, falls der Test mit `sunray-core`
abgebrochen werden muss.

## 2. `sunray-core` parallel vorbereiten

`sunray-core` sollte vorbereitet werden, ohne den Originaldienst sofort zu
ersetzen.

Repo auf den Pi bringen:

```bash
cd ~
mkdir -p ~/sunray-core
```

Wenn das Repo nicht per Git geklont werden kann, den lokalen Stand per `rsync`
oder `scp` kopieren.

Wichtig:

- `~/Sunray` ist der bestehende Originalstand
- `~/sunray-core` ist der neue Teststand
- beide Verzeichnisse liegen bewusst parallel im selben Home-Verzeichnis und
  sollen nicht vermischt werden

Dann auf dem Pi:

```bash
cd ~/sunray-core
cmake -S . -B build_pi
cmake --build build_pi -j2
```

Falls noch keine Laufzeit-Config existiert:

```bash
sudo mkdir -p /etc/sunray-core
sudo cp config.example.json /etc/sunray-core/config.json
sudo touch /etc/sunray-core/map.json
sudo chown -R "$USER":"$USER" /etc/sunray-core
```

Warum nicht `/etc/sunray`:

- der Originalstand heisst `sunray.service`
- auch wenn dort aktuell kein `/etc/sunray` genutzt wird, sollte `sunray-core`
  bewusst einen getrennten Config-Pfad behalten
- das reduziert Verwechslungs- und Rollback-Risiken deutlich

Danach die wichtigsten Alfred-relevanten Config-Werte pruefen:

- `driver_port`
- `i2c_bus`
- `gps_port`
- Batteriewerte
- eventuell Web-/API-Ports, falls bereits belegt

Noch nicht tun:

- keinen bestehenden Originaldienst ueberschreiben
- keinen automatischen Autostart auf `sunray-core` umbiegen
- keine bestehende Service-Datei still ersetzen
- nicht `~/Sunray` und `~/sunray-core` vermischen

## 3. `sunray-core` gezielt manuell starten

Der erste Testlauf sollte bewusst manuell und im Vordergrund passieren. So sind
 Logs direkt sichtbar und ein Abbruch ist jederzeit moeglich.

Zuerst den Originaldienst kontrolliert stoppen:

```bash
sudo systemctl stop sunray
```

Falls der Dienst anders heisst, hier natuerlich den echten Servicenamen
verwenden.

Dann `sunray-core` direkt starten:

```bash
cd ~/sunray-core
./build_pi/sunray-core
```

Wenn das Projekt mit Argumenten oder abweichender Config gestartet werden soll,
den echten Aufruf aus der aktuellen Service-Datei uebernehmen.

Fuer den ersten Testlauf gilt:

- Originaldienst vorher mit `sudo systemctl stop sunray` anhalten
- `sunray-core` bewusst nur manuell starten
- dabei die getrennte Test-Config unter `/etc/sunray-core` verwenden
- keinen neuen `sunray.service` ueber den Originaldienst legen

Waehren des Starts besonders auf Folgendes achten:

- oeffnet `/dev/ttyS0` sauber
- erkennt GPS und UART ohne Endlosschleife aus Fehlermeldungen
- findet I2C-Geraete ohne sofortige Exceptions
- startet WebSocket/HTTP sauber
- laeuft ohne sofort in `Error` oder Prozessabbruch zu gehen

Parallele Beobachtung in zweitem Terminal:

```bash
journalctl -f
```

Wenn `sunray-core` nicht sauber hochkommt:

```bash
sudo systemctl start sunray
```

Damit ist der Rollback sofort moeglich.

## 4. UART, IMU, LEDs, Button und Buzzer vergleichen

Der wichtigste Punkt ist jetzt nicht nur "laeuft der Prozess", sondern ob die
Alfred-spezifischen Runtime-Funktionen auf echter Hardware gleich oder
mindestens sinnvoll reagieren.

### UART / MCU

Pruefen:

- kommen regelmaessig Sensordaten an
- aendern sich Encoder-/Sensorwerte plausibel
- gibt es Frame- oder CRC-Fehler
- reagiert der Pi weiter auf `motorFault`, `lift`, `bumper`, `stopButton`

Warnzeichen:

- wiederkehrende Timeouts
- dauernd "no frame" oder Parse-Fehler
- feststehende Sensordaten trotz aktiver MCU

### IMU

Pruefen:

- wird die IMU erkannt
- kommen Roll/Pitch/Yaw-Daten
- aendern sich die Werte bei Bewegung plausibel
- gibt es Hinweise auf falsche Adresse oder falschen Buspfad

Hinweis:

Da beim Hardware-Scan sowohl `0x68` als auch `0x69` sichtbar waren, sollte bei
IMU-Problemen besonders auf Adress- oder Mux-Themen geachtet werden.

### LEDs

Pruefen:

- Startzustand sichtbar
- GPS-/WiFi-/Op-Status reagieren sichtbar
- keine dauerhaft falsche Farbe oder tote LED

### Button

Pruefen:

- kurzer Halt fuehrt zu Stop/Idle
- laengerer Halt fuehrt zu Dock oder Mow entsprechend der implementierten
  Schwellen
- Shutdown-Pfad nur bewusst testen

### Buzzer

Pruefen:

- Sekunden-Beep beim Button-Halten
- Fehler-/Warnverhalten wie erwartet
- kein dauerhaftes Haengenbleiben

## Bewertung nach dem Test

Nach dem ersten Lauf den Stand in drei Klassen einordnen:

### A. Startet und Kernhardware reagiert

Dann kann `sunray-core` als echter Alfred-Kandidat weiter getestet werden.

Naechste Schritte:

- Ergebnisse in
  [`docs/ALFRED_HARDWARE_ACCEPTANCE.md`](/mnt/LappiDaten/Projekte/sunray-core/docs/ALFRED_HARDWARE_ACCEPTANCE.md)
  uebernehmen
- offene Abweichungen notieren
- erst danach ueber Autostart oder dauerhaften Wechsel sprechen

### B. Startet, aber einzelne Alfred-Funktionen fehlen

Dann nicht sofort produktiv umstellen.

Typische Kandidaten:

- IMU-Pfad falsch
- LED-Zuordnung unvollstaendig
- Button/Buzzer nur teilweise korrekt
- Mux-/ADC-/EEPROM-Pfade noch nicht genutzt

Das ist ein guter Zwischenstand fuer gezielte Treiberarbeit.

### C. Startet nicht sauber

Dann sofort zum Originaldienst zurueck:

```bash
sudo systemctl start sunray
```

Anschliessend Logs sichern und die Ursache separat beheben, bevor erneut
umgestellt wird.

## Nicht sofort tun

- keinen automatischen `systemd`-Autostart auf `sunray-core` aktivieren, bevor
  der manuelle Testlauf sauber war
- die Original-Unit nicht loeschen
- die Original-Config nicht ohne Backup ueberschreiben
- mehrere Dinge gleichzeitig aendern

## Ergebnisdokumentation

Nach dem Test sollten mindestens diese Punkte kurz festgehalten werden:

- startet `sunray-core` auf Alfred manuell stabil: ja/nein
- UART ok: ja/nein
- IMU ok: ja/nein
- LEDs ok: ja/nein
- Button ok: ja/nein
- Buzzer ok: ja/nein
- Auffaelligkeiten
- Rollback noetig: ja/nein

Danach kann entschieden werden, ob als naechster Schritt

- nur gezielte Treiberarbeit noetig ist
- oder ein kontrollierter Wechsel des Autostarts sinnvoll wird
