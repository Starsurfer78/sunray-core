# Implementierungsplan: Robuste Mähplanung
Technische Spezifikation: Sunray Missionsplanung, Vorschau und Laufzeit
1. Ziel

Die Missionsplanung muss so umgesetzt werden, dass der Benutzer einen einfachen Standardablauf bekommt:

Perimeter anlegen
No-Go-Zonen anlegen
Dock setzen
Mähzonen anlegen und konfigurieren
Mission aus Zonen bauen
Vorschau ansehen
Mission starten
Fortschritt im Dashboard live verfolgen

Die interne Architektur muss sicherstellen, dass Vorschau, gespeicherter Plan, Laufzeitroute und Dashboard-Fortschritt dieselbe Route verwenden.

2. Harte Produktanforderungen
2.1 Sichtbarer Benutzerablauf

Die UI zeigt nur diese Konzepte:

Karte
Zonen
Mission
Vorschau
Dashboard

Die UI zeigt keine internen Planungsbegriffe wie:

A*
Costmap
PlannerContext
CoverageDecomposer
TransitPlanner
RouteValidator
2.2 Vorschau

Die Vorschau muss exakt die Route zeigen, die später gefahren wird.

2.3 Missionstart

Beim Missionsstart darf keine andere Route erzeugt werden als die, die in der Vorschau angezeigt wurde.

2.4 Dashboard

Das Dashboard muss den Fortschritt gegen genau dieselbe Route berechnen, die in der Vorschau angezeigt und von der Runtime gefahren wird.

3. Harte Architekturregeln
Regel A — Eine Wahrheit

Es darf genau eine kompilierte Missionsroute geben.

Nicht erlaubt:

separate Vorschau-Route
separate Laufzeit-Route
separate Dashboard-Route
implizite Neugenerierung beim Start
UI-seitige eigene Geometrie-/Pfadlogik

Erlaubt:

eine Route wird im Backend kompiliert
diese Route wird gespeichert
dieselbe Route wird an Vorschau, Runtime und Dashboard geliefert
Regel B — WebUI ist Renderer, nicht Planer

Die WebUI darf keine eigene Bahnplanung oder Geometrieplanung durchführen.
Sie rendert ausschließlich Backend-Daten.

Regel C — Coverage und Transit sind getrennte Planungsschritte

Mähbahnen und Übergänge sind fachlich verschieden und müssen intern getrennt behandelt werden.

Regel D — Keine stillen Fallbacks

Wenn ein Übergang oder Segment nicht gültig geplant werden kann, darf das System keine Fake-Direktverbindung erzeugen.
Stattdessen:

Mission als ungültig markieren
Segment als fehlerhaft markieren
Vorschau entsprechend kennzeichnen
Missionsstart blockieren
Regel E — Route muss validiert werden

Jede kompilierte Route muss vor dem Missionsstart vollständig validiert werden.

4. Zielarchitektur
4.1 Map-Modell

Bestehende Verantwortung von Map:

Perimeter
Exclusions / No-Go
Dock
Zonen
Planner-Defaults

Map ist Weltmodell, nicht zentrale Missionslogik.

4.2 MissionPlanner

Neues oder logisch neu definiertes Modul.

Verantwortung:

nimmt Map + Benutzer-Missionsauswahl
erzeugt eine vollständige Missionsroute
validiert die Route
liefert die Route an Vorschau, Runtime und Dashboard
Eingaben
Map
Liste ausgewählter Zonen
Reihenfolge
Missionsoptionen
Ausgabe
CompiledMissionRoute
4.3 CoveragePlanner

Verantwortung:

Headland / Randmähen
Infill / Stripe
lokale Reihenfolge innerhalb der Coverage-Fläche

CoveragePlanner erzeugt nur Coverage-Segmente:

COVERAGE_EDGE
COVERAGE_INFILL

Er erzeugt keine globalen freien Übergänge.

4.4 TransitPlanner

Verantwortung:

Verbindung zwischen Coverage-Blöcken
Verbindung zwischen Teilflächen
Verbindung zwischen Zonen
Dock-Anfahrt

TransitPlanner darf vorhandene Infrastruktur (Planner, GridMap, Costmap) nutzen.

Er muss Modi unterscheiden:

