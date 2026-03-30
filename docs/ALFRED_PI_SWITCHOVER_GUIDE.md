# Alfred Pi Switchover Guide

Stand: 2026-03-30

Ziel dieser Anleitung ist ein sicherer, nachvollziehbarer erster Test von
`sunray-core` auf echter Alfred-Hardware, ohne den aktuell laufenden
Originalstand unkontrolliert zu verlieren.

Wichtig fuer die WebUI auf dem Pi:

- `webui/dist` muss gebaut sein (`cd ~/sunray-core/webui && npm install && npm run build`)
- fuer den aktuellen Frontend-Stack wird `Node.js >= 20` benoetigt
- wenn `api_token` in `/etc/sunray-core/config.json` gesetzt ist, muss derselbe
  Wert in der WebUI oben in der Topbar unter `API-Token` gespeichert werden,
  sonst bleiben geschuetzte Bereiche wie `Einstellungen` leer

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

Ergebnis:   sunray.service                                              loaded active running Sunray Service

systemctl status sunray --no-pager

● sunray.service - Sunray Service
     Loaded: loaded (/etc/systemd/system/sunray.service; enabled; preset: enabled)
     Active: active (running) since Sat 2025-11-22 10:42:36 CET; 4 months 5 days ago
   Main PID: 497 (start_sunray.sh)
      Tasks: 10 (limit: 3921)
        CPU: 7min 287ms
     CGroup: /system.slice/sunray.service
             ├─497 /bin/bash /home/Sascha/Sunray/alfred/start_sunray.sh
             ├─527 /bin/bash ../ros/scripts/dbus_monitor.sh
             ├─619 sudo dbus-monitor --system "interface='de.sunray.Bus'"
             ├─620 /bin/bash ../ros/scripts/dbus_monitor.sh
             ├─623 sudo dbus-monitor --system "interface='de.sunray.Bus'"
             ├─624 dbus-monitor --system "interface='de.sunray.Bus'"
             └─804 /home/Sascha/Sunray/alfred/build/sunray

Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 19:59:31 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.45 max=36
Mar 29 19:59:32 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.i…
Mar 29 19:59:35 AlfredMower start_sunray.sh[804]: 3056:16:53 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=…age=0.77
Mar 29 19:59:38 AlfredMower start_sunray.sh[804]: FAN POWER STATE 0
Mar 29 19:59:40 AlfredMower start_sunray.sh[804]: 3056:16:58 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=…age=0.77
Mar 29 19:59:41 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=4 min=0 mean=0.48 max=27
Mar 29 19:59:45 AlfredMower start_sunray.sh[804]: 3056:17:3 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=3…age=0.78
Mar 29 19:59:50 AlfredMower start_sunray.sh[804]: 3056:17:8 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=3…age=0.78
Hint: Some lines were ellipsized, use -l to show in full.

ps aux | grep -i sunray
root         497  0.0  0.0   6784  2972 tty12    Ss+  19:07   0:00 /bin/bash /home/Sascha/Sunray/alfred/start_sunray.sh
root         619  0.0  0.1  10892  4036 tty12    S+   19:07   0:00 sudo dbus-monitor --system interface='de.sunray.Bus'
root         623  0.0  0.0  10892  1568 pts/0    Ss+  19:07   0:00 sudo dbus-monitor --system interface='de.sunray.Bus'
root         624  0.0  0.0   6248  2436 pts/0    S    19:07   0:00 dbus-monitor --system interface='de.sunray.Bus'
root         804 13.0  0.0 170232  3648 tty12    Sl+  19:08   6:49 /home/Sascha/Sunray/alfred/build/sunray
Sascha      6028  0.0  0.0   6092  2048 pts/1    S+   20:00   0:00 grep --color=auto -i sunray

ls -l /etc/systemd/system /lib/systemd/system | grep -i sunray
-rw-r--r-- 1 root root  603 Sep  6  2025 sunray.service

ls -l /etc/sunray
ls: cannot access '/etc/sunray': No such file or directory

