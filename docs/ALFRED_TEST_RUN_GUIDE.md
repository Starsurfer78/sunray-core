# Alfred Test Run Guide

Diese Anleitung beschreibt den aktuellen Testablauf auf dem echten Roboter:
Original-Sunray sauber stoppen, `sunray-core` manuell starten, WebUI testen und
bei Bedarf schnell zur Originalsoftware zurueckkehren.

## 1. Auf dem Pi einloggen

```bash
ssh <pi-user>@<pi-ip>
```

## 2. Repo aktualisieren

```bash
cd ~/sunray-core
git pull origin master
git log --oneline -1
```

Erwartet wird aktuell:

```text
5e6e268 Improve map capture UX and dashboard preflight
```

## 3. Laufenden Originaldienst pruefen

```bash
systemctl list-units --type=service | grep -i sunray
systemctl status sunray --no-pager
systemctl cat sunray
```

Falls der Originaldienst anders heisst, in den folgenden Befehlen den echten
Servicenamen verwenden.

## 4. Originalzustand kurz sichern

```bash
journalctl -u sunray -n 100 --no-pager
ps aux | grep -i sunray
```

## 5. Optionaler Hardware-Check

```bash
cd ~/sunray-core
bash scripts/check_alfred_hw.sh
```

Wenn hier bereits UART-, I2C- oder Rechteprobleme auftauchen, diese zuerst
beheben.

## 6. Fehlende Pakete nachinstallieren

Fuer den aktuellen Teststand sollten mindestens die SQLite-Pakete vorhanden
sein, damit History/Events nicht im Stub-Modus laufen:

```bash
sudo apt update
sudo apt install -y sqlite3 libsqlite3-dev
```

Wenn Node.js / npm auf dem Pi noch fehlen, diese ebenfalls zuerst installieren.

## 7. `sunray-core` und WebUI bauen

```bash
cd ~/sunray-core
cmake -S . -B build_pi
cmake --build build_pi -j2
```

Die WebUI wird **nicht** automatisch durch den manuellen Binary-Start gebaut.
Darum zusaetzlich:

```bash
cd ~/sunray-core/webui-svelte
npm install
npm run build
```

Damit liegt die statische WebUI danach unter `~/sunray-core/webui-svelte/dist`.

Wichtig:

- Fuer den normalen Robotertest wird die WebUI **nicht separat gestartet**.
- Die gebaute `dist/`-WebUI wird spaeter direkt vom laufenden
  `sunray-core`-Prozess ausgeliefert.
- Ein eigener Frontend-Dev-Server ist nur fuer UI-Entwicklung noetig, nicht
  fuer den Feldtest auf dem Roboter.

Optional nur fuer Frontend-Entwicklung:

```bash
cd ~/sunray-core/webui-svelte
npm run dev -- --host
```

## 8. Konfiguration pruefen

Wenn die Laufzeit-Config schon existiert:

```bash
ls -l /etc/sunray-core
sed -n '1,240p' /etc/sunray-core/config.json
```

Wenn sie noch nicht existiert:

```bash
sudo mkdir -p /etc/sunray-core
sudo cp config.example.json /etc/sunray-core/config.json
sudo touch /etc/sunray-core/map.json
sudo chown -R "$USER":"$USER" /etc/sunray-core
```

Wichtige Werte pruefen:

- `driver_port`
- `gps_port`
- `i2c_bus`
- `ex3_mux_channel`
- `imu_mux_channel`
- `charger_connected_voltage_v`

Fuer Alfred wichtig:

- `ex3_mux_channel = 0`
- `imu_mux_channel = 4`
- `charger_connected_voltage_v = 7.0`

## 9. Original-Sunray sauber stoppen

```bash
sudo systemctl stop sunray
systemctl status sunray --no-pager
```

## 10. Sicherstellen, dass nichts Altes mehr laeuft

```bash
ps aux | grep -i sunray
```

Wenn dort noch der alte Prozess aus dem Originalprojekt laeuft, erst klaeren,
bevor `sunray-core` gestartet wird.

## 11. `sunray-core` manuell starten

```bash
cd ~/sunray-core
./build_pi/sunray-core /etc/sunray-core/config.json
```

Worauf achten:

- kein sofortiger Absturz
- UART wird geoeffnet
- GPS kommt hoch
- I2C/IMU verursachen keinen harten Fehler
- WebUI ist erreichbar

Falls du lieber den Komplettweg gehen willst, baut das Install-Skript sowohl den
Core als auch die WebUI und installiert die benoetigten Pakete:

```bash
cd ~/sunray-core
./scripts/install_sunray.sh --no-start
```

Fuer den ersten echten Robotertest bleibt der manuelle Vordergrundstart aber der
sicherere Weg.

## 12. Parallel Logs beobachten

In einem zweiten Terminal:

```bash
journalctl -f
```

## 13. WebUI testen

Im Browser:

```text
http://<pi-ip>:8765
```

Falls in der Config bei `ws_port` ein anderer Port gesetzt ist, diesen verwenden.

Praktisch testen:

- Dashboard: Startfreigabe, Blocker, Hinweise
- Karte: Mapping-Assistent
- `+`-Button speichert aktuelle Roboterposition
- NoGo anlegen, abschliessen, auswaehlen, loeschen
- Docking-Pfad aufnehmen

## 14. Sofortiger Rollback, falls etwas nicht sauber laeuft

`sunray-core` mit `Ctrl+C` beenden und dann:

```bash
sudo systemctl start sunray
systemctl status sunray --no-pager
```

## 15. Wichtige Regel fuer diesen Teststand

- Noch **keinen** Autostart auf `sunray-core` umstellen
- Zuerst nur manuell im Vordergrund testen
- Rollback immer ueber den Originaldienst



Gedächnisstütze:

cd ~/sunray-core
git pull origin master
cd webui-svelte
npm install
npm run build
cd ..
cmake -S . -B build_pi
cmake --build build_pi -j2
pkill -9 -f './build_pi/sunray-core'
nohup ./build_pi/sunray-core /etc/sunray-core/config.json > /tmp/sunray-core.log 2>&1 &
tail -f /tmp/sunray-core.log


nohup ./build_pi/sunray-core /tmp/sunray-config-test.json > /tmp/sunray-core.log 2>&1 &
tail -f /tmp/sunray-core.log