# Sunray-Core — Analyse der Striping / Path-Planning-Logik

> **Stand:** April 2026 | Basis: `core/navigation/MowRoutePlanner.cpp` & `BoustrophedonPlanner.cpp`

---

## 1. Hauptfunktion für die Bahnen-Erzeugung

Es gibt **zwei unabhängige Planner-Implementierungen** im Projekt:

### 1a — `buildMissionMowRoutePreview()` (Produktions-Planner)
```
Datei: core/navigation/MowRoutePlanner.cpp, Zeile 1087
Signatur:
  RoutePlan buildMissionMowRoutePreview(const Map &map, const nlohmann::json &missionJson)
```

Dies ist der **Haupt-Einstiegspunkt** für die Mission-Planung. Er wird von außen über den API-Layer aufgerufen. Intern delegiert er für jeden Zone-Block an:

```
static std::vector<Segment> buildStripeSegments(
    const PolygonPoints &zone,
    const PolygonPoints &perimeter,
    const std::vector<PolygonPoints> &exclusions,
    float stripWidth,
    float angleDeg,
    float exclusionPadding = 0.0f)
// MowRoutePlanner.cpp, Zeile 567
```

### 1b — `BoustrophedonPlanner::plan()` (alternativer Planner)
```
Datei: core/navigation/BoustrophedonPlanner.cpp, Zeile ~390
Signatur:
  ZonePlan BoustrophedonPlanner::plan(
      const std::string &zoneId,
      const PolygonPoints &zone,
      const PolygonPoints &perimeter,
      const std::vector<PolygonPoints> &hardExclusions,
      const ZoneSettings &settings)
```

Liefert `ZonePlan` mit `std::vector<Waypoint>` (ältere Datenstruktur). Intern delegiert er an:
```
static std::vector<Seg> buildStripeSegs(
    const PolygonPoints &infillPoly,
    const std::vector<PolygonPoints> &exclusions,
    float stripWidth,
    float angleDeg)
// BoustrophedonPlanner.cpp, Zeile 261
```

Beide Planner implementieren **dieselbe Boustrophedon-Logik** (Scanline + Rotation), aber mit unterschiedlichen Ausgabe-Typen (`RoutePlan` vs. `ZonePlan`).

---

## 2. Wo der Striping-Winkel definiert wird

### Definition in `ZoneSettings` (Map.h, Zeile 77–88):
```cpp
struct ZoneSettings {
    std::string name  = "Zone";
    float stripWidth  = 0.18f;  // Schnittbreite in Metern
    float angle       = 0.0f;   // Striping-Winkel in Grad  ← HIER
    bool edgeMowing   = true;
    int edgeRounds    = 1;
    float speed       = 1.0f;   // m/s
    ZonePattern pattern = ZonePattern::STRIPE;
    bool reverseAllowed = false;
    float clearance   = 0.25f;  // Planner-Abstand in Metern
};
```

**Default: 0° (horizontal, d.h. Bahnen laufen in X-Richtung)**

### Woher kommt der Winkel zur Laufzeit?

Der Winkel wird **nicht in der Map gespeichert** (kein Feld im `Zone`-Struct, kein JSON-Feld in `zones[].polygon`).  
Er wird **per Mission-JSON übergeben** über das `overrides`-Objekt:

```json
{
  "zoneIds": ["z1"],
  "overrides": {
    "z1": {
      "angle": 45.0,
      "stripWidth": 0.25
    }
  }
}
```

In `applyMissionOverrides()` (MowRoutePlanner.cpp, Zeile 839):
```cpp
settings.angle = overrides.value("angle", settings.angle);
```

Der Snapshot wird in `ZonePlanSettingsSnapshot` gespeichert (für die Route enthalten), aber die Map selbst persistiert den Winkel **nicht**.

---

## 3. Wie die Stripen-Abstände berechnet werden

### Kernlogik in `buildStripeSegments()` (MowRoutePlanner.cpp ab Zeile 567):

```cpp
// Startpunkt der ersten Bahn: halbe Schnittbreite ab unterer Kante
for (float y = minY + stripWidth * 0.5f;
     y <= maxY + stripWidth * 0.01f;
     y += stripWidth, ++rowIdx)
```