WITHIN_REGION
WITHIN_ZONE
INTER_ZONE
DOCK
4.5 RouteValidator

Neues Pflichtmodul.

Verantwortung:

vollständige Validierung der geplanten Route vor Missionsstart

Prüft:

alle Punkte in erlaubter Fläche
alle MOW-Transitionen in zulässiger Fläche / Zone
keine Sprünge ohne gültige Verbindung
keine Kollision mit Exclusions
keine Segmente außerhalb Perimeter
konsistente Segmenttypen
Route vollständig und fahrbar

Ohne erfolgreiche Validierung ist die Mission nicht startbar.

4.6 ExecutionEngine

Verantwortung:

fährt die kompilierte Route ab
verwaltet aktives Segment
berechnet Fortschritt
löst lokales Recovery aus

LineTracker bleibt nur Pfadverfolger.

5. Datenmodell
5.1 Mindestanforderung an RoutePoint

RoutePoint muss erweitert werden um:

RouteSemantic semantic
std::string zoneId
Vorgeschlagene Erweiterung
enum class RouteSemantic : uint8_t {
    COVERAGE_EDGE,
    COVERAGE_INFILL,
    TRANSIT_WITHIN_ZONE,
    TRANSIT_INTER_ZONE,
    DOCK_APPROACH,
    RECOVERY,
    UNKNOWN
};
struct RoutePoint {
    Point p;
    bool reverse = false;
    bool slow = false;
    bool reverseAllowed = false;
    float clearance_m = 0.25f;
    WayType sourceMode = WayType::FREE;

    RouteSemantic semantic = RouteSemantic::UNKNOWN;
    std::string zoneId;
};
5.2 Semantik-Pflicht

Jeder vom Planner erzeugte Punkt muss sinnvolle Semantik tragen:

Randmähen
Innenbahn
Übergang innerhalb Zone
Übergang zwischen Zonen
Dock
Recovery

UNKNOWN ist nur für Legacy oder nicht migrierte Altpfade zulässig, nicht für neue Missionsplanung.

5.3 Langfristiges Ziel

Später sollte zusätzlich eine Segmentebene eingeführt werden.
Kurzfristig reicht Punkt-Semantik, solange sie konsequent gesetzt und validiert wird.

6. Verbindlicher Ablauf
6.1 Karte speichern

Benutzer speichert Karte.
Ergebnis: Map

6.2 Mission erstellen

Benutzer wählt:

Zonen
Reihenfolge
Missionsoptionen

Ergebnis: MissionRequest

6.3 Mission kompilieren

Backend führt aus:

Coverage pro Zone erzeugen
Coverage-Reihenfolge festlegen
Übergänge per TransitPlanner erzeugen
Route validieren
Route speichern

Ergebnis: CompiledMissionRoute

6.4 Vorschau anzeigen

WebUI rendert ausschließlich CompiledMissionRoute.

6.5 Mission starten

Runtime erhält exakt CompiledMissionRoute.
Es wird nichts neu erzeugt.

6.6 Dashboard

Dashboard erhält:

CompiledMissionRoute
aktuellen Laufzeitstatus

Fortschritt wird gegen diese Route berechnet.

7. Verbotene Implementierungsmuster

Diese Muster sind ab jetzt unzulässig:

7.1 UI-seitige Planungslogik

Nicht erlaubt:

Vorschau in TypeScript oder Svelte separat nachbauen
Geometrie in der WebUI neu berechnen
eigene Stripe- oder Übergangslogik im Frontend
7.2 Implizite Neugenerierung beim Start

Nicht erlaubt:

beim Start aus zones oder mow eine neue Route bauen, wenn die Vorschau vorher auf etwas anderem basierte
7.3 Stille Direktverbindungen

Nicht erlaubt:

wenn previewPath() fehlschlägt, einfach den Zielpunkt direkt einfügen
7.4 Ungültige Mission trotzdem fahren

Nicht erlaubt:

Routefehler loggen, aber Mission trotzdem als startbar behandeln
8. Bestehender Code: konkrete Anforderungen
8.1 Route.h

Pflicht:

RouteSemantic hinzufügen
zoneId hinzufügen
8.2 MowRoutePlanner.cpp

Pflicht:

semantic setzen
zoneId setzen
Coverage- und Transitpunkte unterscheiden
keine stillen Fake-Fallbacks
8.3 Map.cpp

Pflicht:

Runtime darf nicht stillschweigend andere Route verwenden als Vorschau
startPlannedMowing(...) ist der bevorzugte Einstieg für Runtime
startMowing() / startMowingZones() dürfen nicht zu einer alternativen Wahrheit werden
8.4 neuer RouteValidator

Pflicht:

vor Missionsstart aufrufen
bei Fehler Mission blockieren
8.5 WebUI

Pflicht:

Vorschau aus Backend-Route rendern
keine eigene Planungslogik
Segmenttypen intern farblich differenzieren dürfen
Benutzeroberfläche bleibt einfach
9. WebUI-Spezifikation
9.1 Benutzeroberflächen
Map Editor

Benutzer kann:

Perimeter anlegen
No-Go-Zonen anlegen
Dock setzen
Zonen anlegen
Mission Planner

Benutzer kann:

Zonen auswählen
Reihenfolge festlegen
Vorschau ansehen
Dashboard

Benutzer kann sehen:

aktuelle Zone
Fortschritt
aktive Bahn / aktueller Abschnitt
bisher gefahrene Route
Restzeit / Status
9.2 Interne Darstellung

Die WebUI darf intern Segmentfarben verwenden, z. B.:

Randmähen
Innenbahnen
Übergänge
Dock
Recovery

Diese Begriffe müssen dem Benutzer nicht als technische Klassen gezeigt werden.

10. Recovery-Spezifikation
10.1 Grundsatz

Recovery ist lokal.

Nicht erlaubt:

komplette Mission neu interpretieren
Zonenreihenfolge verlieren
komplette Coverage neu aufbauen

Erlaubt:

aktuelles Segment lokal umgehen
auf gültigen Punkt der bestehenden Route zurückkehren
danach normal fortsetzen
10.2 Recovery-Ziel

Recovery repariert nur das aktuelle Problem, nicht die gesamte Mission.

11. Validierung
11.1 Pflichtprüfungen

Vor Missionsstart muss RouteValidator mindestens prüfen:

jeder Punkt innerhalb global erlaubter Fläche
jede MOW-Transition innerhalb fachlich zulässiger Fläche
kein Segment schneidet harte Exclusions
kein Segment verlässt unzulässig den Perimeter
Segmentfolge vollständig
keine direkten Sprünge ohne Transit
Route nicht leer
Route konsistent mit Mission
11.2 Startbedingung

Mission darf nur starten, wenn Validierung erfolgreich ist.

12. Definition of Done

Ein Task zur Missionsplanung ist nur dann fertig, wenn alle Punkte erfüllt sind.

Architektur
 Es gibt genau eine kompilierte Missionsroute
 Vorschau verwendet diese Route
 Runtime verwendet diese Route
 Dashboard verwendet diese Route
Datenmodell
 RoutePoint enthält semantic
 RoutePoint enthält zoneId
 neue Punkte erhalten sinnvolle Semantik
Planung
 Coverage und Transit werden getrennt behandelt
 keine stillen Direktverbindungen bei Planner-Fehlern
 jede Route wird vor Start validiert
 ungültige Mission ist nicht startbar
WebUI
 keine eigene Planungslogik im Frontend
 Vorschau rendert Backend-Daten
 Dashboard zeigt Fortschritt gegen Backend-Route
Runtime
 Missionsstart nutzt geplante Route
 Recovery wirkt lokal
 Fortschritt bleibt konsistent
13. Priorisierte Umsetzung
Phase 1
RouteSemantic + zoneId in RoutePoint
MowRoutePlanner setzt diese Felder
Vorschau serialisiert diese Daten
WebUI rendert diese Route
Phase 2
CompiledMissionRoute bzw. Route als einzige Wahrheit etablieren
startPlannedMowing(...) als Standardpfad anbinden
Vorschau = Runtime erzwingen
Phase 3
RouteValidator implementieren
Missionsstart nur bei gültiger Route erlauben
Phase 4
TransitPlanner explizit trennen
Recovery lokalisieren
Phase 5
Dashboard auf segment-/routenbasierten Fortschritt umstellen
14. Konkrete Anweisung an den Coder

