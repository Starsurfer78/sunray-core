# Alfred Production Switchover

Stand: 2026-04-02

Diese Anleitung beschreibt den Umstieg von der alten `sunray`-Runtime auf
`sunray-core` als neue Standardsoftware auf Alfred.

Ziel:

- `sunray-core` wird der aktive Produktivdienst
- `sunray` bleibt als Rollback-Anker installiert
- kein staendiges Hin-und-her-Wechseln im Alltag

## Voraussetzungen

- Repo liegt auf dem Pi unter `~/sunray-core`
- `/etc/sunray-core/config.json` und `/etc/sunray-core/map.json` sind vorhanden
- der bisherige Dienst `sunray` laeuft noch sauber
- die STM32-Firmware ist auf dem erwarteten Alfred-Stand

Vor dem Umstieg:

```bash
cd ~/sunray-core
git pull origin master
bash scripts/check_alfred_hw.sh
bash scripts/check_deploy_state.sh
```

Wenn hier UART-, I2C-, Rechte- oder Config-Probleme auftauchen, den
Produktivwechsel noch nicht vollziehen.

## 1. Alten Dienst dokumentieren

```bash
systemctl status sunray --no-pager
systemctl cat sunray
journalctl -u sunray -n 100 --no-pager
```

Damit bleibt klar, wie der bisherige Rollback-Anker gestartet wurde.

## 2. Alten Dienst stilllegen, aber behalten

```bash
sudo systemctl stop sunray
sudo systemctl disable sunray
```

Wichtig:

- nicht loeschen
- nicht deinstallieren
- nur deaktivieren

## 3. `sunray-core` produktiv installieren

```bash
cd ~/sunray-core
bash scripts/install_sunray.sh --autostart yes
```

Falls Root direkt benoetigt wird:

```bash
sudo bash scripts/install_sunray.sh --autostart yes
```

Danach sollte vorhanden sein:

- `build_linux/sunray-core`
- `webui-svelte/dist`
- `/etc/sunray-core/config.json`
- `/etc/sunray-core/map.json`
- `/etc/systemd/system/sunray-core.service`

## 4. Produktivdienst pruefen

```bash
systemctl status sunray-core --no-pager
journalctl -u sunray-core -n 100 --no-pager
bash scripts/check_deploy_state.sh
```

## 5. Erste Betriebspruefung

Nach dem Umschalten mindestens einmal pruefen:

- Boot-Reboot
- Dashboard erreichbar unter `http://<pi-ip>:8765`
- Live-Telemetrie kommt an
- Start / Stop / Dock funktionieren
- Diagnose-Seite erreichbar
- Verlauf-Seite zeigt Events, Sessions und Statistik
- Laden / Undock / Docking laufen sauber
- Stop-Button reagiert

## Rollback

Wenn `sunray-core` auf Alfred nicht stabil genug laeuft:

```bash
sudo systemctl stop sunray-core
sudo systemctl disable sunray-core
sudo systemctl enable sunray
sudo systemctl start sunray
```

Danach:

```bash
systemctl status sunray --no-pager
journalctl -u sunray -n 100 --no-pager
journalctl -u sunray-core -n 100 --no-pager
```

## Zielzustand fuer den Alltag

Wenn die ersten echten Mow-/Dock-/Charge-Zyklen sauber laufen:

- `sunray-core` aktiv und enabled
- `sunray` installiert, aber disabled
- Rollback nur fuer echte Stoerfaelle
