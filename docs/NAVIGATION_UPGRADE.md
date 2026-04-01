# Navigation Upgrade Review

## Scope

Geprueft wurden die Module unter `core/navigation`:

- `StateEstimator`
- `LineTracker`
- `Map`
- `GridMap`

Der Fokus lag auf Laufzeitfehlern, unklarer Architektur, unlogischem Verhalten und praktischen Verbesserungen fuer einen Mower-/Docking-Stack.

## Kurzfazit

Der Code ist insgesamt schon gut lesbar getrennt in `StateEstimator`, `Tracker`, `Map` und `GridMap`. Die groessten Risiken liegen aber aktuell nicht in Stilfragen, sondern in drei funktionalen Bereichen:

1. EKF-Zustand und EKF-Kovarianz werden nicht konsistent propagiert.
2. Das Replanning in `Map` ist inkonsistent und schaltet teilweise in einen `FREE`-Modus, ohne dass ein gueltiger Pfad existiert.
3. Es gibt zwei konkurrierende Routing-Ideen im Code: `GridMap` mit A* und ein aelteres, geometrisches Detour-Verfahren in `Map::findPath()`. Die Header und Kommentare versprechen bereits mehr, als die Standardpfade heute wirklich liefern.

## Kritische Findings

### 1. EKF-Predict nutzt fuer die Kovarianz die falsche Heading-Basis

Referenzen:

- `core/navigation/StateEstimator.cpp:231`
- `core/navigation/StateEstimator.cpp:249`
- `core/navigation/StateEstimator.cpp:74`

In `update()` wird der Zustand bereits mit `midH = heading_ + dTheta * 0.5f` integriert und `heading_` danach auf den neuen Wert gesetzt. Anschliessend berechnet `predictStep()` sein Jacobian erneut mit `midH = heading_ + dTheta * 0.5f`, jetzt aber auf Basis der bereits aktualisierten Heading.

Effekt:

- Die Zustandsintegration nutzt `theta_k + dTheta/2`.
- Die Kovarianz nutzt effektiv `theta_k + 3*dTheta/2`.
- Zustand und Unsicherheit driften mathematisch auseinander.

Praktische Folge:

- Falsche Unsicherheitsellipse bei Kurvenfahrt.
- Schlechteres Verhalten bei GPS-/IMU-Korrekturen.
- Schwer reproduzierbare Filtereffekte, obwohl der Roboter mechanisch korrekt faehrt.

Empfehlung:

- Jacobian aus derselben Heading berechnen wie die Zustandsintegration.
- Entweder `predictStep()` bekommt `midH` explizit uebergeben, oder es uebernimmt auch die komplette Zustandsintegration.

### 2. Fehlgeschlagenes Replanning kann trotzdem `wayMode = FREE` setzen

Referenzen:

- `core/navigation/Map.cpp:315`
- `core/navigation/Map.cpp:321`

In `nextPoint()` wird bei erkannter Blockade `findPath(lastTargetPoint, targetPoint)` aufgerufen, der Rueckgabewert aber ignoriert. Danach wird in jedem Fall `wayMode = WayType::FREE` gesetzt.

Effekt:

- Der Navigator kann in einen FREE-Modus wechseln, obwohl `freePoints_` leer oder unbrauchbar sind.
- Der nachfolgende Ablauf wird dann vom alten `targetPoint` und inkonsistentem Zustand getragen.

Praktische Folge:

- Missionen brechen scheinbar "unlogisch" ab.
- Docking- oder Mow-Progression kann in einen Sackgassenmodus laufen.

Empfehlung:

- `wayMode` nur umschalten, wenn `findPath()` erfolgreich war und `freePoints_` konsistent befuellt wurden.
- Fehlerfall explizit behandeln: Route beibehalten, Op eskalieren oder auf definierten Fallback gehen.

### 3. `Map::findPath()` ignoriert Perimeter und Exclusions

Referenzen:

- `core/navigation/Map.cpp:617`
- `core/navigation/Map.cpp:657`

Die Standard-Routinglogik in `Map::findPath()` baut nur geometrische Detours um Kreise auf. Es gibt dort keine Pruefung gegen:

- Perimeter
- Exclusion-Zonen
- bereits erzeugte, aber weiter blockierte Teilsegmente

Effekt:

- Ein Detour kann ausserhalb des erlaubten Bereichs landen.
- Ein Pfad kann nach `maxIterations` weiterhin kollidieren und wird trotzdem als "fertig" uebernommen.

Praktische Folge:

- Navigation kann ausserhalb der Mow-Area oder durch verbotene Inseln routen.
- Docking-Detours koennen topologisch falsch sein.

Empfehlung:

- Entweder `GridMap` als einzige Pfadplanung nutzen, oder `findPath()` um harte Geofence-/Polygon-Checks erweitern.
- Vor dem Uebernehmen jeden finalen Pfad validieren.

### 4. `GridMap::planPath()` erlaubt Corner-Cutting und hat einen ungesicherten No-Free-Cell-Fall

Referenzen:

- `core/navigation/GridMap.cpp:156`
- `core/navigation/GridMap.cpp:167`
- `core/navigation/GridMap.cpp:231`

Probleme:

- Bei diagonalen Schritten wird nur das Ziel-Feld geprueft, nicht die beiden orthogonalen Nachbarfelder.
- Wenn Ziel ungueltig/belegt ist und es gar keine freie Zelle gibt, bleiben `dx`/`dy` undefiniert.

Effekt:

- A* kann diagonal "durch Ecken" von Hindernissen schneiden.
- Im Vollblockadefall entsteht undefiniertes Verhalten.

Empfehlung:

- Diagonalen nur erlauben, wenn beide angrenzenden Kardinalzellen frei sind.
- Nach der Suche nach einer Ersatz-Zielzelle explizit pruefen, ob ueberhaupt eine freie Zelle gefunden wurde.

### 5. FREE-Pfade verlieren Missionssemantik und Tracking-Flags

Referenzen:

- `core/navigation/Map.cpp:357`
- `core/navigation/Map.cpp:375`
- `core/navigation/Map.cpp:607`

`injectFreePath()` und `nextFreePoint()` sichern nicht sauber, welche Tracking-Eigenschaften fuer das urspruengliche Ziel galten. `trackReverse` und `trackSlow` werden fuer FREE-Segmente nicht explizit sauber gesetzt oder restauriert.

Effekt:

- Ein umgeplanter Pfad kann versehentlich im Reverse-Kontext oder mit falscher Geschwindigkeitslogik gefahren werden.
- Nach Rueckkehr von `FREE` nach `MOW` ist der Status nur implizit, nicht robust.

Empfehlung:

- FREE-Pfade als eigene Route mit eigener Segment-Metadatenstruktur behandeln.
- Beim Eintritt in FREE den Rueckkehrzustand komplett sichern: Zielindex, WayMode, Flags.

## Weitere Probleme und Unklarheiten

### 6. IMU-Heading wird im Stillstand komplett ignoriert

Referenzen:

- `core/navigation/StateEstimator.cpp:280`

Das ist als Drift-Schutz nachvollziehbar, blockiert aber auch legitime Orientierungskorrekturen im Stand. Gerade bei GPS-Antennenoffset und manueller Platzierung ist das unguenstig.

Besser:

- Im Stand nicht hart deaktivieren, sondern gewichten.
- Optional nur bei niedriger Gyro-Qualitaet oder hoher Drift sperren.

### 7. Safety-Clamp teleportiert Pose auf `(0,0)`, ohne den Filter sauber zu resetten

Referenzen:

- `core/navigation/StateEstimator.cpp:237`

Wenn `x_` oder `y_` groesser als 10 km werden, wird die Position auf Null gesetzt, aber Kovarianz, Heading und Modus bleiben erhalten.

Das ist fachlich gefaehrlich:

- Robot pose springt auf Kartenursprung.
- Folgelogik interpretiert das als echte Position.

Besser:

- Entweder `reset()` mit sauberem Fehlerstatus.
- Oder harte Schutzverletzung an den Op-/Fault-Layer melden.

### 8. `GridMap::markCircle()` widerspricht dem eigenen Kommentar

Referenzen:

- `core/navigation/GridMap.cpp:61`

Kommentar: "centre out of range - still mark nearby"
Code: `return`.

Das ist kein kosmetisches Problem, sondern ein Logikbruch. Hindernisse nahe am Grid-Rand koennen so komplett verloren gehen.

### 9. `skipNextMowingPoint()` und `setMowingPointPercent()` pflegen den Tracking-Zustand nicht sauber

Referenzen:

- `core/navigation/Map.cpp:435`
- `core/navigation/Map.cpp:441`

Beide APIs aendern Indizes, aber nicht konsistent:

- `lastTargetPoint`
- `targetPoint`
- `trackReverse`
- `trackSlow`
- `percentCompleted`

Falls diese Funktionen aktiv verwendet werden, sind Spruenge im Tracker-Verhalten vorprogrammiert.

### 10. Dokumentation und echte Implementierung laufen auseinander

Referenzen:

- `core/navigation/GridMap.h`
- `core/navigation/Map.h:199`
- `core/navigation/Map.cpp:617`

Die Header lesen sich so, als sei A* bereits der Standardpfad fuer Hindernisumfahrung. In der Praxis ist das nur teilweise wahr:

- `EscapeReverseOp` nutzt `GridMap`.
- `Map::findPath()` nutzt weiterhin das alte geometrische Detour-Verfahren.

Das ist aktuell die groesste konzeptionelle Unklarheit im Modul.

## Upgrade-Empfehlung

### Phase 1: Korrektheit zuerst

1. EKF-Predict konsistent machen.
2. `Map::nextPoint()` gegen fehlgeschlagenes Replanning absichern.
3. `GridMap::planPath()` gegen Corner-Cutting und No-Free-Cell absichern.
4. Safety-Clamp im `StateEstimator` durch definierten Fault-Flow ersetzen.

### Phase 2: Routing vereinheitlichen

1. Entscheiden, ob `GridMap` oder geometrischer Detour der Standard sein soll.
2. Wenn A* Standard wird:
   `Map::findPath()` auf `GridMap` umstellen oder entfernen.
3. Wenn geometrischer Detour bleibt:
   harte Perimeter-/Exclusion-Validierung nachruesten.

### Phase 3: Zustandsmodell sauber machen

1. FREE-Routen als eigene Datenstruktur mit Metadaten modellieren.
2. Rueckkehr in `MOW`/`DOCK` explizit und testbar machen.
3. Missionsfortschritt, Target-Handling und Replan-Status entkoppeln.

### Phase 4: Tests

Empfohlene Minimaltests:

- EKF: Geradeausfahrt, Kurvenfahrt, GPS-Korrektur nach Kurve.
- Routing: Ziel blockiert, kompletter Grid-Block, Diagonale an Hindernisecke.
- Map-State: Replan-Failure, Rueckkehr von FREE nach MOW, `skipNextMowingPoint()`.
- Geofence: Pfad nahe Perimeter und Exclusion.

## Priorisierung

### Sofort beheben

- EKF-Jacobian/Predict-Inkonsistenz
- `wayMode = FREE` trotz Replan-Fehler
- `Map::findPath()` ohne Geofence-Validierung
- `GridMap` Corner-Cutting / undefinierter Ziel-Fall

### Danach

- FREE-State sauber modellieren
- IMU-Update-Strategie im Stand verbessern
- Tracking-Hilfsfunktionen konsistent machen
- Kommentar-/Architekturabgleich herstellen

## Empfohlene Zielarchitektur

Aus Robotik-Sicht waere eine klare Aufteilung am robustesten:

- `StateEstimator`: nur Pose + Unsicherheit, ohne versteckte Teleports.
- `Map`: Missions- und Waypoint-Zustand.
- `Planner`: eine einzige autoritative Pfadplanung mit Geofence, Obstacle Inflation und Validierung.
- `LineTracker`: reiner Trajektorienfolger, ohne Missionslogik.

Der aktuelle Code ist nah an dieser Struktur, mischt aber noch Missionszustand und Routing-Fallbacks an mehreren Stellen. Genau dort entstehen heute die meisten logischen Brueche.

## Zielbild Mit Aktueller Hardware

### Ausgangslage

Mit der derzeit im Code sichtbaren Hardwarebasis ist realistisch verfuegbar:

- RTK-GPS als globaler Positionsanker
- IMU fuer Heading, Roll, Pitch
- Radodometrie
- Bumper links/rechts
- Lift-Sensor
- Docking-Kontakt bzw. Ladezustandsrueckmeldung

Nicht sichtbar sind aktuell:

- LiDAR
- Kamera / Vision
- Radar
- sonarbasierte Nahfeld-Karte als belastbare Planner-Eingabe

Das ist wichtig, weil damit das Zielbild fuer "optimal" nicht wie bei einem modernen Vision- oder LiDAR-Roboter aussehen kann. Optimal bedeutet hier also:

- maximale Robustheit innerhalb der vorhandenen Sensorik
- konsistente Planung gegen Geofence und bekannte Hindernisse
- sauberes Recovery-Verhalten, wenn die Sensorik keine sichere Weiterfahrt hergibt

### Was mit dieser Hardware optimal ist

Mit der aktuellen Hardware waere die beste sinnvolle Navigationsarchitektur:

1. Globales Kartenmodell
   Perimeter, NoGo-Zonen, Dockpfad, Zonen und Mow-Geometrie bleiben die statische Basis.