**Erklärung:**
- Erste Bahn: `y = minY + 0.5 × stripWidth` → Mittellinie der ersten Bahn liegt bei +½ Schnittbreite
- Folgende Bahnen: `y += stripWidth` → **konstanter Abstand = exakt `stripWidth`**
- Letzte Bahn: Toleranz `+ 0.01 × stripWidth` → verhindert ein Off-by-One durch Floating-Point-Fehler

**Kein spezieller "letzte Bahn"-Algorithmus vorhanden.** Wenn der Platz nicht aufgeht, ist die letzte Bahn entweder kürzer (durch den Polygon-Clip) oder fehlt. Es gibt keine Anpassung/Streckung für die Randzone.

### Scanline-Schnittpunkt-Algorithmus (`polygonIntervalsAtY()`):
```cpp
for (size_t i = 0; i < poly.size(); ++i) {
    const Point &a = poly[i], &b = poly[(i + 1) % poly.size()];
    if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y)) {
        const float t = (y - a.y) / (b.y - a.y);
        xs.push_back(a.x + t * (b.x - a.x));  // linearer Interpolation
    }
}
```
Liefert paarweise X-Koordinaten → `Interval{left, right}`.

---

## 4. Eingaben der Haupt-Planungsfunktion

### `buildStripeSegments()`:
| Parameter | Typ | Bedeutung |
|---|---|---|
| `zone` | `PolygonPoints` | Zone-Polygon (vorverarbeitet mit Clipper2) |
| `perimeter` | `PolygonPoints` | Außen-Perimeter (für Schnittberechnung) |
| `exclusions` | `vector<PolygonPoints>` | Liste der harten No-Go-Zonen |
| `stripWidth` | `float` | Schnittbreite in Metern (Roboter-Mähbreite) |
| `angleDeg` | `float` | Striping-Winkel in Grad |
| `exclusionPadding` | `float` | Sicherheitsabstand um Exclusions (Standard: 0) |

### `buildMissionMowRoutePreview()`:
| Parameter | Typ | Bedeutung |
|---|---|---|
| `map` | `const Map &` | Enthält: Perimeter, Zonen, Exclusions, Planner-Settings |
| `missionJson` | `nlohmann::json` | `{"zoneIds": [...], "overrides": {...}}` |

### Vorverarbeitung mit Clipper2:
Vor der Stripe-Generierung berechnet `computeWorkingArea()`:
```
WorkingArea = zone ∩ perimeter − hardExclusions
```
Das Ergebnis kann **mehrere Polygone** sein (wenn Exclusions die Zone teilen).

---

## 5. Ausgabe der Planungsfunktion

### `buildStripeSegments()` → `std::vector<Segment>`
```cpp
struct Segment { Point a; Point b; };
```
Jeder `Segment` = eine Mäh-Bahn mit Start- und Endpunkt als XY-Koordinaten (lokales Koordinatensystem des Roboters, in Metern).

### `buildMissionMowRoutePreview()` → `RoutePlan`
```cpp
struct RoutePlan {
    std::vector<RoutePoint> points;  // Liste aller Wegpunkte
    bool valid = true;
    std::string invalidReason;
    WayType sourceMode;
    bool active = false;
};

struct RoutePoint {
    Point p;                   // XY-Koordinate
    bool reverse;              // Rückwärts fahren?
    bool slow;                 // Langsam-Zone
    float clearance_m;        // Sicherheitsabstand
    WayType sourceMode;        // MOW / DOCK / TRANSIT
    RouteSemantic semantic;    // COVERAGE_INFILL / COVERAGE_EDGE / TRANSIT_WITHIN_ZONE / ...
    std::string zoneId;        // z.B. "z1"
    std::string componentId;   // z.B. "z1:c1" (bei geteilten Zonen)
    bool reverseAllowed;
};
```

### `BoustrophedonPlanner::plan()` → `ZonePlan`
```cpp
struct ZonePlan {
    std::string zoneId;
    std::vector<Waypoint> waypoints;  // {x, y, mowOn}
    Point startPoint;
    Point endPoint;
    bool valid;
    std::string invalidReason;
};
```

---

## 6. Rotationsmatrix und Koordinatentransformation

### Ja — es wird eine vollständige Rotationsmatrix verwendet.