```

Zusätzlich sinnvoll:

```bash
journalctl -u sunray -n 100 --no-pager
Mar 29 19:58:00 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 19:58:01 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=26
Mar 29 19:58:01 AlfredMower start_sunray.sh[804]: enableCharging 1
Mar 29 19:58:02 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31373, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 19:58:04 AlfredMower start_sunray.sh[804]: charger conn=1 chgEnabled=1 chgTime=0 charger: 31.24 V  0.00 A  bat: 31.58 V   diffV=-0.33 slope(v/min)=0.01 slopeLowCounter=0
Mar 29 19:58:05 AlfredMower start_sunray.sh[804]: 3056:15:23 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.57,0.007(0.39) chg=31.24,1(0.00) diff=-0.337 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64703000 lon=9.20329280 lat=53.60768321 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.94
Mar 29 19:58:06 AlfredMower start_sunray.sh[804]: enableCharging 0
Mar 29 19:58:09 AlfredMower start_sunray.sh[804]: charger conn=1 chgEnabled=0 chgTime=0 charger: 31.24 V  0.00 A  bat: 31.55 V   diffV=-0.33 slope(v/min)=0.01 slopeLowCounter=0
Mar 29 19:58:10 AlfredMower start_sunray.sh[804]: 3056:15:28 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.57,0.007(0.39) chg=31.22,1(0.00) diff=-0.329 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64708000 lon=9.20329278 lat=53.60768325 h=42.8 n=-7.39 e=-3.95 d=2.73 sol=2 age=0.94
Mar 29 19:58:11 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=35
Mar 29 19:58:15 AlfredMower start_sunray.sh[804]: 3056:15:33 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.59,0.006(0.43) chg=31.24,1(0.00) diff=-0.319 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64713000 lon=9.20329285 lat=53.60768321 h=42.7 n=-7.38 e=-3.96 d=2.75 sol=2 age=0.93
Mar 29 19:58:20 AlfredMower start_sunray.sh[804]: 3056:15:38 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.56,0.006(0.46) chg=31.25,1(0.00) diff=-0.318 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64718000 lon=9.20329280 lat=53.60768317 h=42.7 n=-7.39 e=-3.96 d=2.75 sol=2 age=0.95
Mar 29 19:58:21 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.46 max=25
Mar 29 19:58:25 AlfredMower start_sunray.sh[804]: 3056:15:43 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.57,0.007(0.46) chg=31.23,1(0.00) diff=-0.321 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64723000 lon=9.20329277 lat=53.60768316 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.95
Mar 29 19:58:29 AlfredMower start_sunray.sh[804]: batTemp=-9999  cpuTemp=47
Mar 29 19:58:30 AlfredMower start_sunray.sh[804]: 3056:15:48 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.61,0.007(0.45) chg=31.26,1(0.00) diff=-0.326 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64728000 lon=9.20329279 lat=53.60768323 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.95
Mar 29 19:58:30 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=17
Mar 29 19:58:30 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 19:58:30 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 19:58:31 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.45 max=35
Mar 29 19:58:32 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31372, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 19:58:35 AlfredMower start_sunray.sh[804]: 3056:15:53 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.55,0.007(0.44) chg=31.24,1(0.00) diff=-0.344 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64733000 lon=9.20329281 lat=53.60768320 h=42.7 n=-7.39 e=-3.95 d=2.75 sol=2 age=0.94
Mar 29 19:58:39 AlfredMower start_sunray.sh[804]: FAN POWER STATE 0
Mar 29 19:58:40 AlfredMower start_sunray.sh[804]: 3056:15:58 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.58,0.007(0.43) chg=31.25,1(0.00) diff=-0.333 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64738000 lon=9.20329277 lat=53.60768316 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.94
Mar 29 19:58:41 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=26
Mar 29 19:58:45 AlfredMower start_sunray.sh[804]: 3056:16:3 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.56,0.007(0.44) chg=31.24,1(0.00) diff=-0.347 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64743200 lon=9.20329282 lat=53.60768324 h=42.8 n=-7.38 e=-3.96 d=2.73 sol=2 age=0.95
Mar 29 19:58:50 AlfredMower start_sunray.sh[804]: 3056:16:8 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.62,0.007(0.46) chg=31.23,1(0.00) diff=-0.340 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64748000 lon=9.20329277 lat=53.60768317 h=42.8 n=-7.38 e=-3.96 d=2.73 sol=2 age=0.96
Mar 29 19:58:51 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.47 max=36
Mar 29 19:58:55 AlfredMower start_sunray.sh[804]: 3056:16:13 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.59,0.007(0.46) chg=31.22,1(0.00) diff=-0.339 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64753200 lon=9.20329281 lat=53.60768323 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.96
Mar 29 19:59:00 AlfredMower start_sunray.sh[804]: 3056:16:18 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.57,0.006(0.46) chg=31.23,1(0.00) diff=-0.330 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64758200 lon=9.20329278 lat=53.60768324 h=42.7 n=-7.39 e=-3.95 d=2.75 sol=2 age=0.97
Mar 29 19:59:00 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=17
Mar 29 19:59:00 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 19:59:00 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 19:59:01 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.47 max=27
Mar 29 19:59:02 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31372, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 19:59:05 AlfredMower start_sunray.sh[804]: 3056:16:23 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.58,0.006(0.48) chg=31.23,1(0.00) diff=-0.336 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64763200 lon=9.20329279 lat=53.60768322 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.97
Mar 29 19:59:09 AlfredMower start_sunray.sh[804]: charger conn=1 chgEnabled=0 chgTime=1 charger: 31.27 V  0.00 A  bat: 31.57 V   diffV=-0.34 slope(v/min)=0.01 slopeLowCounter=0
Mar 29 19:59:10 AlfredMower start_sunray.sh[804]: 3056:16:28 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.59,0.007(0.47) chg=31.25,1(0.00) diff=-0.338 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64768200 lon=9.20329281 lat=53.60768324 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.97
Mar 29 19:59:11 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.47 max=39
Mar 29 19:59:15 AlfredMower start_sunray.sh[804]: 3056:16:33 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.57,0.007(0.45) chg=31.27,1(0.00) diff=-0.333 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64773200 lon=9.20329280 lat=53.60768320 h=42.8 n=-7.37 e=-3.96 d=2.73 sol=2 age=0.95
Mar 29 19:59:20 AlfredMower start_sunray.sh[804]: 3056:16:38 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.49,0.006(0.47) chg=31.19,1(0.00) diff=-0.332 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64778200 lon=9.20329277 lat=53.60768322 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.76
Mar 29 19:59:21 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.46 max=27
Mar 29 19:59:25 AlfredMower start_sunray.sh[804]: 3056:16:43 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.54,0.006(0.48) chg=31.17,1(0.00) diff=-0.334 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64783200 lon=9.20329281 lat=53.60768324 h=42.8 n=-7.38 e=-3.96 d=2.73 sol=2 age=0.77
Mar 29 19:59:29 AlfredMower start_sunray.sh[804]: batTemp=-9999  cpuTemp=46
Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: 3056:16:48 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.51,0.006(0.47) chg=31.19,1(0.00) diff=-0.332 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64788200 lon=9.20329276 lat=53.60768316 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.76
Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=17
Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 19:59:30 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 19:59:31 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.45 max=36
Mar 29 19:59:32 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31371, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 19:59:35 AlfredMower start_sunray.sh[804]: 3056:16:53 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.52,0.006(0.48) chg=31.20,1(0.00) diff=-0.335 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64793200 lon=9.20329284 lat=53.60768321 h=42.8 n=-7.38 e=-3.95 d=2.73 sol=2 age=0.77
Mar 29 19:59:38 AlfredMower start_sunray.sh[804]: FAN POWER STATE 0
Mar 29 19:59:40 AlfredMower start_sunray.sh[804]: 3056:16:58 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.54,0.006(0.48) chg=31.21,1(0.00) diff=-0.337 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64798200 lon=9.20329277 lat=53.60768317 h=42.8 n=-7.39 e=-3.96 d=2.74 sol=2 age=0.77
Mar 29 19:59:41 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=4 min=0 mean=0.48 max=27
Mar 29 19:59:45 AlfredMower start_sunray.sh[804]: 3056:17:3 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.55,0.007(0.48) chg=31.20,1(0.00) diff=-0.342 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64803200 lon=9.20329280 lat=53.60768315 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.78
Mar 29 19:59:50 AlfredMower start_sunray.sh[804]: 3056:17:8 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.50,0.006(0.46) chg=31.20,1(0.00) diff=-0.340 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64808200 lon=9.20329282 lat=53.60768324 h=42.8 n=-7.38 e=-3.96 d=2.74 sol=2 age=0.78
Mar 29 19:59:51 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.50 max=26
Mar 29 19:59:55 AlfredMower start_sunray.sh[804]: 3056:17:13 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.50,0.006(0.46) chg=31.16,1(0.00) diff=-0.350 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64813200 lon=9.20329283 lat=53.60768320 h=42.8 n=-7.38 e=-3.95 d=2.73 sol=2 age=0.68
Mar 29 20:00:00 AlfredMower start_sunray.sh[804]: 3056:17:18 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.45,0.005(0.49) chg=31.13,1(0.00) diff=-0.331 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64818200 lon=9.20329285 lat=53.60768315 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.77
Mar 29 20:00:00 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=18
Mar 29 20:00:00 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 20:00:00 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 20:00:01 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=33
Mar 29 20:00:02 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31371, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 20:00:05 AlfredMower start_sunray.sh[804]: 3056:17:23 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.46,0.005(0.49) chg=31.10,1(0.00) diff=-0.343 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64823200 lon=9.20329280 lat=53.60768318 h=42.8 n=-7.40 e=-3.94 d=2.74 sol=2 age=0.78
Mar 29 20:00:09 AlfredMower start_sunray.sh[804]: charger conn=1 chgEnabled=0 chgTime=2 charger: 31.13 V  0.00 A  bat: 31.41 V   diffV=-0.33 slope(v/min)=0.00 slopeLowCounter=0
Mar 29 20:00:10 AlfredMower start_sunray.sh[804]: 3056:17:28 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.45,0.005(0.48) chg=31.14,1(0.00) diff=-0.326 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64828200 lon=9.20329278 lat=53.60768318 h=42.8 n=-7.40 e=-3.95 d=2.73 sol=2 age=0.77
Mar 29 20:00:11 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=21
Mar 29 20:00:15 AlfredMower start_sunray.sh[804]: 3056:17:33 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.50,0.006(0.50) chg=31.15,1(0.01) diff=-0.334 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64833200 lon=9.20329277 lat=53.60768315 h=42.8 n=-7.39 e=-3.95 d=2.73 sol=2 age=0.77
Mar 29 20:00:20 AlfredMower start_sunray.sh[804]: 3056:17:38 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.45,0.005(0.50) chg=31.14,1(0.00) diff=-0.330 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64838200 lon=9.20329284 lat=53.60768320 h=42.8 n=-7.38 e=-3.95 d=2.73 sol=2 age=0.78
Mar 29 20:00:21 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.46 max=35
Mar 29 20:00:25 AlfredMower start_sunray.sh[804]: 3056:17:43 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.48,0.006(0.50) chg=31.16,1(0.00) diff=-0.325 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64843200 lon=9.20329277 lat=53.60768318 h=42.8 n=-7.39 e=-3.95 d=2.73 sol=2 age=0.78
Mar 29 20:00:29 AlfredMower start_sunray.sh[804]: batTemp=-9999  cpuTemp=47
Mar 29 20:00:30 AlfredMower start_sunray.sh[804]: 3056:17:48 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.49,0.006(0.51) chg=31.18,1(0.00) diff=-0.320 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64848200 lon=9.20329280 lat=53.60768317 h=42.8 n=-7.39 e=-3.94 d=2.71 sol=2 age=0.78
Mar 29 20:00:30 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=18
Mar 29 20:00:30 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 20:00:30 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 20:00:31 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.46 max=31
Mar 29 20:00:32 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31370, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 20:00:35 AlfredMower start_sunray.sh[804]: 3056:17:53 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.54,0.006(0.48) chg=31.18,1(0.00) diff=-0.333 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64853200 lon=9.20329281 lat=53.60768322 h=42.7 n=-7.40 e=-3.94 d=2.76 sol=2 age=0.21
Mar 29 20:00:37 AlfredMower start_sunray.sh[804]: FAN POWER STATE 0
Mar 29 20:00:40 AlfredMower start_sunray.sh[804]: 3056:17:58 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.55,0.006(0.45) chg=31.19,1(0.00) diff=-0.342 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64858200 lon=9.20329282 lat=53.60768323 h=42.8 n=-7.39 e=-3.94 d=2.72 sol=2 age=0.23
Mar 29 20:00:41 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=5 min=0 mean=0.48 max=36
Mar 29 20:00:45 AlfredMower start_sunray.sh[804]: 3056:18:3 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.54,0.006(0.43) chg=31.21,1(0.00) diff=-0.347 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64863200 lon=9.20329278 lat=53.60768322 h=42.8 n=-7.39 e=-3.95 d=2.73 sol=2 age=0.22
Mar 29 20:00:50 AlfredMower start_sunray.sh[804]: 3056:18:8 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.55,0.006(0.43) chg=31.20,1(0.00) diff=-0.336 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64868200 lon=9.20329278 lat=53.60768316 h=42.8 n=-7.39 e=-3.95 d=2.74 sol=2 age=0.23
Mar 29 20:00:51 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.47 max=25
Mar 29 20:00:55 AlfredMower start_sunray.sh[804]: 3056:18:13 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.55,0.006(0.43) chg=31.21,1(0.00) diff=-0.323 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64873200 lon=9.20329282 lat=53.60768320 h=42.8 n=-7.40 e=-3.95 d=2.74 sol=2 age=0.23
Mar 29 20:01:00 AlfredMower start_sunray.sh[804]: 3056:18:18 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.53,0.006(0.44) chg=31.20,1(0.00) diff=-0.325 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64878200 lon=9.20329280 lat=53.60768325 h=42.8 n=-7.40 e=-3.94 d=2.74 sol=2 age=0.23
Mar 29 20:01:00 AlfredMower start_sunray.sh[804]: GPS time (UTC): dayOfWeek=sun  hour=18
Mar 29 20:01:00 AlfredMower start_sunray.sh[804]: AUTOSTOP: timetable is disabled
Mar 29 20:01:00 AlfredMower start_sunray.sh[804]: AUTOSTART: not docked automatically (use DOCK command first)
Mar 29 20:01:01 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.47 max=33
Mar 29 20:01:02 AlfredMower start_sunray.sh[804]: ChargeOp: charging completed (DOCKING_STATION=1, battery.isDocked=0, dockOp.initiatedByOperator=1, maps.mowPointsIdx=13, DOCK_AUTO_START=1, dockOp.dockReasonRainTriggered=0, dockOp.dockReasonRainAutoStartTime(min remain)=31370, timetable.mowingCompletedInCurrentTimeFrame=0, timetable.mowingAllowed=1, finishAndRestart=0)
Mar 29 20:01:05 AlfredMower start_sunray.sh[804]: 3056:18:23 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.53,0.006(0.43) chg=31.21,1(0.00) diff=-0.336 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64883200 lon=9.20329279 lat=53.60768316 h=42.8 n=-7.40 e=-3.94 d=2.73 sol=2 age=0.23
Mar 29 20:01:10 AlfredMower start_sunray.sh[804]: charger conn=1 chgEnabled=0 chgTime=3 charger: 31.22 V  0.00 A  bat: 31.54 V   diffV=-0.32 slope(v/min)=0.01 slopeLowCounter=0
Mar 29 20:01:10 AlfredMower start_sunray.sh[804]: 3056:18:28 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.56,0.006(0.46) chg=31.21,1(0.00) diff=-0.324 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64888200 lon=9.20329282 lat=53.60768320 h=42.8 n=-7.39 e=-3.95 d=2.75 sol=2 age=0.01
Mar 29 20:01:11 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.48 max=26
Mar 29 20:01:15 AlfredMower start_sunray.sh[804]: 3056:18:33 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.53,0.006(0.46) chg=31.21,1(0.00) diff=-0.327 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64893200 lon=9.20329284 lat=53.60768318 h=42.8 n=-7.39 e=-3.95 d=2.75 sol=2 age=0.00
Mar 29 20:01:20 AlfredMower start_sunray.sh[804]: 3056:18:38 ctlDur=0.02 op=Charge(initiatedByOperator 0) mem=3648 bat=31.52,0.006(0.45) chg=31.21,1(0.00) diff=-0.329 tg=-3.14,-7.82 x=-4.08 y=-7.36 delta=2.69 tow=64898200 lon=9.20329279 lat=53.60768316 h=42.7 n=-7.38 e=-3.96 d=2.75 sol=2 age=0.82
Mar 29 20:01:21 AlfredMower start_sunray.sh[804]: Info - LoopTime(ms) now=1 min=0 mean=0.46 max=34

