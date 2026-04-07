# TASK.md

## Projektziel

Vorschau, gespeicherter Missionsplan, Laufzeitroute und Dashboard-Fortschritt basieren auf derselben kompilierten Missionsroute.
Die WebUI enthält keine eigene Planungslogik.
Coverage und Transit werden intern getrennt behandelt.
Ungültige Missionen sind nicht startbar.

## Status

- Phase: Vereinheitlichung von Planung und Laufzeit
- Stand: Kernpaket erledigt
- Priorität: Hoch
- Owner: Navigation / WebUI / Runtime

## Harte Regeln

- Es gibt genau eine kompilierte Missionsroute.
- Vorschau, Runtime und Dashboard verwenden genau diese Route.
- Die WebUI rendert nur Backend-Daten.
- Es gibt keine stillen Direktverbindungen, wenn Planner-Übergänge fehlschlagen.
- Jede Mission wird vor Start validiert.
- Recovery wirkt lokal, nicht global.

## Erledigt: Missionsroute vereinheitlichen

### P1 - Zwingend zuerst

- [x] Task 1.1 - RoutePoint um Semantik erweitern
- [x] Task 1.2 - MowRoutePlanner setzt semantic und zoneId
- [x] Task 1.3 - Vorschau-API serialisiert semantic und zoneId
- [x] Task 1.4 - WebUI rendert nur Backend-Route
- [x] Task 2.1 - CompiledMissionRoute als Standardpfad definieren
- [x] Task 2.2 - startPlannedMowing() als Standard-Einstieg verwenden
- [x] Task 2.3 - Implizite Neugenerierung beim Start entfernen
- [x] Task 3.1 - RouteValidator neu einführen
- [x] Task 3.2 - Missionsstart bei ungültiger Route blockieren
- [x] Task 3.3 - Fehlerzustände in Preview exportieren

### P2 - Fachliche Planung stabilisieren

- [x] Task 4.1 - MowRoutePlanner logisch in Coverage und Transition aufteilen
- [x] Task 4.2 - Globales Nearest-Neighbour aus Coverage entfernen und Fix verifizieren
- [x] Task 4.3 - Transitionen fachlich korrekt behandeln
- [x] Task 5.1 - Inset/Offset auf Clipper2 umstellen
- [x] Task 5.2 - Headland/Infill-Anschluss testen

### P3 - Laufzeit und UX sauber machen

- [x] Task 6.1 - Recovery nur auf aktuelles Segment anwenden
- [x] Task 7.1 - Dashboard-Fortschritt segment- und routenbasiert berechnen

## Erledigt: Pflicht-Tests

- [x] Test A - Einfache Rechteckzone
- [x] Test B - Zone nahe Perimeter
- [x] Test C - Zone mit Exclusion
- [x] Test D - Mehrere Zonen
- [x] Test E - Planner-Fehler / nicht planbarer Übergang

## Definition of Done

- [x] Es gibt genau eine kompilierte Missionsroute.
- [x] Vorschau verwendet diese Route.
- [x] Runtime verwendet diese Route.
- [x] Dashboard verwendet diese Route.
- [x] Die WebUI enthält keine eigene Planungslogik.
- [x] RoutePoint enthält semantic.
- [x] RoutePoint enthält zoneId.
- [x] Alle neuen Planner-Punkte tragen sinnvolle Semantik.
- [x] Ungültige Transitionen werden nicht kaschiert.
- [x] Jede Route wird vor Start validiert.
- [x] Ungültige Missionen sind nicht startbar.
- [x] Pflicht-Tests sind grün.

## Später: RTK- und GPS-Folgearbeiten

- [ ] Zielsystem-Konfiguration prüfen: /etc/sunray-core/config.json gegen Repo-Defaults abgleichen, insbesondere gps_configure=true.
- [ ] Live-Hardwaretest mit F9P durchführen: korrigierte Signale, RTCM-Alter und Dashboard-Anzeige unter Realbedingungen prüfen.
- [ ] RTK-Warnschwellen nach Realmessung feinjustieren: insbesondere RTCM-Warnschwelle und Verhalten bei fehlenden korrigierten Signalen.

## Nächster Hauptblock: Komponenten und Transitionen