**Schritt 1: Rotation ins "gerade" Koordinatensystem** (Rot um `-angle`):
```cpp
// MowRoutePlanner.cpp, Zeile 54-61:
static PolygonPoints rotatePolygon(const PolygonPoints &poly, float angleRad) {
    const float cosA = std::cos(angleRad);
    const float sinA = std::sin(angleRad);
    for (const auto &p : poly)
        out.push_back(rotatePoint(p, cosA, sinA));
}

static Point rotatePoint(const Point &p, float cosA, float sinA) {
    return {
        p.x * cosA - p.y * sinA,   // x' = x·cos(α) − y·sin(α)
        p.x * sinA + p.y * cosA    // y' = x·sin(α) + y·cos(α)
    };
}
```

Im `buildStripeSegments()`:
```cpp
const float angle = angleDeg * M_PI / 180.0f;
const PolygonPoints rotZone      = rotatePolygon(zone,      -angle);  // −α  (Hinrotation)
const PolygonPoints rotPerimeter = rotatePolygon(perimeter, -angle);
// Exclusions werden ebenfalls rotiert
```

**Schritt 2: Scanline im rotierten System** → alle Bahnen sind jetzt horizontal (y = const).

**Schritt 3: Rück-Rotation** (+α) auf Welt-Koordinaten:
```cpp
// Zeile 634-635:
const Point a = rotatePoint({interval.left,  y}, cosPos, sinPos);  // +α (Rückrotation)
const Point b = rotatePoint({interval.right, y}, cosPos, sinPos);
```

**Zusammenfassung:**
- `cos()` und `sin()` werden explizit berechnet
- Perimeter-Punkte werden transformiert (rotiert, Scanline, rückrotiert)
- Die Ausgabe-Punkte liegen wieder in Welt-Koordinaten (GPS-Lokalsystem des Roboters)
- Keine Koordinaten-Drift, da Rotation mathematisch exakt invertierbar ist

---

## 7. Erzeugung der parallelen Linien (Boustrophedon-Loop)

```cpp
// MowRoutePlanner.cpp, Zeilen 628-645
int rowIdx = 0;
for (float y = minY + stripWidth * 0.5f;
     y <= maxY + stripWidth * 0.01f;
     y += stripWidth, ++rowIdx)
{
    // Schnittpunkte der Scanline mit dem (rotierten) Polygon
    std::vector<Interval> intervals = effectiveIntervalsAtY(rotZone, rotPerimeter, rotExclusions, y);

    // Boustrophedon: ungerade Zeilen werden umgekehrt (Schlangen-Muster)
    if (rowIdx % 2 != 0)
        std::reverse(intervals.begin(), intervals.end());

    for (const auto &interval : intervals) {
        const Point a = rotatePoint({interval.left,  y}, cosPos, sinPos);
        const Point b = rotatePoint({interval.right, y}, cosPos, sinPos);
        segments.push_back(rowIdx % 2 == 0 ? Segment{a, b} : Segment{b, a});
    }
}
```

### Antworten auf die konkreten Fragen:

| Frage | Antwort |
|---|---|
| Loop mit `y += schnittbreite`? | **Ja.** Exakt: `y += stripWidth` |
| Wo ist `stripWidth` definiert? | `ZoneSettings::stripWidth` (Map.h:80), Default `0.18f` m |
| Basiert auf Mähbreite des Roboters? | **Ja** — sollte der physischen Schneidbreite entsprechen (konfigurierbar per Mission-JSON) |
| Anpassung für letzte Linie? | **Nein** — keine spezielle Anpassung. Toleranz `+0.01×stripWidth` verhindert nur Float-Auslassung |
| Boustrophedon? | **Ja** — gerade Zeilen links→rechts, ungerade rechts→links |

### Beispiel: 20 m × 20 m Feld, 0.5 m Schnittbreite:
```
minY = 0.0
Erste Bahn: y = 0.0 + 0.5×0.5 = 0.25
Letzte Bahn: y ≤ 20.0 + 0.01×0.5 = 20.005
Bahnen: y = 0.25, 0.75, 1.25, ..., 19.25, 19.75 → 40 Bahnen ✓
```
**Erwartete 40 Bahnen werden erzeugt. ✓**

---

## 8. Scanline-Polygon-Schnitt-Algorithmus

