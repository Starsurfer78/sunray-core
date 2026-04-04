# Known Issues

## Offen

### MQTT 30min-Disconnect
- Status: Investigating
- Symptom: Verbindung trennt nach ~30 Minuten
- Hypothesen: Broker-Keepalive-Mismatch, stale Session, Token-Refresh
- Nächste Schritte:
  1. Broker-side Disconnect-Logs mit Zeitstempel sichern
  2. Client Keepalive und Reconnect-Logs vergleichen
  3. Reconnect-Zähler instrumentieren, Langzeit-Soak-Test

### avahi-daemon Port-Fix
- Status: Pending (einmalige Aktion auf Pi)
- `scripts/sunray.avahi.service.xml` hat Port 8765 (war 8080)
- Braucht: `sudo systemctl restart avahi-daemon` auf Alfred

## Update-Regel
- Gelöste Einträge entfernen, nicht archivieren
- Ursache und Fix in Commit-Message festhalten
