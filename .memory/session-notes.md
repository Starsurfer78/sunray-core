# Session Notes

## Stand 2026-04-04

### Abgeschlossen
- Backend Phase 1–3 vollständig (Safety, Reliability, Telemetrie, History, STM-OTA)
- WebUI responsiv, Joystick, OTA/Restart, STM-Flash, Verlauf
- Mobile App V1: Discovery, Dashboard (GPS-Karte, Joystick), Missionen, Karte, Service
- Pi-OTA in App: Check/Install/Restart mit Reconnect-Erkennung
- App-OTA: GitHub Release v1.0.0 erstellt mit APK-Asset

### Noch offen
- MQTT 30min-Disconnect: Hypothesen dokumentiert, nicht reproduziert
- HA-Integration: MQTT-Discovery läuft, App-Seite fehlt
- Joystick auf echtem Alfred noch nicht getestet
- avahi-daemon auf Pi braucht einmaligen Neustart wegen Port-Fix (8080→8765)