Diese Anweisung ist verbindlich:

Implementiere die Missionsplanung so, dass genau eine kompilierte Missionsroute erzeugt wird.
Diese Route ist die einzige Wahrheit für Vorschau, Laufzeit und Dashboard.
Die WebUI darf keine eigene Planungslogik enthalten.
Coverage und Transit sind getrennte interne Schritte.
Jede Route muss vor Missionsstart validiert werden.
Ungültige Transitionen dürfen nicht stillschweigend durch Direktverbindungen ersetzt werden.
RoutePoint ist mindestens um semantic und zoneId zu erweitern.
Die Runtime nutzt bevorzugt eine bereits geplante Route, nicht implizite Neugenerierung aus anderen Datenquellen.

15. Abnahmetests

Der Coder muss mindestens diese Tests liefern.

Test A — einfache Rechteckzone

Erwartung:

plausible Coverage
keine ungültigen Transitionen
Vorschau = Runtime
Test B — Zone nahe Perimeter

Erwartung:

keine Abkürzung über Perimeter
Route validierbar
Test C — Zone mit Exclusion

Erwartung:

keine Transition durch Exclusion
Coverage bleibt konsistent
Test D — mehrere Zonen

Erwartung:

definierte Reihenfolge
saubere Übergänge
Dashboard zeigt richtige aktive Zone
Test E — Planner-Fehler

Erwartung:

Mission wird als ungültig markiert
kein stiller Direktsprung
























## ALT #####

Basierend auf der Analyse in `mapping-TXT.md` und dem aktuellen Code-Stand.

---

## Status des aktuellen Codes

### Was bereits funktioniert
- `Map` als Weltmodell (Perimeter, Exclusions, Zonen, Dock)
- `MowRoutePlanner`: Headland + Stripe + Transitionen (nach den letzten Fixes)
- `Planner / GridMap / Costmap`: A*-Pfadplanung mit Costmap-Layern
- `LineTracker`: Stanley-Controller für Segmentabfahrt
- Zonenbewusste Transitionen und deterministisches Stripe-Boustrophedon (letzter Fix)
- WebUI-Vorschau ruft dieselbe C++-Planungslogik wie die Laufzeit auf

### Was noch fehlt (laut Dokument)
1. **`RoutePoint` hat keine Semantik** — kein `zoneId`, kein `segmentKind` → kein Debug, keine zonenspezifische Laufzeitregel
2. **Preview ≠ Runtime**: Preview nutzt `buildMissionMowRoutePreview()`, Runtime nutzt `allMowRoute_` aus JSON `mow:`-Punkten. Wenn `mow: []` leer ist, wird erst bei `startMowing()` on-the-fly generiert — andere Startposition, andere Route.
3. **Kein CoverageDecomposer**: Große/verwinkelte Zonen werden als eine Fläche behandelt. Engstellen, schmale Arme und konkave Bereiche sind nicht explizit erkannt.
4. **Recovery zu global**: Replanning bei Hindernissen wirkt auf die gesamte Route, nicht nur lokal.

---

## Stufe 1 — Sofort: Semantik + Preview = Runtime

**Ziel**: Minimaler Umbau, maximale Stabilitätsverbesserung.

### 1.1 `RoutePoint` semantisch erweitern

**Datei**: `core/navigation/Route.h`

```cpp
enum class RouteSemantic : uint8_t {
    COVERAGE_EDGE,       // Randmähbahn
    COVERAGE_INFILL,     // Innenbahn (Stripe)
    TRANSIT_WITHIN_ZONE, // Übergang innerhalb der Zone
    TRANSIT_INTER_ZONE,  // Übergang zwischen zwei Zonen
    DOCK_APPROACH,       // Einfahrt zur Ladestation
    RECOVERY,            // Wiederherstellungsfahrt
    UNKNOWN,             // Legacy / unbekannt
};
```

`RoutePoint` bekommt:
```cpp
RouteSemantic semantic = RouteSemantic::UNKNOWN;
std::string   zoneId;   // ID der aktiven Zone bei diesem Punkt
```

**Aufwand**: klein. Alle bestehenden Aufrufer müssen nichts ändern (Default-Wert `UNKNOWN`).  
**Nutzen**: Debug-Export, WebUI-Farbkodierung, spätere zonenspezifische Laufzeitregeln.

**MowRoutePlanner.cpp**: `makePoint()` und `routeFromPolyline()` füllen `semantic` und `zoneId`.

---

### 1.2 Preview = Runtime erzwingen

**Problem**: `Map::loadJson()` baut `allMowRoute_` aus den rohen `mow:`-Punkten im JSON.  
`startMowing()` nutzt diese Route. Die Vorschau nutzt `buildMissionMowRoutePreview()`.  
Wenn `mow: []` leer ist (zones-basierter Workflow), wird die Route erst bei `startMowing()` live generiert — mit der aktuellen Roboterposition als Startpunkt statt dem geplanten Einstiegspunkt.

**Fix in `Map::startMowing()` und `Map::startMowingZones()`**:

```cpp
bool Map::startMowing(float robotX, float robotY) {
    // Wenn Zonen vorhanden: Route aus Zonen generieren (nicht aus mow:-Punkten)
    if (!zones_.empty()) {
        allMowRoute_ = generatedMowRouteFromZones(*this);
    }
    activateMowRoute(allMowRoute_);
    // ... Rest wie bisher
}
```

**Datei**: `core/navigation/Map.cpp`  
**Aufwand**: klein. Ein ~5-Zeilen-Block.

---

### 1.3 `appendTransition()` Fallback wenn Planner keine Route findet

**Problem**: Wenn `previewPath()` leer zurückkommt (kein A*-Pfad in der Zone), wird der Übergang stillschweigend übersprungen. Der Roboter springt direkt zum nächsten Punkt.

**Fix in `MowRoutePlanner.cpp`**:

```cpp
static void appendTransition(...) {
    if (samePoint(from, to)) return;
    const RoutePlan transition = map.previewPath(..., constraintZone);
    if (transition.points.empty()) {
        // Fallback: geraden Punkt einfügen — Laufzeit kann darüber hinwegfahren
        // Besser sichtbar als ein stiller Sprung.
        RoutePoint rp;
        rp.p = to;
        rp.semantic = RouteSemantic::TRANSIT_WITHIN_ZONE;
        route.points.push_back(rp);
        return;
    }
    // ... wie bisher
}
```

**Aufwand**: klein.

---

### Deliverable Stufe 1

| # | Änderung | Datei | Aufwand |
|---|----------|-------|---------|
| 1.1 | `RouteSemantic` + `zoneId` in `RoutePoint` | `Route.h`, `MowRoutePlanner.cpp` | 1–2h |
| 1.2 | `startMowing()` generiert Route aus Zonen | `Map.cpp` | 1h |
| 1.3 | Transition-Fallback bei leerem Planner-Ergebnis | `MowRoutePlanner.cpp` | 0.5h |

---

## Stufe 2 — Mittelfristig: Robuste Coverage für komplexe Zonen

**Ziel**: Große, verwinkelte, konkave Zonen korrekt behandeln.

### 2.1 Stripe-Reihenfolge: Mehrere Intervalle pro Zeile korrekt verbinden

**Problem**: Wenn eine Zone konkav ist oder Exclusions hat, entstehen pro Stripe-Zeile mehrere Intervall-Segmente. Heute werden sie in Eingabereihenfolge geschrieben und mit Transitionen verbunden. Bei komplexen Formen kann die Reihenfolge innerhalb einer Zeile suboptimal sein.

**Fix in `buildStripeSegments()`**: Pro Zeile die Intervalle so sortieren und flippen, dass das Ende des letzten Segments möglichst nah am Anfang des nächsten liegt — aber nur *innerhalb einer Zeile*, nicht global.

```cpp
// Pro Zeile: Intervalle nach Abstand vom letzten Endpunkt sortieren
// Dann Flip entscheiden (von links nach rechts oder rechts nach links)
```

**Datei**: `core/navigation/MowRoutePlanner.cpp`  
**Aufwand**: mittel (~2h)

---

### 2.2 `CoverageDecomposer` — Engstellen erkennen und Zonen aufteilen