### `polygonIntervalsAtY()` — Even-Odd Scanline:
```cpp
static std::vector<Interval> polygonIntervalsAtY(const PolygonPoints &poly, float y) {
    std::vector<float> xs;
    for (size_t i = 0; i < poly.size(); ++i) {
        const Point &a = poly[i], &b = poly[(i + 1) % poly.size()];
        // Kante kreuzt die Scanline?
        if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y)) {
            const float t = (y - a.y) / (b.y - a.y);      // Parameter t ∈ [0,1]
            xs.push_back(a.x + t * (b.x - a.x));           // X-Schnittpunkt
        }
    }
    std::sort(xs.begin(), xs.end());  // Paarweise: [x0,x1], [x2,x3] = Intervalle
    // Intervalle: [xs[0],xs[1]], [xs[2],xs[3]], ...
}
```

### Wie wird mit Exclusions umgegangen?

`effectiveIntervalsAtY()` berechnet:
```
Intervalle = poly_intervals(zone) ∩ poly_intervals(perimeter) − poly_intervals(exclusion₁) − poly_intervals(exclusion₂) − ...
```

Via `intersectIntervals()` und `subtractIntervals()`:
- **Intersect**: Two-Pointer-Algorithmus auf sortierten Intervallen → `O(n+m)`
- **Subtract**: Cursor-Durchlauf mit Cuts → Nicht-überlappende Ausgabe-Intervalle

### Konkreter Ablauf je Bahn:
1. Scanline trifft Zone → 1–N Intervalle (bei konkavem Polygon: mehrere!)
2. Intersect mit Perimeter-Intervall → kürzt Über-Begrenzung
3. Subtract Exclusion-Intervalle → löcht heraus
4. Ergebnis: Liste von `{x_start, x_end}` → je 1 Segment pro Intervall

### Was passiert bei Bahnen außerhalb des Perimeters?
- `polygonIntervalsAtY()` gibt **leere Liste** zurück → keine Segmente erzeugt
- Die Bahn wird still übersprungen (kein Fehler)

### Komplexe Perimeter (Inseln, Korridore):
- **Inseln (Holes)**: Clipper2 Differenz entfernt Exclusions **bevor** der Planner läuft → `workingArea` ist bereits "sauber"
- **Korridore** (schmale Verbindungen): funktionieren, solange min. 1× `stripWidth` breit. Bei schmalerem Korridor liefert die Scanline 0 Intervalle → Korridor wird übersprungen
- **Komplexere No-Go-Shapes**: `subtractIntervals()` kann mehrere Cuts in einem Intervall subtrahieren → korrekt implementiert

---

## 9. Validierung der erzeugten Stripen

### Liegen Start/Ende auf dem Perimeter?
**Nicht notwendigerweise.** Die Punkte liegen an den **Schnittpunkten der Scanline mit dem Polygon-Rand** — das kann eine Zone-Grenze oder ein Exclusion-Rand sein. Sie sind aber **garantiert innerhalb der `effectiveIntervals`**, also innerhalb von `zone ∩ perimeter − exclusions`.

### Liegt die gesamte Linie INNERHALB des Perimeters?
**Ja** — durch Konstruktion: die Scanline-Intervalle werden mit dem Perimeter-Intervall überschnitten (`intersectIntervals`). Kein Punkt liegt außerhalb.

### Sind die Stripen-Linien wirklich parallel zueinander?
**Ja** — alle Scanlines im rotierten System haben identischen Winkel (horizontal), der Winkel der Rückrotation ist für alle gleich. Mathematisch exakt parallel.

### Ist der Abstand zwischen den Linien konstant?
**Ja** — `y += stripWidth` ist exakt. Floating-Point-Akkumulation kann nach vielen Schritten minimal abweichen (bei 1000+ Bahnen theoretisch ~1 ULP), aber für praktische Felder (<100 Bahnen) vernachlässigbar.

### Gibt es Überlappungen zwischen Stripen?
**Keine Streifen-Überlappungen** durch den Algorithmus — außer wenn `stripWidth` kleiner als die tatsächliche Schneidbreite des Messers konfiguriert wird (Konfigurationsfehler, kein Code-Fehler).

### Gibt es Lücken > 1 cm zwischen den Stripen?
**Nein** — bei korrekter Konfiguration (`stripWidth` = physischer Schneidbreite) liegen die Mittellinien exakt `stripWidth` auseinander. Lücken entstehen nur bei zu großem `stripWidth`.

