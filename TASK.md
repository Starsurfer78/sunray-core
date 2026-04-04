# TASK.md

## Projektstand (2026-04-04)

### Backend (C++ / Raspberry Pi)
- ✅ Runtime stabil, 237/237 Tests grün
- ✅ Safety-Pfade: Stop, Watchdog, MCU-Verlust, Dock-Recovery, Stuck-Detection
- ✅ WebSocket-API: Commands serialisiert auf Control-Loop-Thread, inkl. `reset` Support
- ✅ Joystick: Unterstützung für manuelle Steuerung in `Idle` und `Charge` (Alfred-Hardware-getestet)
- ✅ MQTT: Reconnect + Subscription-Recovery, inkl. `drive` und `reset` Befehle
- ✅ History-DB mit Wanduhr-Timestamps
- ✅ STM32-OTA: Upload + Flash via WebUI
- ✅ Pi-OTA: `POST /api/ota/check` + `POST /api/ota/update` + `POST /api/restart`
- ✅ systemd-Stop über `sigwait()`-Thread (kein mehr hängendes Restart)

### WebUI (Svelte)
- ✅ Responsiv (≤640px Mobile-Layout)
- ✅ Joystick (`drive`-Kommando, linear/angular)
- ✅ OTA-Karte: Update prüfen, installieren, Service-Neustart mit Reconnect-Warten
- ✅ STM-Flash: .bin Upload + kontrollierter Flash
- ✅ Verlauf-Seite: Events, Sessions, Statistik, Export

### Mobile App (Flutter / Android)
- ✅ mDNS-Discovery + manueller Connect
- ✅ Dashboard: Karte (GPS-Position), Status-Overlay, Joystick-FAB, Start/Stop/Dock
- ✅ Missionen: Karte + DraggableSheet, Zone-Auswahl, Vorschau, Start
- ✅ Karte: Map-Editor mit Perimeter/Zonen/No-Go/Dock
- ✅ Service: Pi-OTA (Check/Install/Restart), App-OTA via GitHub Releases
- ✅ v1.0.0 Release auf GitHub mit APK-Asset

## Offene Punkte

| Thema | Prio | Notiz |
|---|---|---|
| MQTT 30min-Disconnect | H | Broker-Keepalive oder Session-Expiry — noch nicht reproduziert |
| HA-Integration | M | MQTT-Discovery vorhanden, App-Seite fehlt noch |
| STM-OTA aus App | L | WebUI hat es, App noch nicht |
| avahi-daemon Neustart auf Pi | L | Port 8765 in sunray.service.xml, braucht einmaligen `sudo systemctl restart avahi-daemon` |

## Nächste Aufgabe

Vor dem Start einer neuen Aufgabe hier eintragen:

- **Aufgabe:**
- **Ziel:**
- **Betroffene Dateien:**
- **Risiko:**