- [x] Working-Area-Komponenten als explizites Planungsmodell einführen.
- [x] RoutePoint um componentId erweitern.
- [x] Transit-Semantik um TRANSIT_BETWEEN_COMPONENTS ergänzen.
- [x] Coverage-Reihenfolge zuerst lokal innerhalb einer Komponente optimieren.
- [x] Inter-Komponenten-Transitionen über explizite Policy statt implizit behandeln.
- [x] RouteValidator um Regeln für unzulässige Komponentenwechsel erweitern.

### Geplante Tests

- [x] Test H - Zwei echte Working-Area-Komponenten, global legal verbindbar.
- [x] Test I - Zwei echte Komponenten, nicht legal verbindbar.
- [x] Test J - Mehrere Komponenten mit stabiler, reproduzierbarer Reihenfolge.

## Planner-Konsolidierung (Stand 2026-04-07)

### P1

- [x] Working-Area-Fallback entfernen
- [x] Offset/Inset auf Polygonmengen umbauen
- [x] Grid-Endpunktlogik hart machen
- [x] Runtime-Replan semantikfähig machen
- [x] Legacy-Startpfade hart trennen: NavToStart und Mow bauen keine Route mehr implizit auf; gültige Startpfade aktivieren die Route vor dem Op-Wechsel
- [x] Legacy-Map-Zonenstartpfad entfernen: zonenbasierte Starts laufen nur noch über die kompilierte Missionsroute

### P1.5

- [x] Komponentenidentität stabil halten
- [x] componentId nicht implizit von Clipper-Reihenfolge ableiten
- [x] deterministische Sortierung pro Build garantieren

### P2

- [x] Validator auf Planner-Regeln anheben
- [x] Komponentenreihenfolge fachlich definieren
- [x] Debug-Ausgaben entlang der Planner-Kette schärfen

### P3

- [x] UI-Debug-Layer ausbauen

## Projektstand (2026-04-04)

### Backend (C++ / Raspberry Pi)

- [x] Runtime stabil, 237/237 Tests grün
- [x] Safety-Pfade: Stop, Watchdog, MCU-Verlust, Dock-Recovery, Stuck-Detection
- [x] WebSocket-API: Commands serialisiert auf Control-Loop-Thread, inkl. reset-Support
- [x] Joystick: Unterstützung für manuelle Steuerung in Idle und Charge (Alfred-Hardware-getestet)
- [x] MQTT: Reconnect + Subscription-Recovery, inkl. drive- und reset-Befehle
- [x] History-DB mit Wanduhr-Timestamps
- [x] STM32-OTA: Upload + Flash via WebUI
- [x] Pi-OTA: POST /api/ota/check + POST /api/ota/update + POST /api/restart
- [x] systemd-Stop über sigwait()-Thread

### WebUI (Svelte)

- [x] Responsiv (<=640px Mobile-Layout)
- [x] Joystick (drive-Kommando, linear/angular)
- [x] OTA-Karte: Update prüfen, installieren, Service-Neustart mit Reconnect-Warten
- [x] STM-Flash: .bin Upload + kontrollierter Flash
- [x] Verlauf-Seite: Events, Sessions, Statistik, Export

### Mobile App (Flutter / Android)

- [x] mDNS-Discovery + manueller Connect
- [x] Dashboard: Karte (GPS-Position), Status-Overlay, Joystick-FAB, Start/Stop/Dock
- [x] Missionen: Karte + DraggableSheet, Zone-Auswahl, Vorschau, Start
- [x] Karte: Map-Editor mit Perimeter/Zonen/No-Go/Dock
- [x] Service: Pi-OTA (Check/Install/Restart), App-OTA via GitHub Releases
- [x] v1.0.0 Release auf GitHub mit APK-Asset

## Offene Punkte

| Thema | Prio | Notiz |
|---|---|---|
| MQTT 30min-Disconnect | H | Broker-Keepalive oder Session-Expiry - noch nicht reproduziert |
| HA-Integration | M | MQTT-Discovery vorhanden, App-Seite fehlt noch |
| STM-OTA aus App | L | WebUI hat es, App noch nicht |
| avahi-daemon Neustart auf Pi | L | Port 8765 in sunray.service.xml, braucht einmalig sudo systemctl restart avahi-daemon |

## Nächste Aufgabe

Vor dem Start einer neuen Aufgabe hier eintragen:

- Aufgabe:
- Ziel:
- Betroffene Dateien:
- Risiko: