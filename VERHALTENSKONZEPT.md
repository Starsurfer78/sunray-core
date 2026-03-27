# GreenGuard OS – Verhaltenskonzept
**Version:** 2.0  
**Stand:** 27.03.2026  
**Zweck:** Dieses Dokument beschreibt den aktuellen Verhaltensstand des Systems, das daraus ableitbare Zielbild und die konzeptionellen Leitplanken, die weiterhin richtungsweisend sind.

---

## 1. Einordnung

Die erste Fassung dieses Dokuments war stark zukunftsorientiert. Sie hat eine vollständige Zielarchitektur beschrieben, von der heute ein relevanter Teil bereits umgesetzt ist, ein anderer Teil aber noch nicht oder nur vereinfacht vorliegt.

Diese Neufassung verfolgt deshalb drei Ziele:

1. Sie beschreibt den **aktuellen Ist-Zustand** des Systems auf Basis des laufenden Codes.
2. Sie hält fest, **wie das Verhalten mittelfristig aussehen könnte**, ohne Wunschbild und Realität zu vermischen.
3. Sie markiert die Punkte aus dem ursprünglichen Konzept, die **weiterhin richtungsweisend** sind und bei künftigen Erweiterungen nicht verloren gehen sollten.

Das Dokument ist damit weder reine Architekturvision noch reine API-Doku. Es ist eine belastbare Arbeitsgrundlage für Weiterentwicklung, Reviews und Feldtests.

---

## 2. Leitprinzipien

Die folgenden Grundideen aus dem ursprünglichen Konzept sind weiterhin gültig:

- **Sicherheit vor Funktion.** Bei Lift, Motorfehlern, kritischer Unterspannung oder ungefangenen Laufzeitfehlern stoppt der Roboter.
- **Deterministisches Verhalten vor versteckter Magie.** Die wesentlichen Zustandswechsel passieren explizit über die Op-Kette.
- **Klare Verantwortlichkeiten.** Robot-Loop, Navigation, Web/API, Map-Logik und Operations sind im Code sauber getrennt.
- **Hardwareferne Kernlogik.** Verhalten wird gegen `HardwareInterface` implementiert und kann dadurch in Tests und Simulation geprüft werden.

Diese Prinzipien sind bereits im aktuellen Code sichtbar und sollten beibehalten werden.

---

## 3. Ist-Zustand

### 3.1 Laufzeitmodell

Die Laufzeit wird heute von `Robot::run()` gesteuert. Pro Zyklus werden:

1. Hardware-Tick und Sensor-Snapshots gelesen
2. State Estimation aktualisiert
3. Batterie- und GPS-Resilience geprüft
4. die Op-State-Machine getickt
5. LEDs und Telemetrie aktualisiert

Die zentrale Orchestrierung liegt in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp).

### 3.2 Aktuelles Zustandsmodell

Der Roboter verwendet aktuell eine **Op-Kette** statt einer enum-basierten FSM. Implementierte Operations sind:

- `Idle`
- `Mow`
- `Dock`
- `Charge`
- `EscapeReverse`
- `EscapeForward`
- `GpsWait`
- `Error`

Die Op-Verwaltung liegt in [Op.h](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.h) und [Op.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/Op.cpp).

Wichtig dabei:

- Der heutige Code kennt **keine eigenen Ops** für `Undock`, `NavToStart`, `WaitRain`, `KidnapWait`, `Manual` oder `GpsRebootRecovery`.
- Das reale Verhalten ist daher kompakter als im ursprünglichen Konzept.

### 3.3 Aktuelle Übergänge

Vereinfacht gilt derzeit:

- `Idle -> Mow` durch Bedienkommando
- `Idle -> Dock` durch Bedienkommando
- `Mow -> EscapeReverse` bei Hindernis
- `Mow -> GpsWait` bei GPS-Verlust
- `Mow -> Dock` bei niedriger Batterie, Regen, Missionsende oder GPS-Timeout
- `Mow -> Error` bei Lift, Motorfehler oder IMU-Fehler
- `Dock -> Charge` bei erkanntem Ladeanschluss
- `Dock -> EscapeReverse` bei Hindernis
- `Dock -> GpsWait` bei GPS-Verlust
- `Dock -> Error` bei Lift, Motorfehler oder wiederholtem Routing-Fehler
- `Charge -> Idle` bei Charger-Disconnect
- `Charge -> Mow` bei Timetable-Start und ausreichender Batterie
- `GpsWait -> vorherige Op oder Idle` bei GPS-Erholung
- `GpsWait -> Error` bei Timeout

Die eigentliche Reaktionslogik steckt in:

