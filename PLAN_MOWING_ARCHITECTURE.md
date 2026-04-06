# Implementierungsplan: Robuste Mähplanung

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