2. Lokale Costmap um den Roboter
   Ein lokales Raster um den Roboter, das laufend aus statischen und dynamischen Quellen aufgebaut wird.

3. Ein einheitlicher lokaler Planner
   Kein gemischtes System aus Detour-Workarounds und separatem Escape-A* mehr, sondern eine einzige Planungsinstanz fuer lokale Umfahrungen.

4. Reiner Trajektorienfolger
   `LineTracker` folgt nur noch dem vom Planner gelieferten lokalen Pfad.

5. Recovery nur als Ausnahme
   `EscapeReverse` bleibt erhalten, aber nur dann, wenn der lokale Planner keine gueltige Trajektorie mehr findet oder ein Bumperereignis bereits passiert ist.

## UX-Zielbild Fuer Endnutzer

Die sichtbare Standard-UI sollte bewusst einfacher bleiben als die interne Navigationslogik.

Normaler Nutzer-Workflow:

- Perimeter aufnehmen
- NoGo-Zonen anlegen
- Zonen anlegen
- Mähwinkel einstellen
- Randbahnen festlegen
- Zeitplan / Missionsreihenfolge definieren

Nicht Teil der Standard-UI:

- Planner-Parameter wie Clearance, Inflation, Replan-Takt oder Grid-Aufloesung
- Docking-Feinjustage wie `approachMode`, `finalAlignHeadingDeg` oder ein manuell editierter Dock-Korridor
- NoGo-Details wie `hard/soft`, `costScale` oder spezielle Clearance-Werte
- Zonale Service-Felder wie `reverseAllowed` oder planner-spezifische `clearance`

Diese Werte gehoeren stattdessen in:

- interne Defaults im Core
- `config.json` fuer Inbetriebnahme und Service-Tuning
- optional spaeter in einen versteckten Diagnose-/Service-Modus

Umgesetzte Produktentscheidung:

- Die Standard-WebUI wurde auf den einfachen Mapping-Ablauf reduziert.
- Erweiterte Planner- und Docking-Felder bleiben im Datenmodell und Core erhalten, sind fuer normale Nutzer aber nicht mehr sichtbar.
- Planner- und Docking-Defaults koennen intern ueber Config-Fallbacks gesetzt werden, auch wenn in der Karte keine erweiterten Felder gepflegt werden.

## Was "Costmap + Continuous Replanning" hier konkret bedeutet

### Costmap

Die lokale Costmap ist kein neues Kartenformat fuer den Nutzer, sondern ein internes Arbeitsmodell fuer den Planner.

In diese Costmap fliessen ein:

- Perimeter:
  ausserhalb immer gesperrt
- NoGo-Zonen:
  immer gesperrt
- Dock-Geometrie:
  fuer Docking zulaessiger Zielkorridor
- bekannte On-The-Fly-Obstacles:
  gesperrt plus Sicherheitsabstand
- Sicherheitsabstand zu Grenzen:
  nicht nur "frei/belegt", sondern auf Kosten aufgeweicht

Das heisst praktisch:

- direkt im Hindernis = verboten
- nah am Hindernis = teuer
- weit weg vom Hindernis = guenstig

Genau dieser Sicherheitsabstand fehlt dem aktuellen Code weitgehend noch als durchgaengiges Konzept.

### Continuous Replanning

Aktuell wird oft erst dann reagiert, wenn etwas schon schiefgeht:

- Hindernis wird erkannt
- Detour wird nachtraeglich gebaut
- Sonderfalllogik schaltet auf `FREE`

Continuous replanning bedeutet stattdessen:

- in jedem oder jedem n-ten Regelzyklus Costmap aktualisieren
- aktuellen lokalen Zielpunkt oder Zielkorridor bestimmen
- den besten lokalen Pfad neu berechnen
- wenn sich Hindernisse oder Geometrie aendern, sofort neuen Pfad verwenden

Mit eurer Hardware ist das nicht "Objekte live verfolgen", sondern:

- bekannte statische Geometrie permanent beachten
- gemerkte Bumper-Hindernisse fortlaufend in die Planung einbeziehen
- nach jeder relevanten Aenderung Pfad neu rechnen

### Unterschied zum aktuellen Ad-hoc-Detour

Aktuell:

- `Map::findPath()` baut im Konfliktfall Einzel-Detours
- `EscapeReverseOp` nutzt zusaetzlich `GridMap`-A*
- Missionszustand und Umplanung haengen an mehreren Stellen zusammen

Ziel:

- eine Costmap
- ein lokaler Planner
- ein gemeinsamer Pfadtyp
- ein klarer Uebergang: Planen -> Folgen -> Recovery bei Nichtloesbarkeit

Das ist nicht nur "schicker", sondern testbarer und deutlich robuster.

## Empfohlene Architektur Fuer Sunray Mit Aktueller Hardware

### 1. State Estimation

Beibehalten:

- EKF aus Odometrie + RTK + IMU

Verbessern:

- EKF-Mathematik korrigieren
- IMU-Update im Stand nicht hart abschalten, sondern gewichten
- Plausibilitaetsfehler in definierten Fault-State ueberfuehren statt Pose-Teleport

### 2. Static Map Model

Beibehalten:

- `perimeter`
- `exclusions`
- `dock`
- `zones`
- `mow`

Wichtig:

- Dieses Modell ist schon nah an dem, was ein guter Outdoor-Mower braucht.
- Das Problem liegt heute eher in der Planner-Nutzung als im Grundmodell.

### 3. Dynamic Obstacle Layer

Aus aktueller Hardware ableitbar:

- Bumper-Hits erzeugen persistente oder temporaere Hindernisse
- diese Hindernisse landen in einem dynamischen Layer der Costmap
- sie werden mit Inflation versehen

Das ist mit eurer Hardware der wichtigste Ersatz dafuer, dass ihr keine Kamera/LiDAR-Objekterkennung habt.

### 4. Local Planner

Empfohlen:

- `GridMap` als Basis weiterentwickeln
- daraus echte lokale Costmap + A* oder Theta*/Hybrid-A* im Nahbereich machen
- keine parallele zweite Logik in `Map::findPath()` mehr

Fuer eure Plattform ist das wahrscheinlich die beste Balance aus:

- Rechenaufwand
- Implementierbarkeit
- Testbarkeit
- Robustheit

### 5. Path Follower

`LineTracker` sollte nur noch:

- den geplanten lokalen Pfad verfolgen
- Geschwindigkeiten begrenzen
- Reverse nur dort erlauben, wo die Route es explizit erlaubt

Nicht mehr zustaendig fuer:

- Missionszustandswechsel
- implizite Routenumbauten
- Planer-Fallback-Entscheidungen

### 6. Recovery Layer

Recovery bleibt wichtig, aber klar getrennt:

- Stop
- kurzes Ruecksetzen
- lokale Neuplanung
- bei weiterem Scheitern Op-Eskalation

Mit aktueller Hardware ist das unverzichtbar, weil Bumper-Ereignisse immer erst nach Kontakt kommen koennen.

## Muss Das WebUI-Kartenmodell Geaendert Werden

### Kurzantwort

Nicht grundsaetzlich, aber es sollte erweitert werden.

Die heutigen vom WebUI erzeugten Kernobjekte sind fuer eine deutlich bessere Navigation schon brauchbar:

- `perimeter`
- `exclusions`
- `dock`
- `zones`
- zonale Parameter wie `stripWidth`, `angle`, `edgeMowing`, `edgeRounds`, `pattern`, `speed`

Das ist fuer Missionserzeugung und statische Planung bereits gut.

Fuer einen wirklich sauberen lokalen Planner fehlen aber noch einige semantische Angaben.

## Was unveraendert bleiben kann

Diese Strukturen koennen in der Grundform bleiben:

- `perimeter` als Polygon
- `exclusions` als Polygone
- `zones` als Polygone mit Fahr-/Mow-Parametern
- `dock` als Punktfolge bzw. Pfad

Auch die WebUI-seitige Geometrie-Pipeline fuer:

- effective area
- edge rounds
- stripe / spiral preview

passt weiterhin gut zur Missionsebene.

Die bestehende Trennung aus:

- statischer Karte
- zonalen Parametern
- missionsspezifischen Overrides

ist fachlich sinnvoll und sollte bleiben.

## Was am Kartenmodell ergaenzt werden sollte

### 1. Planner-Sicherheitsabstaende explizit modellieren

Heute werden Sicherheitsabstaende eher implizit im Code angenommen.
Besser waeren explizite optionale Felder:

- Perimeter-Sicherheitsabstand
- NoGo-Inflation
- Dock-Korridorbreite
- Hindernis-Sicherheitsradius

Warum:

- Der Planner braucht daraus direkt Costmap-Kosten.
- Heute steckt das teils als magische Zahl im Code, z.B. `+ 0.4f`.

### 2. NoGo-Zonen semantisch unterscheiden

Aktuell sind `exclusions` nur Geometrie.
Fuer einen besseren Planner waeren Typen sinnvoll:

- hart verboten
- weich zu meiden
- saisonal / temporaer

Mit aktueller Hardware reicht fuer den Anfang:

- `hard` und `soft`

Dann kann der Planner harte NoGo-Zonen blockieren und weiche Zonen nur verteuern.

### 3. Docking-Bereich staerker strukturieren

Aktuell ist `dock` nur eine Punktfolge. Das reicht fuer einfache Sequenzen, aber nicht optimal fuer robustes Docking.

Sinnvolle Ergaenzungen:

- Dock-Approach-Korridor oder Dock-Polygon
- bevorzugte Einfahrtrichtung
- optionaler finaler langsamer Align-Bereich

Damit laesst sich Docking besser vom normalen Mow-Planner unterscheiden, ohne Sonderfaelle zu verteilen.

### 4. Zonen optional um Planner-Metadaten erweitern

Die heutigen Zonenparameter sind auf Bahnerzeugung optimiert. Fuer den Planner waeren optional nuetzlich:

- bevorzugte Einfahrkante
- Reverse erlaubt ja/nein
- maximale Geschwindigkeit in der Zone
- Clearance-Faktor fuer enge Bereiche

Nicht alles ist Pflicht, aber die Struktur sollte das spaeter aufnehmen koennen.

### 5. Map-Version und abgeleitete Planner-Daten trennen

Empfehlung:

- die vom Nutzer gezeichnete Karte bleibt kompakt und semantisch
- abgeleitete Planner-Daten werden nicht im Editorformat gespeichert

Also:

- WebUI speichert weiter semantische Polygone und Parameter
- Core erzeugt daraus intern:
  - inflated geometry
  - cost layers
  - docking corridor masks
  - local planning grids

Das ist der sauberere Schnitt.

## Was ich fuer den naechsten Stand konkret empfehlen wuerde

### Im WebUI-Format beibehalten

- `perimeter`
- `exclusions`
- `zones`
- `dock`
- bestehende Zonen-Settings

### Im WebUI-Format neu ergaenzen

- `exclusions[].type`
  z.B. `hard` oder `soft`
- `planner`
  globaler Abschnitt mit Standardwerten fuer Clearance/Inflation
- `dockMeta`
  optionaler Dock-Korridor und bevorzugte Anfahrtrichtung
- `zone.settings.reverseAllowed`
  falls Rueckwaertsfahrt je Zone unterschiedlich gewuenscht ist
- `zone.settings.clearance`
  optionaler Planner-Abstand fuer enge oder sensible Bereiche

### Im Core intern neu ableiten

- statische Costmap aus Perimeter + NoGo + Dock-Meta
- dynamische Hindernis-Layer aus Bumper-Events
- einheitlicher lokaler Pfadtyp fuer `MOW`, `DOCK`, `FREE`

## Fazit Fuer Deine Frage

Mit der aktuellen Hardware muss das WebUI-Kartenmodell nicht komplett neu erfunden werden.
Es ist schon brauchbar und fuer Missionen sogar recht gut.

Geaendert werden sollte vor allem:

- mehr Planner-Semantik in die Karte bringen
- Sicherheitsabstaende explizit modellieren
- Docking nicht nur als Punktliste betrachten
- harte und weiche Sperrflaechen unterscheiden

Der groesste Hebel liegt aber trotzdem nicht zuerst im WebUI, sondern im Core:

- einheitliche Costmap
- ein Planner
- kontinuierliche Neuplanung
- klare Trennung zwischen Planen, Folgen und Recovery

Das Kartenmodell sollte dafuer erweitert werden, aber nicht ersetzt.

## Konkreter Datenvertragsvorschlag Fuer `map.json`

### Ziele

Der Datenvertrag sollte:

- bestehende Karten weiter laden koennen
- das aktuelle WebUI-Modell nicht brechen
- zusaetzliche Planner-Semantik optional ergaenzen
- klar zwischen editierter Karte und intern abgeleiteten Planner-Daten trennen

### Bestehendes Format

Heute ist fachlich bereits vorhanden:

- `perimeter`
- `mow`
- `dock`
- `exclusions`
- `zones`
- `captureMeta`

Das bleibt die Grundlage.

### Kompatibel erweitertes Zielschema

```json
{
  "perimeter": [[0.0, 0.0], [10.0, 0.0], [10.0, 8.0], [0.0, 8.0]],
  "mow": [
    [1.0, 1.0],
    { "p": [2.0, 1.0], "rev": false, "slow": false }
  ],
  "dock": [[0.8, 0.5], [0.3, 0.2]],
  "exclusions": [
    [[4.0, 3.0], [5.0, 3.0], [5.0, 4.0], [4.0, 4.0]]
  ],
  "zones": [
    {
      "id": "zone-front",
      "order": 1,
      "polygon": [[0.5, 0.5], [4.5, 0.5], [4.5, 3.5], [0.5, 3.5]],
      "settings": {
        "name": "Vorne",
        "stripWidth": 0.18,
        "angle": 0.0,
        "edgeMowing": true,
        "edgeRounds": 1,
        "speed": 0.8,
        "pattern": "stripe",
        "reverseAllowed": false,
        "clearance": 0.25
      }
    }
  ],
  "planner": {
    "defaultClearance": 0.25,
    "perimeterSoftMargin": 0.15,
    "perimeterHardMargin": 0.05,
    "obstacleInflation": 0.35,
    "softNoGoCostScale": 0.6,
    "replanPeriodMs": 250,
    "gridCellSize": 0.1
  },
  "dockMeta": {
    "approachMode": "forward_only",
    "corridor": [[0.0, 0.0], [1.2, 0.0], [1.2, 1.0], [0.0, 1.0]],
    "finalAlignHeadingDeg": 180.0,
    "slowZoneRadius": 0.6
  },
  "exclusionMeta": [
    { "type": "hard", "clearance": 0.25 },
    { "type": "soft", "clearance": 0.15, "costScale": 0.5 }
  ],
  "captureMeta": {
    "origin": "rtk"
  }
}
```

### Wichtige Regel

Alle neuen Felder sind optional.

Wenn sie fehlen, gelten Defaultwerte und die Karte verhaelt sich wie heute.

## Bedeutung Der Neuen Felder

### `planner`

Globale Standardparameter fuer den lokalen Planner und die Costmap.

Empfohlene erste Felder:

- `defaultClearance`
  Standard-Sicherheitsabstand in Metern
- `perimeterSoftMargin`
  Abstand zur Begrenzung, der teuer aber noch fahrbar ist
- `perimeterHardMargin`
  Abstand zur Begrenzung, der als gesperrt behandelt wird
- `obstacleInflation`
  zusaetzlicher Sicherheitsradius fuer dynamische Hindernisse
- `softNoGoCostScale`
  Gewichtung weicher Sperrflaechen
- `replanPeriodMs`
  Zielzyklus fuer Neuplanung
- `gridCellSize`
  Zielaufloesung der lokalen Costmap

Warum global:

- diese Werte sind keine Editordaten pro Polygon
- sie beschreiben das Fahrverhalten des Roboters

### `dockMeta`

Erweitert die heutige `dock`-Punktfolge um Planner-Semantik.

Empfohlene Felder:

- `approachMode`
  z.B. `forward_only`, `reverse_allowed`
- `corridor`
  Polygon, in dem die finale Dock-Anfahrt erfolgen soll
- `finalAlignHeadingDeg`
  bevorzugte Schlussausrichtung
- `slowZoneRadius`
  Bereich fuer langsame Anfahrt

Nutzen:

- Docking bekommt einen klaren Sonderkorridor
- Sonderlogik im Core wird geringer

### `exclusionMeta`

Die heutige `exclusions`-Geometrie bleibt unberuehrt.
Die Metadaten stehen parallel dazu und werden indexbasiert zugeordnet.

Empfohlene Felder je Exclusion:

- `type`
  `hard` oder `soft`
- `clearance`
  zusaetzlicher lokaler Abstand
- `costScale`
  nur fuer `soft` relevant

Wichtig:

- Das verhindert einen Bruch des bisherigen `Point[][]`-Formats.
- Spaeter kann man immer noch auf objektbasierte Exclusions umstellen, wenn noetig.

### `zone.settings.reverseAllowed`

Erlaubt dem Planner, Rueckwaertsfahrt zonenspezifisch zuzulassen oder zu verbieten.

Das ist sinnvoll, weil:

- Rueckwaertsfahrt im offenen Mow-Bereich oft unnoetig ist
- im Docking- oder Engstellenkontext aber bewusst erlaubt sein kann

### `zone.settings.clearance`

Optionaler planner-relevanter Abstand fuer einzelne Zonen.

Beispiele:

- enge Nebenflaechen mit hoeherer Vorsicht
- sensible Randbereiche
- Zonen mit absichtlich engerem Verhalten

## Empfohlene TypeScript-Typen Im WebUI

Vorschlag fuer `webui-svelte/src/lib/stores/map.ts` oder eine gemeinsame API-Type-Datei:

```ts
export interface Point {
  x: number
  y: number
}

export type ExclusionType = 'hard' | 'soft'
export type DockApproachMode = 'forward_only' | 'reverse_allowed'

export interface PlannerSettings {
  defaultClearance: number
  perimeterSoftMargin: number
  perimeterHardMargin: number
  obstacleInflation: number
  softNoGoCostScale: number
  replanPeriodMs: number
  gridCellSize: number
}

export interface DockMeta {
  approachMode: DockApproachMode
  corridor: Point[]
  finalAlignHeadingDeg: number | null
  slowZoneRadius: number
}

export interface ExclusionMeta {
  type: ExclusionType
  clearance?: number
  costScale?: number
}

export interface ZoneSettings {
  name: string
  stripWidth: number
  angle: number
  edgeMowing: boolean
  edgeRounds: number
  speed: number
  pattern: 'stripe' | 'spiral'
  reverseAllowed?: boolean
  clearance?: number
}

export interface Zone {
  id: string
  order: number
  polygon: Point[]
  settings: ZoneSettings
}

export interface RobotMap {
  perimeter: Point[]
  dock: Point[]
  mow: Array<Point | { p: [number, number]; rev?: boolean; slow?: boolean }>
  exclusions: Point[][]
  exclusionMeta?: ExclusionMeta[]
  zones: Zone[]
  planner?: Partial<PlannerSettings>
  dockMeta?: Partial<DockMeta>
  captureMeta?: Record<string, unknown>
}
```

### Wichtige Anmerkung zum WebUI-Modell

Wenn das WebUI intern weiter mit:

- `mow: Point[]`
- `exclusions: Point[][]`

arbeiten will, ist das voellig ok.

Dann sollte es beim Laden/Speichern nur eine kleine Normalisierung geben:

- alte Karten lesen
- optionale neue Metadaten lesen
- fehlende Defaults lokal ergaenzen

## Empfohlene C++-Erweiterungen

### `Map.h`

Ergaenzen um neue Strukturen:

```cpp
enum class ExclusionType { HARD, SOFT };
enum class DockApproachMode { FORWARD_ONLY, REVERSE_ALLOWED };

struct PlannerSettings {
    float defaultClearance_m   = 0.25f;
    float perimeterSoftMargin_m = 0.15f;
    float perimeterHardMargin_m = 0.05f;
    float obstacleInflation_m   = 0.35f;
    float softNoGoCostScale     = 0.6f;
    unsigned replanPeriod_ms    = 250;
    float gridCellSize_m        = 0.10f;
};

struct ExclusionMeta {
    ExclusionType type = ExclusionType::HARD;
    float clearance_m  = 0.25f;
    float costScale    = 1.0f;
};

struct DockMeta {
    DockApproachMode approachMode = DockApproachMode::FORWARD_ONLY;
    PolygonPoints corridor;
    float finalAlignHeading_deg = 0.0f;
    bool  hasFinalAlignHeading  = false;
    float slowZoneRadius_m      = 0.6f;
};
```

Zusatzfelder in `Map`:

- `PlannerSettings planner_`
- `std::vector<ExclusionMeta> exclusionMeta_`
- `DockMeta dockMeta_`

### Ladeverhalten

Die JSON-Parser sollten:

- neue Felder optional lesen
- bei fehlenden Feldern sichere Defaults verwenden
- alte Karten voll kompatibel weiter laden

## Migrationsstrategie Ohne Formatbruch

### Phase A

Nur lesen koennen:

- Core und WebUI koennen neue Felder optional lesen
- Speichern bleibt zunaechst im alten Format oder schreibt nur Defaults mit

### Phase B

Neue Felder im WebUI editierbar machen:

- Exclusion-Typ `hard/soft`
- globale Planner-Defaults
- Dock-Korridor / Dock-Meta
- optionale Clearance pro Zone

### Phase C

Core nutzt die Felder aktiv:

- Costmap aus `planner`, `dockMeta`, `exclusionMeta`
- lokaler Planner ersetzt schrittweise die alten Detours

## Was Ich Nicht Empfehlen Wuerde

Nicht sinnvoll waere es, die Karte jetzt schon mit internem Planner-Ballast zu fuellen, zum Beispiel:

- fertige lokale Grids in `map.json`
- vorberechnete A*-Pfade
- Costmap-Zellen als persistente Nutzdaten

Das sollte abgeleitet bleiben, nicht editiert.

## Empfehlung Fuer Den Naechsten Praktischen Schritt

1. `map.json` und `Map::load()` kompatibel um `planner`, `dockMeta` und `exclusionMeta` erweitern.
2. WebUI-Typen optional erweitern, aber noch nicht voll im UI expose'n.
3. Danach zuerst den Core-Planner auf diese neuen Felder umstellen.
4. Erst wenn der Core Nutzen daraus zieht, die Editieroberflaechen im WebUI nachziehen.

So bleibt der Umbau kontrolliert und ihr verhindert, dass das UI schon komplexer wird, bevor der Planner davon profitiert.

## TODO Liste

### P0 Korrektheit Und Sicherheitsluecken

- [x] `StateEstimator`: EKF-Predict mathematisch korrigieren, damit Zustandsupdate und Kovarianzupdate dieselbe Heading-Basis nutzen.
- [x] `StateEstimator`: Safety-Clamp auf `(0,0)` entfernen und durch definierten Fault-/Reset-Pfad ersetzen.
- [x] `StateEstimator`: IMU-Update im Stand nicht hart abschalten, sondern kontrolliert gewichten.
- [x] `Map::nextPoint()`: `wayMode = FREE` nur noch setzen, wenn Replanning wirklich erfolgreich war.
- [x] `Map::findPath()`: finalen Pfad immer gegen Perimeter, Exclusions und Hindernisse validieren.
- [x] `GridMap::planPath()`: No-Free-Cell-Fall absichern.
- [x] `GridMap::planPath()`: Corner-Cutting bei diagonalen Schritten verhindern.
- [x] `GridMap::markCircle()`: Verhalten am Grid-Rand korrigieren, damit Randhindernisse nicht verloren gehen.

### P1 Datenvertrag Und Kompatibilitaet

- [x] `Map.h`: `PlannerSettings`, `ExclusionMeta`, `DockMeta`, `ExclusionType`, `DockApproachMode` einfuehren.
- [x] `Map`: neue Member fuer `planner_`, `exclusionMeta_`, `dockMeta_` anlegen.
- [x] `Map::load()`: optionale JSON-Felder `planner`, `dockMeta`, `exclusionMeta` lesen.
- [x] `Map::save()`: neue Felder kompatibel schreiben, ohne alte Karten unlesbar zu machen.
- [x] `Map`: Getter fuer Planner-/Dock-/Exclusion-Metadaten bereitstellen.
- [x] Bestehende Kartenmigration definieren: fehlende Felder -> sinnvolle Defaults.

### P1 WebUI-Typen Vorbereiten

- [x] `webui-svelte/src/lib/stores/map.ts`: Typen um `planner`, `dockMeta`, `exclusionMeta` erweitern.
- [x] WebUI-Normalisierung beim Laden anpassen, damit alte und neue Karten akzeptiert werden.
- [x] Defaultwerte fuer neue Planner-Felder zentral definieren.
- [x] Sicherstellen, dass bestehende Editorfunktionen mit optionalen Metadaten weiter stabil laufen.

### P2 Planner-Vereinheitlichung

- [x] Entscheiden: `GridMap` wird der alleinige lokale Planner.
- [x] `Map::findPath()` entweder auf `GridMap` umstellen oder komplett entfernen.
- [x] Einheitlichen lokalen Pfadtyp fuer `MOW`, `DOCK` und replante `FREE`-Segmente definieren.
- [x] `injectFreePath()` und `nextFreePoint()` durch robustes Rueckkehrmodell mit gesichertem Kontext ersetzen.
- [x] `previousWayMode`-basierte implizite Logik auf explizite Routenzustaende umstellen.

Aktueller Stand:

- `MOW`, `DOCK` und replante `FREE`-Segmente laufen intern jetzt ueber `RoutePlan` und segmentierte Routensemantik.
- Offener Rest ist nicht mehr Architektur, sondern reale Validierung unter Linux/Pi und auf Hardware.

### P2 Costmap Einfuehren

- [x] Interne statische Costmap aus `perimeter`, `exclusions`, `dockMeta` und Planner-Defaults erzeugen.
- [x] Dynamischen Hindernis-Layer aus Bumper-/Obstacle-Ereignissen einfuehren.
- [x] Hindernisinflation ueber `planner.defaultClearance` und `planner.obstacleInflation` parametrisieren.
- [x] Harte und weiche Sperrflaechen unterschiedlich behandeln.
- [x] Replan-Zyklus (`replanPeriodMs`) im Core verankern.

Aktueller Stand:

- Mit `Costmap` existiert jetzt eine eigenstaendige Kostenkarten-Komponente.
- Statische Geometrie, Exclusions, dynamische Hindernisse und Dock-Praeferenz sind intern als getrennte Layer modelliert und werden zum Planner-Raster komponiert.

### P2 Docking-Verbesserung

- [x] Docking nicht mehr nur als Punktfolge behandeln, sondern `dockMeta.corridor` und `slowZoneRadius` nutzen.
- [x] Finale Dock-Anfahrt als eigenen Planner-Modus behandeln.
- [x] `approachMode` im Docking-Controller beruecksichtigen.
- [x] `finalAlignHeadingDeg` fuer die Schlussausrichtung nutzen, wenn vorhanden.

### P3 LineTracker Entkoppeln

- [x] `LineTracker` auf reine Pfadverfolgung reduzieren.
- [x] Missionswechsel und Planner-Fallback-Entscheidungen aus `LineTracker` herausziehen.
- [x] Reverse-/Slow-Logik an explizite Pfadsegment-Metadaten binden.
- [x] Kidnap-/Off-Path-Erkennung gegen neuen Planner-Zustand neu bewerten.

### P3 WebUI-Editor Fuer Neue Felder

- [x] Editor fuer globale Planner-Defaults ergaenzen.
- [x] NoGo-Zonen um `hard/soft` und optionalen Clearance-Wert ergaenzen.
- [x] Docking-Bereich um Korridor und Anfahrtsmodus erweitern.
- [x] Zonen optional um `reverseAllowed` und `clearance` erweitern.
- [x] UI so bauen, dass neue Felder optional bleiben und bestehende Karten nicht ueberfrachten.

### P3 Tests

- [x] Unit-Tests fuer EKF-Konsistenz bei Geradeaus- und Kurvenfahrt.
- [x] Unit-Tests fuer `Map::load()` mit altem und erweitertem `map.json`.
- [x] Planner-Tests fuer blockiertes Ziel, Vollblockade und Randhindernisse.
- [x] Geofence-Tests fuer Perimeter- und Exclusion-Naehe.
- [x] Tests fuer Rueckkehr von replanten Pfaden nach `MOW` und `DOCK`.
- [x] Docking-Tests mit und ohne `dockMeta`.
- [x] Robot-Tests fuer Perimeterverletzung in `Mow` und `NavToStart`.
- [x] Robot-Test fuer Dock-Watchdog-Eskalation.
- [x] Robot-Test fuer kritische Battery-Policy auf `Error` statt implizitem `Idle`.
- [x] Robot-Tests fuer `EscapeForward`/`EscapeReverse`-Eventfolgen.
- [x] Robot-Tests fuer `onMapChanged`-Dispatch waehrend aktiver Mission.

### Safety-Stand Nach Umbau

- [x] Perimeterverletzungen werden zentral in `Robot` erkannt und als `onPerimeterViolated()` an aktive Ops dispatcht.
- [x] `Mow`, `NavToStart`, `EscapeReverse` und `EscapeForward` reagieren darauf explizit.
- [x] `EscapeForward` und `EscapeReverse` behandeln inzwischen auch erneute Hindernisse bzw. Lift-Ereignisse.
- [x] Kritische Unterspannung eskaliert auf Op-Ebene nach `Error` statt ueber den Default implizit nach `Idle`.
- [x] `DockOp::lastMapRoutingFailed` ist als toter Zustand entfernt.
- [x] `DockOp` hat einen expliziten Op-Watchdog ueber `dock_max_duration_ms`.

Weiter offen:

- [x] Weitere Watchdog-Abdeckung fuer andere langlaufende Recovery-/Transit-States bewertet.
- [x] Eigene Regressionstests fuer `EscapeForward`/`EscapeReverse`-Eventfolgen.
- [x] Explizite Tests fuer `onMapChanged`-Dispatch waehrend aktiver Mission.

Bewertung Watchdog-Abdeckung:

- `DockOp` nutzt jetzt einen expliziten Watchdog.
- `UndockOp` und `GpsWaitFixOp` besitzen bereits eigene harte Zeitgrenzen in ihrer Laufzeitlogik.
- `EscapeForwardOp` und `EscapeReverseOp` sind durch kurze feste Dauer begrenzt.
- Fuer `ChargeOp` und `WaitRainOp` ist ein generischer Watchdog derzeit bewusst nicht sinnvoll, weil diese States absichtlich langlaufend sein duerfen.

### Safety-Nachzug

- [x] Perimeterverletzung als explizites Safety-Ereignis im `Robot`-Loop verdrahten.
- [x] `MowOp` und `NavToStartOp` bei Perimeterverletzung sicher abbrechen.
- [x] `EscapeForwardOp` um `onObstacle`, `onLiftTriggered` und Perimeter-Guard ergaenzen.
- [x] `EscapeReverseOp` um Reaktion auf erneutes Hindernis und Lift erweitern.
- [x] `onBatteryUndervoltage` auf konsistente Eskalation nach `Error` vereinheitlichen.
- [x] `DockOp::lastMapRoutingFailed` als toten Code entfernen.
- [x] `onMapChanged` als explizites Op-Ereignis einfuehren.
- [x] Op-Watchdog vorbereiten und fuer `DockOp` ueber `dock_max_duration_ms` aktivieren.

Aktueller Stand:

- Perimeterverletzung wird jetzt zentral in `Robot` erkannt und an die aktive Operation dispatcht, statt nur implizit ueber einzelne Sonderfaelle.
- Kartenwechsel waehrend einer aktiven Mission werden nicht mehr nur ueber `resumeBlockedByMapChange` fuer `GpsWait` sichtbar, sondern als eigenes Op-Ereignis behandelbar.
- Der erste echte Op-Watchdog ist fuer `DockOp` aktiv. Weitere Ops koennen denselben Mechanismus spaeter ueber eigene Timeouts nutzen.
- Die neue Safety-Abdeckung ist funktional und durch gezielte Regressionstests fuer Escape-, Geofence-, Map-Change- und Docking-Randfaelle abgesichert.

### P4 Architektur-Nachzug

- [x] Eigenstaendige `Costmap`-Komponente aus `GridMap` herausziehen.
- [x] Statische Layer (`perimeter`, `exclusions`, `dockMeta`) klar von dynamischen Hindernis-Layern trennen.
- [x] Gemeinsamen Routentyp fuer `MOW`, `DOCK` und `FREE` definieren statt gemischter Punktlisten.
- [x] `LineTracker` nur noch gegen einen vollstaendig segmentierten Routentyp arbeiten lassen.
- [x] Zonenparameter wie `reverseAllowed` und `clearance` in die eigentliche lokale Routengenerierung integrieren statt nur im Datenvertrag vorzuhalten.

Aktueller Zwischenstand:

