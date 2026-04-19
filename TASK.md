# TASK.md - Roadmap & Architektur-Vision

## Projektziel
Sunray-Core ist ein vollständig lokales, präzises Mähroboter-Betriebssystem. 
Vorschau, gespeicherter Missionsplan, Laufzeitroute und Dashboard-Fortschritt basieren auf derselben kompilierten Missionsroute. Die WebUI enthält keine eigene Planungslogik. Coverage und Transit werden intern getrennt behandelt. Ungültige Missionen sind nicht startbar.

---

## Aktueller Status (Stand 2026-04)
- **Kern-Navigation (RTK/GPS, Path-Planning, A*)**: Abgeschlossen und funktional.
- **State Machine & Ops**: Grundlegend stabil (Idle, Mow, Dock, Undock, Charge).
- **Benutzeroberfläche (WebUI / App)**: Responsiv, inkl. Joystick, Map-Editor und OTA-Fähigkeit.
- **Datenmodelle**: Saubere Trennung zwischen Karte (Geometrie) und Mission (Zonen & Mäh-Parameter).

---

## Zukünftige Roadmap & Architektur-Vision (Navimow-Niveau)

Um das System auf das Level moderner kommerzieller Roboter (z.B. Segway Navimow, Husqvarna EPOS) zu heben, sind folgende Erweiterungen geplant:

### 1. Sensor-Fusion & "Vision" (Kamera-Integration)
* **Ziel:** Weg von rein physischer Hinderniserkennung (Bumper/Motorstrom) hin zu prädiktiver Erkennung (Igel, Spielzeug, Gartenschläuche).
* **Ansatz:** Integration einer Kamera (z.B. ESP32-Cam oder USB-Cam am Pi) gekoppelt mit OpenCV/KI-Modellen.
* **Architektur:** Eine neue Klasse `VisionSensor`, die bei Erkennung eines Objekts dynamisch virtuelle Hindernisse (OnTheFlyObstacles) auf der `Map` registriert, *bevor* der Roboter kollidiert.

### 2. Intelligentes Hindernis-Management (Smooth Detour)
* **Ziel:** Flüssigeres Umfahren von Hindernissen ohne starre "Stop & Rotate"-Rettungsmanöver.
* **Ansatz:** Erweiterung des `EscapeReverseOp` bzw. Einführung eines `DynamicDetourOp`. Wenn ein Hindernis (durch Vision oder Bumper) erkannt wird, berechnet der PathPlanner eine weiche Umfahrung (Spline/Bezier) und gliedert den Roboter sanft wieder in die ursprüngliche Mähbahn ein.

### 3. Predictive GPS (Vorhersage von Abschattungen)
* **Ziel:** Vermeidung von Liegenbleibern (`GpsWaitFix`) an Hauswänden oder unter dichten Bäumen.
* **Ansatz:** Aufbau einer "GPS-Heatmap" als Layer über der `sunray-map.json`, die sich Signalqualitäten an Koordinaten merkt.
* **Architektur:** Der `MissionCompiler` nutzt diese Heatmap, um Zonen mit schlechtem Empfang bevorzugt zu Tageszeiten mit optimaler Satellitenabdeckung zu mähen oder die Bahn-Winkel so zu drehen, dass Abschattungen auf dem kürzesten Weg durchquert werden.
* **Fallback (Visual Odometry):** Bei kurzzeitigem GPS-Verlust kann das Kamera-Modul (siehe Punkt 1) zur optischen Wegstreckenmessung genutzt werden, um Drift zu minimieren.

### 4. Glattere Pfadübergänge (Splines / Bezier-Kurven)
* **Ziel:** Schonung des Rasens an Wendepunkten (Vermeidung von tiefen Radspuren durch Drehen auf der Stelle).
* **Ansatz:** Der Pfadplaner berechnet an den Enden der Mähbahnen weiche Kurven (Clothoiden / "Schlüsselloch"-Bewegungen).
* **Architektur:** Erweiterung der `RoutePoint`-Semantik und Anpassung der Motorsteuerung (`LineTracker`), um kontinuierliche Bögen statt harter Stop-and-Turn-Befehle zu fahren.

### 5. WebUI & App: "Virtueller Joystick" per RTK
* **Ziel:** Einfachere Kartierung für den Nutzer.
* **Ansatz:** Anstatt den Roboter beim Kartieren permanent manuell wie ein RC-Auto zu steuern, klickt der Nutzer in der App auf einen Punkt in der Nähe. Der Roboter fährt autonom per RTK dorthin. Der Nutzer setzt dann nur noch die Grenzpunkte (Vertices) der Zone.

---

## Technische Schulden & Cleanup
- [ ] **MQTT Disconnect-Probleme**: MQTT 30min-Disconnect (Broker-Keepalive oder Session-Expiry) genauer analysieren und reproduzieren.
- [ ] **Home Assistant Integration**: MQTT-Discovery im Backend ist vorhanden, das Pendant in der Mobile App fehlt noch.
- [ ] **Avahi-Daemon**: Nach Updates muss der Avahi-Daemon auf dem Pi manchmal manuell neu gestartet werden (`sudo systemctl restart avahi-daemon`), um mDNS wieder sauber auszustrahlen.
- [ ] **RTK-Warnschwellen feinjustieren**: Schwellen für RTCM-Alter und Warnungen bei ausbleibenden Korrektursignalen nach Realmessungen im Feld anpassen.