**Problem**: Wenn eine Zone eine Engstelle hat (z.B. schmaler Durchgang zwischen zwei Bereichen), werden Stripe-Segmente in beiden Bereichen erzeugt, aber die Transitionen müssen durch die Engstelle — was zu schlechten Pfaden oder Abbrüchen führt.

**Neue Datei**: `core/navigation/CoverageDecomposer.h/.cpp`

```cpp
struct CoverageRegion {
    PolygonPoints polygon;       // Teilfläche
    float         preferredAngleDeg = 0.0f;
    Point         entryPoint;    // empfohlener Einstiegspunkt
    Point         exitPoint;     // empfohlener Ausstiegspunkt
};

// Zerlegt eine bereinigte Zonenfläche in mähbare Teilflächen.
// Strategie: Connectivity-Test bei verschiedenen Y-Schnitten.
// Wenn die Breite an einem Schnitt < minPassageWidth: teile dort auf.
std::vector<CoverageRegion> decomposeCoverageArea(
    const PolygonPoints& zonePoly,
    float stripWidth,
    float minPassageWidth = 0.5f);
```

**Integration in `buildMissionMowRoutePreview()`**:

```cpp
const auto regions = decomposeCoverageArea(prepared.zone, settings.stripWidth);
for (const auto& region : regions) {
    // Headland + Stripe pro Region statt pro Zone
    // Übergänge zwischen Regionen mit definiertem Ein-/Austrittspunkt
}
```

**Aufwand**: groß (~8–12h). Kann schrittweise eingeführt werden — zuerst Passthrough (eine Region = ganze Zone), dann schrittweise Erkennung.

---

### 2.3 Infill-Inset geometrisch reparieren

**Problem**: Der Infill-Inset (`offsetPolygon(prepared.zone, infillInset)`) nutzt eine eigene Offset-Implementierung. Bei konkaven Ecken entstehen Selbstüberschneidungen.

**Fix**: Clipper2 statt eigener Offset-Funktion verwenden (wie bereits im TypeScript-Frontend).

```cpp
// In MowRoutePlanner.cpp: offsetPolygon() durch Clipper2-Aufruf ersetzen
// Gibt es als C++-Bibliothek: clipper2lib
```

**Aufwand**: mittel (~3h für Integration + Test)

---

### Deliverable Stufe 2

| # | Änderung | Datei | Aufwand |
|---|----------|-------|---------|
| 2.1 | Zeilen-lokale Intervall-Sortierung | `MowRoutePlanner.cpp` | 2h |
| 2.2 | `CoverageDecomposer` | neu `CoverageDecomposer.h/.cpp` | 8–12h |
| 2.3 | Infill-Inset via Clipper2 | `MowRoutePlanner.cpp` | 3h |

---

## Stufe 3 — Langfristig: TransitPlanner mit Modi + robustes Recovery

**Ziel**: Professionell skalierbar für beliebige Gärten.

### 3.1 Expliziter TransitPlanner mit drei Modi

**Neue Datei**: `core/navigation/TransitPlanner.h/.cpp`

```cpp
enum class TransitMode {
    WITHIN_REGION,  // direkte Verbindung innerhalb einer Teilfläche
    WITHIN_ZONE,    // Verbindung zwischen Regionen derselben Zone (A* mit Zonenconstraint)
    INTER_ZONE,     // Verbindung zwischen verschiedenen Zonen (A* global erlaubt)
};

RoutePlan planTransit(
    const Map& map,
    const Point& from,
    const Point& to,
    TransitMode mode,
    const ZoneSettings& settings,
    const PolygonPoints& constraintZone = {});
```

`appendTransition()` in `MowRoutePlanner` wird ersetzt durch Aufrufe des `TransitPlanner` mit dem richtigen Modus.

**Aufwand**: mittel (~4h, baut auf den bereits vorhandenen Costmap/Planner-Änderungen auf)

---

### 3.2 Recovery lokal begrenzen

**Problem**: Das bestehende Replanning über `GridMap` kann beliebig weit greifen.

**Fix in `MowOp` / `EscapeReverseOp`**: Nach einem Hindernis soll das Replanning nur für den aktuellen Übergang zur nächsten Mähbahn gelten, nicht für die gesamte Route.

```cpp
// Nach Escape: nicht startMowing() von vorne,
// sondern nur nächste erreichbare RoutePoint-Position suchen
// und von dort normal weiterfahren.
```