- [MowOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/MowOp.cpp)
- [DockOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/DockOp.cpp)
- [ChargeOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/ChargeOp.cpp)
- [GpsWaitFixOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/GpsWaitFixOp.cpp)
- [ErrorOp.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/op/ErrorOp.cpp)

### 3.4 Navigation und Lagebestimmung

Der aktuelle Code ist bei der Pose-Schätzung bereits weiter als das ursprüngliche Konzept:

- Es gibt einen **EKF-basierten StateEstimator** für Odometrie, GPS und IMU.
- GPS-Fix und GPS-Float werden unterschieden.
- Bei verschlechterter GPS-Qualität wird die Fahrgeschwindigkeit reduziert.
- Die Navigation erfolgt per **Stanley-ähnlichem LineTracker** entlang geladener Wegpunkte.

Wesentliche Bausteine:

- [StateEstimator.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.h)
- [StateEstimator.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/StateEstimator.cpp)
- [LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp)

Aktueller fachlicher Stand:

- Die Pose-Schätzung ist implementiert.
- Kidnap-Erkennung existiert als Pfadabweichungslogik im Tracker.
- GPS-Speed-/Motion-Themen sind nur teilweise aktiv und nicht als eigenes Op-Modell ausgebaut.

### 3.5 Karten- und Missionsmodell

Heute arbeitet das System mit einer bereits erzeugten Kartenstruktur:

- `perimeter`
- `mow`
- `dock`
- `exclusions`
- `zones`

Die Mission wird **nicht** im Kernsystem erzeugt, sondern über vorhandene Wegpunkte ausgeführt. Der Roboter:

- lädt die Karte aus JSON,
- startet an einem geeigneten Mähpunkt,
- kann Zonenmengen filtern,
- kann Dock-Routen aufbauen,
- kann virtuelle Hindernisse berücksichtigen.

Wesentliche Stellen:

- [Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h)
- [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp)

Nicht Teil des aktuellen Kerns:

- keine Headland-Generierung
- keine Boustrophedon-Erzeugung im Robot-Core
- keine adaptive Wendelogik als vollständige Missionsplanung

### 3.6 Safety und Fehlerverhalten

Im aktuellen Stand sind folgende Sicherheitsreaktionen fest verankert:

- **Bumper, Lift, Motorfehler** stoppen die Motoren unmittelbar im Robot-Loop.
- **Kritische Unterspannung** stoppt den Loop und fordert Abschalten an.
- **Ungefangene Exceptions** im Laufzeitpfad werden abgefangen, geloggt und führen zu Motor-Stopp.
- **ErrorOp** hält den Roboter an und signalisiert Fehlerzustand.

Die robuste Abarbeitung ist wesentlich in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) umgesetzt.

### 3.7 Kommunikation, Telemetrie und Bedienung

Der Kommunikationsstand ist heute deutlich besser definiert als im ursprünglichen Konzept:

- HTTP-API und WebSocket sind vorhanden.
- Schreibende API-Operationen und WS-Commands sind per `api_token` abgesichert.
- Telemetrie enthält neben Zuständen inzwischen auch Debug-Felder wie:
  - `ts_ms`
  - `state_since_ms`
  - `event_reason`
  - `error_code`
- MQTT kann optional dieselben Telemetriedaten publizieren.
- Schedule/Timetable ist bereits umgesetzt und per API verfügbar.

Wesentliche Stellen:

- [WebSocketServer.h](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.h)
- [WebSocketServer.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp)
- [Schedule.h](/mnt/LappiDaten/Projekte/sunray-core/core/Schedule.h)
- [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp)

### 3.8 WebUI als Verhaltensoberfläche

Die WebUI ist nicht nur ein Anzeigewerkzeug, sondern faktisch die wichtigste Bedien- und Diagnoseoberfläche des Systems. Sie gehört deshalb zum Verhaltenskonzept dazu.

Aus heutiger Sicht übernimmt die WebUI vier Rollen:

- **Bedienoberfläche** für Start, Dock, Stop, Diagnose und Konfiguration
- **Statusoberfläche** für laufende Telemetrie, GPS, Batterie, aktiven Zustand und Fehlersicht
- **Konfigurationsoberfläche** für Karte, Schedule und sicherheitsrelevante Betriebsparameter
- **Diagnoseoberfläche** für Servicefälle, Inbetriebnahme und Feldtests

Architektonisch ist wichtig:

- Die WebUI ist **nicht** die Quelle der Fachlogik.
- Die WebUI spiegelt und steuert Verhalten, das im Core modelliert sein muss.
- Wenn die WebUI mehr Zustände anzeigen soll als der Core sauber unterscheidet, entsteht eine Lücke zwischen Bedienmodell und Laufzeitmodell.