- Mit [Route.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Route.h) existiert jetzt ein eigener Navigationstyp-Baustein fuer `Point`, `WayType`, `RoutePoint`, `RouteSegment` und `RoutePlan`.
- [Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h), [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp) und [LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp) nutzen diesen gemeinsamen Routentyp bereits fuer die replante lokale Route und Tracking-Semantik.
- `MOW`, `DOCK` und replante `FREE`-Segmente laufen intern jetzt vollstaendig ueber `RoutePlan`; die frueheren Legacy-Punktcontainer in [Map.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.h) und [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp) sind entfernt.
- [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) und [test_navigation.cpp](/mnt/LappiDaten/Projekte/sunray-core/tests/test_navigation.cpp) lesen fuer Missionsstart und Navigationsregressionen inzwischen ebenfalls `mowRoutePlan()` bzw. `dockRoutePlan()` statt der Legacy-Listen.
- Mit `exportMapJson()` in [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp), `getMapJson()` in [Robot.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/Robot.cpp) und `onMapGet(...)` in [WebSocketServer.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/WebSocketServer.cpp) laeuft auch der REST-Exportpfad `/api/map` jetzt ueber eine gezielte Exportfunktion statt ueber implizite Container- oder Fallback-Ansichten.
- Mit [PlannerContext.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/PlannerContext.h), [Planner.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.h) und [Planner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.cpp) gibt es jetzt eine eigene Planner-Fassade; [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp) delegiert `findPath()`, Replan-Pruefung und lokale Pfaderzeugung dorthin, statt `GridMap` direkt zusammenzubauen.
- Mit [Costmap.h](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.h) und [Costmap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Costmap.cpp) gibt es jetzt ausserdem eine eigene Costmap-Komponente, aus der [GridMap.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/GridMap.cpp) seine Rasterdaten bezieht.
- Die `Costmap` ist intern jetzt bereits in getrennte Layer fuer statische Geometrie, Exclusions, dynamische Hindernisse und Dock-Praeferenz aufgeteilt und wird am Ende zu einem Planner-Raster komponiert.
- [LineTracker.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/LineTracker.cpp) folgt jetzt den im `RouteSegment` vorverdichteten Informationen wie `targetHeadingRad`, `distanceToTarget_m`, `nextSegmentStraight` und `targetReached`, statt diese Entscheidungen erneut aus `Map`-Details zusammenzubauen.
- Zonenparameter `clearance` und `reverseAllowed` fliessen jetzt in [Planner.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Planner.cpp) und [Map.cpp](/mnt/LappiDaten/Projekte/sunray-core/core/navigation/Map.cpp) in lokale Replan-Routen und Segment-Metadaten ein.
- `P4` ist damit funktional abgeschlossen. Ein spaeterer Feinschliff koennte die interne Layer-Repraesentation der Costmap noch weiter formal abstrahieren, ist aber kein offener Architekturblock mehr.

Ziel von P4:

- Die heute bereits funktionalen Planner-Erweiterungen sollen von einer pragmatischen Einbettung in `Map` und `GridMap` auf eine saubere, testbare Architektur gehoben werden.
- Erst mit diesem Nachzug ist die in diesem Dokument beschriebene Zielarchitektur wirklich voll erreicht.

#### Technischer Zielzuschnitt Fuer P4

Empfohlene neue Bausteine:

- `core/navigation/Costmap.h/.cpp`
  Verantwortlich fuer das lokale Kostenraster, Layer-Merging und Zellkosten.
- `core/navigation/Route.h/.cpp`
  Gemeinsame Routensemantik fuer `MOW`, `DOCK` und `FREE`.
- `core/navigation/PlannerContext.h`
  Kleine Datenstruktur fuer Planner-Eingaben, damit `Map`, `Robot` und `LineTracker` nicht gegenseitig zu viele Details kennen muessen.
- `core/navigation/Planner.h/.cpp`
  Autoritativer lokaler Planner auf Basis von `Costmap` und gemeinsamer Route.

Empfohlene Kernstrukturen:

- `CostmapCell`
  `occupied`, `cost`, `sourceMask`, optional `clearance`
- `CostmapLayer`
  getrennte Layer fuer `perimeter`, `exclusions`, `dock`, `dynamic_obstacles`
- `RouteSegment`
  `start`, `goal`, `mode`, `reverseAllowed`, `slow`, `targetHeading`, `clearance`, `segmentId`
- `RoutePlan`
  Liste von `RouteSegment` plus Resume-/Mission-Kontext
- `PlannerContext`
  `robotPose`, `missionMode`, `target`, `zoneId`, `dockMeta`, `plannerSettings`

Verantwortungsschnitt:

- `Map`
  bleibt Besitzer von Karten- und Missionsdaten
- `Costmap`
  erzeugt und aktualisiert das lokale Weltmodell
- `Planner`
  berechnet daraus einen `RoutePlan`
- `LineTracker`
  folgt nur dem aktuell aktiven `RouteSegment`

Empfohlene Migrationsreihenfolge innerhalb P4:

1. `RouteSegment` und `RoutePlan` einfuehren, aber zunaechst nur fuer heutige `FREE`-Pfadlogik verwenden.
2. `MOW`- und `DOCK`-Folgen schrittweise auf denselben Routentyp umstellen.
   Ist jetzt funktional und strukturell erreicht.
3. `Costmap` aus `GridMap` extrahieren und statische/dynamische Layer trennen.
4. `Planner` als neue Fassade einfuehren, `GridMap` nur noch als Suchalgorithmus dahinter nutzen.
5. `LineTracker` nur noch gegen `RouteSegment` arbeiten lassen, nicht mehr gegen `Map`-Zustandsdetails.

Definition von "fertig" fuer P4:

- `Map` enthaelt keine eingebettete Planner-Logik mehr ausser Mission-/Resume-Verwaltung.
- `GridMap` kennt keine Karten-Semantik mehr direkt, sondern arbeitet nur auf Costmap-/Planner-Daten.
- `LineTracker` liest keine impliziten Missionsflags mehr aus `Map`.
- `MOW`, `DOCK` und replante `FREE`-Abschnitte laufen alle ueber denselben Routentyp.

### Empfohlene Reihenfolge

- [x] Schritt 1: P0-Fixes umsetzen.
- [x] Schritt 2: Datenvertrag in Core und WebUI-Typen kompatibel erweitern.
- [x] Schritt 3: Planner vereinheitlichen und Costmap intern einfuehren.
- [x] Schritt 4: Docking auf neue Planner-Semantik umstellen.
- [x] Schritt 5: WebUI-Editor fuer neue Felder erweitern.
- [ ] Schritt 6: Tests und Validierung vervollstaendigen.
- [x] Schritt 7: Architektur-Nachzug fuer echte Costmap- und Routensemantik umsetzen.