---

## 10. Wende-Erzeugung zwischen zwei Stripen

Es gibt **keine explizite "Wende"-Logik** mit Kreisbogen oder außerhalb des Perimeters. Stattdessen:

### Zone-gebundene A\*-Transition (innerhalb der Zone):

Zwischen zwei Segmenten ruft `appendSegmentRoute()` → `appendTransition()` auf:

```cpp
// MowRoutePlanner.cpp, appendTransition(), Zeile 874:
static void appendTransition(RoutePlan &route, const Map &map,
                              const Point &from, const Point &to,
                              const ZoneSettings &settings,
                              const PolygonPoints &zonePoly, ...)
{
    const RoutePlan transition = map.previewPath(
        from, to,
        WayType::MOW, WayType::MOW,
        0.0f, false,
        settings.reverseAllowed,
        settings.clearance,
        settings.clearance + map.plannerSettings().obstacleInflation_m,
        zonePoly);  // ← A* bleibt in dieser Zone!
    // ...
}
```

- **Eingabe**: `from` = Ende der aktuellen Bahn, `to` = Anfang der nächsten Bahn
- **Algorithmus**: A\* auf Grid-Map (`GridMap` / `Costmap`), beschränkt auf `zonePoly`
- **Ausgabe**: Wegpunkte als `TRANSIT_WITHIN_ZONE`-Punkte in der Route

### Im `BoustrophedonPlanner` (älterer Planner):
```cpp
auto transit = [&](float x, float y) {
    appendWaypoint(wps, x, y, false);  // mowOn=false = kein Mähen während Wende
};
// Direkte Linie vom Ende Bahn N zum Start Bahn N+1
transit(stripe.a.x, stripe.a.y);
cover(stripe.a.x, stripe.a.y);
```
Hier ist es eine **direkte gerade Verbindung** (mowOn=false).

### Zusammenfassung:

| Eigenschaft | `MowRoutePlanner` | `BoustrophedonPlanner` |
|---|---|---|
| Wende-Algorithmus | A\* innerhalb Zone | Direkte Linie |
| Perimeter überschreitbar? | **Nein** (Zone-Constraint im A\*) | Nein (direkte Linie, nicht validiert) |
| Semantic der Wende-Punkte | `TRANSIT_WITHIN_ZONE` | `mowOn=false` |
| Kürzeste Verbindung? | A\* findet günstigsten Pfad | Immer direkte Linie |
| Kein-Pfad-Behandlung | Route → `valid=false` | Nicht geprüft |

**Wende-Korridor**: Es gibt keine dedizierte "Headland Turn"-Logik. Der Roboter fährt die Wende innerhalb der Zone. Wenn Headland aktiv ist (`edgeMowing=true`), wird zuerst ein Randstreifen-Contour abgefahren — dieser dient indirekt als Wendefläche.

---

## 11. Unterstützung verschiedener Striping-Winkel

### Verschiedene Winkel per Session?
**Ja, vollständig unterstützt.** Der Winkel wird pro Mission im `overrides`-JSON übergeben:

```json
{
  "zoneIds": ["z1"],
  "overrides": {
    "z1": {
      "angle": 45.0
    }
  }
}
```

Auflösung in `applyMissionOverrides()`:
```cpp
settings.stripWidth   = overrides.value("stripWidth",   settings.stripWidth);
settings.angle        = overrides.value("angle",        settings.angle);       // ←
settings.edgeMowing   = overrides.value("edgeMowing",   settings.edgeMowing);
settings.edgeRounds   = overrides.value("edgeRounds",   settings.edgeRounds);
settings.speed        = overrides.value("speed",        settings.speed);
settings.pattern      = ...;
settings.reverseAllowed = overrides.value("reverseAllowed", settings.reverseAllowed);
settings.clearance    = overrides.value("clearance",    settings.clearance);
```