Genau diese Lücke ist aktuell teilweise sichtbar:

- Das UI denkt in fachlich reicheren Phasen wie Start, Docking, Fehler, Diagnose und manueller Bedienung.
- Der Core kennt momentan aber eine kleinere Menge expliziter Ops.
- Dadurch muss die WebUI teils implizit aus Telemetrie ableiten, was fachlich eigentlich als eigener Zustand modelliert sein sollte.

Für die weitere Architektur bedeutet das:

- Die WebUI sollte im Verhaltensmodell als **eigene Konsumentin und Steueroberfläche** mitgedacht werden.
- Neue Ops wie `UndockOp`, `NavToStartOp`, `WaitRainOp` oder `ManualOp` würden nicht nur die Core-Logik verbessern, sondern auch das UI-Modell vereinfachen und robuster machen.

### 3.8 Was heute bewusst noch nicht vollständig da ist

Folgende Teile waren im ursprünglichen Konzept vorgesehen, sind aber im aktuellen Code noch nicht oder nur in vereinfachter Form vorhanden:

- vollständige Boot-/Handshake-State-Maschine zwischen Pi und MCU
- eigene `UndockOp`
- eigene `NavToStartOp`
- eigene `WaitRainOp`
- eigene `KidnapWaitOp`
- eigene `GpsRebootRecoveryOp`
- vollwertiger `ManualOp`
- vollständige Missionsgenerierung im Core
- vollständige Dock-Retry- und Kontakt-Logik wie im Ursprungsdokument

---

## 4. Was aus dem alten Konzept weiterhin richtungsweisend ist

Nicht alles aus der ersten Fassung ist veraltet. Einige Teile sind weiterhin sehr gute Architekturentscheidungen.

### 4.1 Op-Kette statt enum-FSM

Diese Entscheidung war richtig und hat sich bewährt.

Warum sie weiterhin richtungsweisend ist:

- verkettete Recovery-Pfade lassen sich sauber ausdrücken
- Rückkehr zur vorherigen Operation ist natürlich modellierbar
- neue Sonderzustände lassen sich ergänzen, ohne eine zentrale Switch-FSM aufzublähen

Konsequenz:

- Neue Verhaltensbausteine sollten weiter als Ops entstehen, nicht als verstreute Sonderflags im Robot-Loop.

### 4.2 Sicherheit als eigenständige erste Schicht

Das Konzept hat Safety konsequent priorisiert. Das sollte so bleiben.

Richtungsweisend bleiben insbesondere:

- Motor-Stopp als sofortige Primärreaktion
- Lift niemals als normales Recovery-Event behandeln
- kritische Fehler sauber von weicheren Degradationspfaden trennen
- sicherheitsrelevante Entscheidungen nicht in UI- oder Netzlogik verstecken

### 4.3 Klare Trennung zwischen Verhaltensschicht und Planungs-/Missionsschicht

Das alte Konzept wollte langfristig:

- eine Missionserzeugung,
- eine Fahr-/Recovery-Schicht,
- eine Hardware-/Safety-Schicht

klar voneinander trennen.

Das ist weiterhin sinnvoll. Der heutige Code ist diesem Ziel näher als am Anfang, aber noch nicht ganz dort.

### 4.4 Reiche, erklärbare Telemetrie

Das war im ersten Dokument noch eher eine Vision. Inzwischen ist es teilweise real.

Weiterhin richtungsweisend ist:

- Zustandsdauer sichtbar machen
- Gründe für Übergänge maschinenlesbar benennen
- Fehlercodes stabil halten
- aus der Telemetrie echte Diagnose ableiten können

### 4.5 Docking und Recovery als eigene fachliche Domänen

Das ursprüngliche Konzept hat korrekt erkannt, dass Docking, GPS-Resilience und Obstacle-Recovery keine Nebensachen sind, sondern eigenständige Verhaltensdomänen.

Das bleibt wichtig. Gerade diese Teile sollten künftig nicht nur „irgendwie funktionieren“, sondern bewusst modelliert werden.

---

## 5. Zielbild – wie das System sinnvollerweise werden könnte

Das folgende Zielbild leitet sich aus dem aktuellen Code und dem alten Konzept gemeinsam ab. Es ist bewusst realistischer als die erste Fassung.

### 5.1 Ziel-Zustandsmodell

Langfristig sinnvoll wäre eine erweiterte Op-Landschaft:

- `IdleOp`
- `UndockOp`
- `NavToStartOp`
- `MowOp`
- `EscapeReverseOp`
- `DockOp`
- `ChargeOp`
- `GpsWaitOp`
- `WaitRainOp`
- `KidnapWaitOp`
- `ManualOp`
- `ErrorOp`