```

Dokumentieren:

- Name und Pfad des aktuell laufenden Dienstes
- welche Binary oder welches Script gestartet wird
- welche Config-Dateien verwendet werden
- ob zusätzliche Startskripte, Cronjobs oder lokale Anpassungen existieren

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

Danach die wichtigsten Alfred-relevanten Config-Werte pruefen:

- `driver_port`
- `i2c_bus`
- `ex3_mux_channel`
- `charger_connected_voltage_v`
- `gps_port`
- Batteriewerte
- eventuell Web-/API-Ports, falls bereits belegt

Fuer den aktuell bestaetigten Alfred-Pi-Stand gelten:

- `ex3_mux_channel = 0`
- `imu_mux_channel = 4`
- `charger_connected_voltage_v = 7.0`

Wichtig:

- `sudo cp config.example.json /etc/sunray-core/config.json` ueberschreibt
  vorhandene lokale Hardware-Anpassungen.
- Nach einem solchen Copy-Schritt die Alfred-spezifischen Werte immer direkt
  wieder pruefen, insbesondere `ex3_mux_channel`.

Noch nicht tun:

- keinen bestehenden Originaldienst ueberschreiben
- keine bestehende Alfred-Config blind durch die Example-Datei ersetzen, ohne
  die Hardware-Werte danach erneut abzugleichen
- keinen automatischen Autostart auf `sunray-core` umbiegen
- keine bestehende Service-Datei still ersetzen

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