**Aufwand**: mittel (~4h)

---

### 3.3 Debug-Export aller Segment-Typen in der WebUI

Wenn `RouteSemantic` (Stufe 1.1) vorhanden ist, kann die WebUI die Vorschau farbkodiert rendern:

| Farbe | Typ |
|-------|-----|
| Blau | `COVERAGE_EDGE` (Randmähbahn) |
| Grün | `COVERAGE_INFILL` (Innenbahn) |
| Orange | `TRANSIT_WITHIN_ZONE` |
| Gelb | `TRANSIT_INTER_ZONE` |
| Rot | `RECOVERY` |

**Datei**: `webui-svelte/src/lib/components/Mission/PathPreview.svelte`

Das setzt voraus, dass der `/api/planner/preview`-Endpoint die `semantic`-Felder mit ausgibt.

**Aufwand**: mittel (~3h für Backend-Serialisierung + Frontend-Rendering)

---

### Deliverable Stufe 3

| # | Änderung | Datei | Aufwand |
|---|----------|-------|---------|
| 3.1 | `TransitPlanner` mit Modi | neu `TransitPlanner.h/.cpp` | 4h |
| 3.2 | Recovery lokal begrenzen | `MowOp.cpp`, `EscapeReverseOp.cpp` | 4h |
| 3.3 | Farbkodierte WebUI-Vorschau | `PathPreview.svelte`, `WebSocketServer.cpp` | 3h |

---

## Dateistruktur nach Abschluss aller Stufen

```
core/navigation/
  Route.h                   ← + RouteSemantic, zoneId (Stufe 1.1)
  Map.h / Map.cpp           ← startMowing() nutzt Zonen-Route (Stufe 1.2)
  MowRoutePlanner.h/.cpp    ← semantic gesetzt, Infill via Clipper2 (Stufe 1+2)
  CoverageDecomposer.h/.cpp ← NEU (Stufe 2.2)
  TransitPlanner.h/.cpp     ← NEU (Stufe 3.1)
  PlannerContext.h           ← + constraintZone (bereits fertig)
  Planner.h/.cpp             ← Grid-Erweiterung (bereits fertig)
  GridMap.h/.cpp             ← constraintZone-Weiterleitung (bereits fertig)
  Costmap.h/.cpp             ← Zone-Blocking (bereits fertig)
  LineTracker.h/.cpp         ← unverändert
  StateEstimator.h/.cpp      ← unverändert
```

---

## Reihenfolge und Abhängigkeiten

```
Stufe 1.1 (Semantik)
    └─→ Stufe 1.2 (Preview=Runtime)
            └─→ Stufe 3.3 (Farbkodierung WebUI)
Stufe 1.3 (Transition-Fallback)
    └─→ unabhängig, jederzeit

Stufe 2.1 (Zeilen-Sortierung)
    └─→ unabhängig, sofort machbar
Stufe 2.3 (Clipper2 Inset)
    └─→ Voraussetzung für Stufe 2.2 (saubere Teilflächen)
Stufe 2.2 (CoverageDecomposer)
    └─→ Voraussetzung für Stufe 3.1 (TransitPlanner mit Regionen)

Stufe 3.1 (TransitPlanner)
    └─→ baut auf Stufe 2.2 + vorhandene Costmap-Änderungen auf
Stufe 3.2 (Recovery lokal)
    └─→ unabhängig, aber sinnvoll nach 3.1
```

---

## Qualitätskriterien (aus Dokument)

Nach Abschluss aller Stufen sollte gelten:

- [ ] 100 % der Zonenfläche wird von Stripe-Bahnen abgedeckt (kein ungepflegter Ring, keine Lücken)
- [ ] 0 Fahrt außerhalb der aktiven Zone bei MOW-Transitionen
- [ ] 0 fachlich falsche Zonenwechsel (nur explizit erlaubte Inter-Zone-Übergänge)
- [ ] Vorschau in der WebUI = gespeicherter Plan = gefahrene Route
- [ ] Recovery greift nur lokal, zerstört nicht die Missionsreihenfolge
- [ ] Segmenttyp ist für jeden Wegpunkt bekannt (Debug/Farbkodierung)