Nicht jede dieser Ops muss sofort kommen. Aber genau diese Richtung würde das heutige vereinfachte Verhalten sauberer und besser testbar machen.

### 5.2 Ziel für Start- und Missionspfad

Der heutige direkte Sprung nach `Mow` ist funktional, aber fachlich grob.

Mittelfristig sinnvoll wäre:

- `Idle -> Undock`
- `Undock -> NavToStart`
- `NavToStart -> Mow`

Vorteile:

- klarere Telemetrie
- bessere Fehlersuche am Dock
- präzisere Recovery-Regeln je Phase
- weniger implizite Zustandsannahmen in `MowOp`

### 5.3 Ziel für GPS-Resilience

Heute gibt es einen guten Anfang mit `GpsWait`.

Sinnvolle Weiterentwicklung:

- Trennung zwischen „Signal degradiert, aber weiterfahrbar“ und „Signal unbrauchbar“
- definierte Recovery-Dauer
- optionaler GPS-Reboot-Pfad
- klare Rückkehrregel in die vorherige Op

### 5.4 Ziel für Regen- und Umgebungsreaktionen

Der heutige Regenpfad ist noch grob.

Sinnvoll wäre:

- `WaitRainOp` statt sofortigem Übergang zu `Dock` in allen Fällen
- Kombination aus Timetable, Wetter und Ladezustand
- einheitliche Behandlung von Regen, Temperatur und Auto-Restart

### 5.5 Ziel für Docking

Docking ist heute lauffähig, aber noch nicht voll ausmodelliert.

Sinnvoll wäre:

- explizites Undocking
- konsistente Dock-Retry-Strategie
- bessere Unterscheidung zwischen Wegfehler, Kontaktfehler und GPS-Näheproblemen
- ein nachvollziehbarer Übergang von `Dock` nach `Charge` und danach zurück in `Idle` oder `Mow`

### 5.6 Ziel für Missionserzeugung

Der aktuelle Kern sollte weiter geladene Wegpunkte akzeptieren. Das ist praktisch und robust.

Zusätzlich könnte langfristig eine saubere externe oder optionale Missionsgenerierung ergänzt werden:

- Headland
- Füllbahnen
- Zonenstrategie
- Wendemanöver
- optionale Glättung

Wichtig dabei:

- Diese Planungslogik sollte nicht ungeordnet in `Map` oder `Robot` hineinwachsen.
- Sie gehört in eine klar abgegrenzte Planungs-/Mission-Komponente.

---

## 6. Empfohlene nächste Architektur-Schritte

Aus heutiger Sicht sind diese Schritte am sinnvollsten:

Wichtig dabei: Diese Punkte sind nicht als völlig neue Ideen zu verstehen. Sie sind eher die **Rückführung des Systems auf einen Sollzustand**, der im ursprünglichen Konzept bereits angelegt war, im aktuellen Code aber nur teilweise angekommen ist.

1. **Startpfad explizit machen.**
   `UndockOp` und `NavToStartOp` würden den größten Verhaltensgewinn bringen.

2. **Docking fachlich schließen.**
   `DockOp` sollte die noch offenen TODOs verlieren und einen sauberen Retry-Pfad bekommen.

3. **GPS-Warte- und Recovery-Pfad verfeinern.**
   `GpsWait` kann in einen klareren Resilience-Baustein überführt werden.

4. **Regen/Umwelt nicht mehr nur als Dock-Shortcut behandeln.**
   Ein eigener Warte-/Resume-Pfad wäre fachlich sauberer.

5. **Das Telemetriemodell weiter stabilisieren.**
   Zustandsgründe, Fehlercodes und Übergangsgründe sollten künftig bewusst Teil der Verhaltensarchitektur sein.

6. **WebUI und Core-Zustandsmodell enger zusammenführen.**
   Die WebUI sollte fachliche Zustände nicht erraten müssen. Ziel sollte sein, dass Bedienoberfläche, Telemetrie und Op-Modell dieselbe Sprache sprechen.

---

## 7. Dokumentstatus

Dieses Dokument beschreibt ab jetzt:

- den **realen Ist-Zustand** des Projekts,
- die **richtungsweisenden Architekturentscheidungen**,
- und ein **realistisches Zielbild** für die nächsten Ausbaustufen.

Es ist ausdrücklich **keine reine Wunscharchitektur** mehr. Wenn Code und Dokument auseinanderlaufen, soll dieses Dokument an den realen Stand angepasst oder die Abweichung bewusst markiert werden.