### Automatische Winkel-Optimierung?
**Nein.** Es gibt keinen Algorithmus zur automatischen Winkel-Optimierung (z.B. „längste Bahnen zuerst"). Der Winkel ist ein reiner Input-Parameter.

### Wo wird der Winkel persistiert?
**Nicht in der Map-Datei** (`Map::exportMapJson()` exportiert keinen Zone-Settings-Winkel). Der Winkel lebt nur im Mission-JSON. Die Map enthält nur Geometrie (Perimeter, Zonen-Polygon, Exclusions, Dock).

### Session-zu-Session-Änderung:
Um täglich den Winkel zu ändern (z.B. Tag1=0°, Tag2=45°, Tag3=90°), muss:
- Das Mission-JSON mit dem neuen `angle`-Override gespeichert/übergeben werden
- **Oder** die API-Schicht kümmert sich um Rotation-Scheduling (kein Code dafür im `sunray-core` gefunden)

### `ZonePlanSettingsSnapshot` — Protokollierung des verwendeten Winkels:
```cpp
static ZonePlanSettingsSnapshot snapshotZoneSettings(const ZoneSettings &settings) {
    snapshot.stripWidth_m = settings.stripWidth;
    snapshot.angle_deg    = settings.angle;       // ← gespeichert im Snapshot
    snapshot.edgeMowing   = settings.edgeMowing;
    // ...
}
```
Der Snapshot wird in der Route gespeichert, sodass im Nachhinein nachvollziehbar ist, mit welchem Winkel eine Route geplant wurde.

---

## 12. Zusammenfassung — Architektur-Übersicht

```
buildMissionMowRoutePreview(map, missionJson)
  │
  ├── applyMissionOverrides(ZoneSettings{}, overrides)
  │     └── liest angle, stripWidth, edgeMowing, speed, pattern, ...
  │
  ├── prepareCoverageArea(zone, map)
  │     └── sammelt zone, perimeter, hardExclusions
  │
  ├── computeWorkingArea(zone ∩ perimeter − exclusions)  [Clipper2]
  │     └── → 1..N Polygone (Komponenten)
  │
  ├── sortComponentsStable(components)    [deterministisch, x-Zentroid]
  │
  └── für jede Komponente:
        ├── [Headland] offsetPolygon(compPoly, inset)  [Clipper2]
        │     └── appendContourRuns → COVERAGE_EDGE-Punkte
        │
        ├── buildStripeSegments(infillPoly, perimeter, exclusions,
        │                       stripWidth, angleDeg, exclusionPadding)
        │     ├── rotatePolygon(zone/perimeter/exclusions, -angle)  [cos/sin]
        │     ├── Scanline-Loop: y = minY + 0.5×sw; y += sw
        │     │     ├── polygonIntervalsAtY(rotZone, y)
        │     │     ├── intersectIntervals(…, polygonIntervalsAtY(rotPerimeter, y))
        │     │     ├── subtractIntervals(…, polygonIntervalsAtY(rotExcl, y))
        │     │     └── rotatePoint(a/b, +cos, +sin)  [Rückrotation]
        │     └── → vector<Segment>
        │
        └── appendSegmentsWithTransitions(route, segments, ...)
              └── für jedes Segment:
                    ├── appendTransition(from, to)  [A* innerhalb Zone]
                    └── push start/end als COVERAGE_INFILL-Punkte
```

---

## 13. Datei-Referenzen

| Datei | Inhalt |
|---|---|
| `core/navigation/MowRoutePlanner.cpp` | Haupt-Produktions-Planner (RoutePlan-Ausgabe) |
| `core/navigation/BoustrophedonPlanner.cpp` | Alternativer self-contained Planner (ZonePlan/Waypoint-Ausgabe) |
| `core/navigation/MowRoutePlanner.h` | API-Header für buildMissionMowRoutePreview |
| `core/navigation/BoustrophedonPlanner.h` | API-Header für BoustrophedonPlanner::plan() |
| `core/navigation/Map.h` | `ZoneSettings`, `Zone`, `PlannerSettings` Structs |
| `core/navigation/Map.cpp` | `loadJson()`, Winkel kommt **nicht** aus Map-JSON |
| `core/navigation/Route.h` | `RoutePlan`, `RoutePoint`, `ZonePlanSettingsSnapshot` |
| `core/navigation/GridMap.cpp` | A\* Grid-Aufbau für Transitionen |
| `core/navigation/Costmap.cpp` | Costmap mit Perimeter/Exclusion-Markierungen |
| `core/navigation/Planner.cpp` | A\* Pfad-Planung (Transitionen zwischen Bahnen) |
