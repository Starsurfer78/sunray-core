# Implementierungsplan: Robuste Mähplanung

Technische Spezifikation: Sunray Missionsplanung, Vorschau und Laufzeit

## 1. Ziel

Die Missionsplanung muss so umgesetzt werden, dass der Benutzer einen einfachen Standardablauf bekommt:

- Perimeter anlegen
- No-Go-Zonen anlegen
- Dock setzen
- Mähzonen anlegen und konfigurieren
- Mission aus Zonen bauen
- Vorschau ansehen
- Mission starten
- Fortschritt im Dashboard live verfolgen

Die interne Architektur muss sicherstellen, dass Vorschau, gespeicherter Plan, Laufzeitroute und Dashboard-Fortschritt dieselbe Route verwenden.

## 2. Harte Produktanforderungen

### 2.1 Sichtbarer Benutzerablauf

Die UI zeigt nur diese Konzepte:

- Karte
- Zonen
- Mission
- Vorschau
- Dashboard

Die UI zeigt keine internen Planungsbegriffe wie:

- A*
- Costmap
- PlannerContext
- CoverageDecomposer
- TransitPlanner
- RouteValidator

### 2.2 Vorschau

Die Vorschau muss exakt die Route zeigen, die später gefahren wird.

### 2.3 Missionstart

Beim Missionsstart darf keine andere Route erzeugt werden als die, die in der Vorschau angezeigt wurde.

### 2.4 Dashboard

Das Dashboard muss den Fortschritt gegen genau dieselbe Route berechnen, die in der Vorschau angezeigt und von der Runtime gefahren wird.

## 3. Harte Architekturregeln

### Regel A - Eine Wahrheit

Es darf genau eine kompilierte Missionsroute geben.

Nicht erlaubt:

- separate Vorschau-Route
- separate Laufzeit-Route
- separate Dashboard-Route
- implizite Neugenerierung beim Start
- UI-seitige eigene Geometrie- oder Pfadlogik

Erlaubt:

- eine Route wird im Backend kompiliert
- diese Route wird gespeichert
- dieselbe Route wird an Vorschau, Runtime und Dashboard geliefert

### Regel B - WebUI ist Renderer, nicht Planer

Die WebUI darf keine eigene Bahnplanung oder Geometrieplanung durchführen.
Sie rendert ausschließlich Backend-Daten.

### Regel C - Coverage und Transit sind getrennte Planungsschritte

Mähbahnen und Übergänge sind fachlich verschieden und müssen intern getrennt behandelt werden.

### Regel D - Keine stillen Fallbacks

Wenn ein Übergang oder Segment nicht gültig geplant werden kann, darf das System keine Fake-Direktverbindung erzeugen.
Stattdessen:

- Mission als ungültig markieren
- Segment als fehlerhaft markieren
- Vorschau entsprechend kennzeichnen
- Missionsstart blockieren

### Regel E - Route muss validiert werden

Jede kompilierte Route muss vor dem Missionsstart vollständig validiert werden.

### Regel F - Komponenten sind explizite Planungsobjekte

Eine Zone kann aus mehreren Working-Area-Komponenten bestehen.
Diese Komponenten müssen intern explizit modelliert werden.

Nicht erlaubt:

- alle Coverage-Blöcke einer Zone ungeprüft in eine einzige Reihenfolge kippen
- Übergänge zwischen verschiedenen Komponenten als normalen Folgeübergang behandeln
- fehlende Konnektivität durch implizite Direktverbindungen verdecken

Erlaubt:

- Komponenten zuerst getrennt planen
- Reihenfolge zunächst innerhalb einer Komponente optimieren
- Übergänge zwischen Komponenten nur über explizite Policy behandeln

## 4. Zielarchitektur

### 4.1 Map-Modell

Bestehende Verantwortung von Map:

- Perimeter
- Exclusions / No-Go
- Dock
- Zonen
- Planner-Defaults

Map ist Weltmodell, nicht zentrale Missionslogik.

### 4.2 MissionPlanner

Neues oder logisch neu definiertes Modul.

Verantwortung:

- nimmt Map und Benutzer-Missionsauswahl
- erzeugt eine vollständige Missionsroute
- validiert die Route
- liefert die Route an Vorschau, Runtime und Dashboard

Eingaben:

- Map
- Liste ausgewählter Zonen
- Reihenfolge
- Missionsoptionen

Ausgabe:

- CompiledMissionRoute

### 4.3 WorkingAreaComponents

Neues logisches Zwischenmodell zwischen Zonengeometrie und Coverage.

Verantwortung:

- zerlegt die mähbare Arbeitsfläche einer Zone in zusammenhängende Komponenten
- vergibt stabile componentId-Werte innerhalb einer Zone
- trennt echte Inseln von nur lokal getrennten Coverage-Blöcken
- liefert die Grundlage für Coverage-Reihenfolge und Inter-Komponenten-Transitionen

Eine Working-Area-Komponente ist eine zusammenhängende fachlich mähbare Fläche innerhalb einer Zone nach Abzug von Exclusions, No-Go und sonstigen Sperrflächen.

### 4.4 CoveragePlanner

Verantwortung:

- Headland / Randmähen
- Infill / Stripe
- lokale Reihenfolge innerhalb einer Working-Area-Komponente

CoveragePlanner erzeugt nur Coverage-Segmente:

- COVERAGE_EDGE
- COVERAGE_INFILL

Er erzeugt keine globalen freien Übergänge.

### 4.5 TransitPlanner

Verantwortung:

- Verbindung innerhalb einer Komponente
- Verbindung zwischen Komponenten derselben Zone
- Verbindung zwischen Zonen
- Dock-Anfahrt

TransitPlanner darf vorhandene Infrastruktur wie Planner, GridMap und Costmap nutzen.

Er muss Modi unterscheiden:

- WITHIN_COMPONENT
- BETWEEN_COMPONENTS
- INTER_ZONE
- DOCK

### 4.6 RouteValidator

Neues Pflichtmodul.

Verantwortung:

- vollständige Validierung der geplanten Route vor Missionsstart

Prüft:

- alle Punkte in erlaubter Fläche
- alle MOW-Transitionen in zulässiger Fläche oder Zone
- keine Sprünge ohne gültige Verbindung
- keine Kollision mit Exclusions
- keine Segmente außerhalb Perimeter
- konsistente Segmenttypen
- konsistente Zonen- und Komponentenwechsel
- Route vollständig und fahrbar

Ohne erfolgreiche Validierung ist die Mission nicht startbar.

### 4.7 ExecutionEngine

Verantwortung:

- fährt die kompilierte Route ab
- verwaltet aktives Segment
- berechnet Fortschritt
- löst lokales Recovery aus

LineTracker bleibt nur Pfadverfolger.

## 5. Datenmodell

### 5.1 Mindestanforderung an RoutePoint

RoutePoint muss erweitert werden um:

- RouteSemantic semantic
- std::string zoneId
- std::string componentId

Vorgeschlagene Erweiterung:

```cpp
enum class RouteSemantic : uint8_t {
    COVERAGE_EDGE,
    COVERAGE_INFILL,
    TRANSIT_WITHIN_ZONE,
    TRANSIT_BETWEEN_COMPONENTS,
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
    std::string componentId;
};
```

### 5.2 Semantik-Pflicht

Jeder vom Planner erzeugte Punkt muss sinnvolle Semantik tragen:

- Randmähen
- Innenbahn
- Übergang innerhalb derselben Zone
- Übergang zwischen Komponenten derselben Zone
- Übergang zwischen Zonen
- Dock
- Recovery

UNKNOWN ist nur für Legacy oder nicht migrierte Altpfade zulässig, nicht für neue Missionsplanung.

### 5.3 Komponenten-Pflicht

Jeder vom Planner erzeugte Coverage-Punkt muss eine gültige componentId tragen.

Regeln:

- gleiche Zone, gleiche componentId: normaler interner Zusammenhang
- gleiche Zone, andere componentId: expliziter Sonderfall, nie impliziter Standardübergang
- andere Zone: Inter-Zonen-Übergang

### 5.4 Langfristiges Ziel

Später sollte zusätzlich eine Segmentebene eingeführt werden.
Kurzfristig reichen Punkt-Semantik plus zoneId und componentId, solange sie konsequent gesetzt und validiert werden.

## 6. Verbindlicher Ablauf

### 6.1 Karte speichern

Benutzer speichert Karte.
Ergebnis: Map

### 6.2 Mission erstellen

Benutzer wählt:

- Zonen
- Reihenfolge
- Missionsoptionen

Ergebnis: MissionRequest

### 6.3 Mission kompilieren

Backend führt aus:

- Working-Area-Komponenten pro Zone erzeugen
- Coverage pro Komponente erzeugen
- Reihenfolge innerhalb jeder Komponente festlegen
- Komponentenreihenfolge festlegen
- Übergänge per TransitPlanner erzeugen
- Route validieren
- Route speichern

Ergebnis: CompiledMissionRoute

### 6.4 Vorschau anzeigen

WebUI rendert ausschließlich CompiledMissionRoute.

### 6.5 Mission starten

Runtime erhält exakt CompiledMissionRoute.
Es wird nichts neu erzeugt.

### 6.6 Dashboard

Dashboard erhält:

- CompiledMissionRoute
- aktuellen Laufzeitstatus

Fortschritt wird gegen diese Route berechnet.

## 7. Policy für Komponenten-Reihenfolge und Inter-Komponenten-Transitionen

### 7.1 Fachliche Grundunterscheidung

Es gibt zwei fachlich verschiedene Situationen:

1. Mehrere Coverage-Blöcke innerhalb derselben logisch erreichbaren Arbeitsfläche
2. Mehrere real getrennte Working-Area-Komponenten

Nur im ersten Fall ist ein normaler interner Übergang ohne Sonderbehandlung zulässig.

### 7.2 Wann ist ein Übergang erlaubt?

Ein Übergang ist als normaler Intra-Zonen-Übergang nur erlaubt, wenn mindestens eine der folgenden Bedingungen erfüllt ist:

- Start- und Zielpunkt liegen in derselben Working-Area-Komponente
- ein legaler Pfad innerhalb der Zone oder Working Area existiert

Wenn Start und Ziel in verschiedenen Komponenten liegen, darf der Übergang nicht automatisch als normaler TRANSIT_WITHIN_ZONE behandelt werden.

### 7.3 Wann ist ein Übergang ein Sonderfall?

Liegt ein Wechsel zwischen zwei verschiedenen Komponenten vor, muss der Planner explizit unterscheiden:

- legaler Pfad innerhalb global erlaubter Fahrfläche vorhanden
- nur über fachlich erlaubten Sondermodus zulässig
- gar nicht zulässig

Wenn ein legaler globaler Pfad vorhanden und fachlich erlaubt ist, entsteht ein expliziter TRANSIT_BETWEEN_COMPONENTS.

### 7.4 Wann ist ein Übergang ein Fehler?

Ein Übergang ist ein Fehler, wenn:

- keine legale Verbindung existiert
- nur ein verbotener Pfad durch Exclusions oder außerhalb des Perimeters möglich wäre
- die Missionspolicy keinen Komponentenwechsel außerhalb der Zone erlaubt

Dann gilt:

- kein stiller Übergang
- keine Fake-Verbindung
- Route als unvollständig oder ungültig markieren
- optional in getrennte Teilmissionen splitten, aber nie implizit verschleiern

### 7.5 Minimal saubere Start-Policy

Für den ersten belastbaren Implementierungsschritt gilt:

- innerhalb derselben Komponente: normal verbinden
- zwischen verschiedenen Komponenten: global legalen Transit versuchen
- wenn dieser gelingt: explizit als TRANSIT_BETWEEN_COMPONENTS markieren
- wenn dieser nicht gelingt: Route invalidieren

## 8. Praktische Entscheidungslogik

### 8.1 Schrittfolge

1. Working-Area-Komponenten erzeugen
2. Pro Komponente Headland, Infill und lokale Segmentreihenfolge erzeugen
3. Komponenten zueinander anordnen
4. Für jede geplante Komponentenkante Konnektivität prüfen
5. Transit oder Fehler explizit erzeugen

### 8.2 Reihenfolge-Regeln

Die Reihenfolgeoptimierung erfolgt zuerst innerhalb einer Komponente.
Eine globale Reihenfolge über alle Coverage-Segmente derselben Zone ist unzulässig, solange Komponenten nicht explizit behandelt werden.

### 8.3 Konnektivitätsprüfung pro Komponentenkante

Für jede geplante Kante zwischen zwei Komponenten ist in dieser Reihenfolge zu prüfen:

1. legaler Pfad innerhalb der Working Area
2. legaler Pfad innerhalb der Zone
3. legaler Pfad innerhalb global erlaubter Fahrfläche
4. keine legale Verbindung

Ergebnis:

- Fall 1 oder 2: zulässiger Intra-Zonen-Transit
- Fall 3: expliziter TRANSIT_BETWEEN_COMPONENTS
- Fall 4: Fehler oder Split in Teilmissionen

## 9. Verbotene Implementierungsmuster

Diese Muster sind unzulässig:

### 9.1 UI-seitige Planungslogik

Nicht erlaubt:

- Vorschau in TypeScript oder Svelte separat nachbauen
- Geometrie in der WebUI neu berechnen
- eigene Stripe- oder Übergangslogik im Frontend

### 9.2 Implizite Neugenerierung beim Start

Nicht erlaubt:

- beim Start aus zones oder mow eine neue Route bauen, wenn die Vorschau vorher auf etwas anderem basierte

### 9.3 Stille Direktverbindungen

Nicht erlaubt:

- wenn previewPath() fehlschlägt, einfach den Zielpunkt direkt einfügen

### 9.4 Ungültige Mission trotzdem fahren

Nicht erlaubt:

- Routefehler loggen, aber Mission trotzdem als startbar behandeln

### 9.5 Komponenten ignorieren

Nicht erlaubt:

- Coverage mehrerer Komponenten ungeprüft in eine einzige Bahnfolge mischen
- Komponentenwechsel ohne eigene Semantik behandeln
- unterschiedliche Inseln derselben Zone als normalen Stripe-Fortschritt ausgeben

## 10. Bestehender Code: konkrete Anforderungen

### 10.1 Route.h

Pflicht:

- RouteSemantic ergänzen um TRANSIT_BETWEEN_COMPONENTS
- zoneId hinzufügen
- componentId hinzufügen

### 10.2 MowRoutePlanner.cpp

Pflicht:

- semantic setzen
- zoneId setzen
- componentId setzen
- Coverage- und Transitpunkte unterscheiden
- Komponenten erst lokal planen, dann explizit verbinden
- keine stillen Fake-Fallbacks

### 10.3 Map.cpp

Pflicht:

- Runtime darf nicht stillschweigend andere Route verwenden als Vorschau
- startPlannedMowing(...) ist der bevorzugte Einstieg für Runtime
- startMowing() und startMowingZones() dürfen nicht zu einer alternativen Wahrheit werden

### 10.4 neuer RouteValidator

Pflicht:

- vor Missionsstart aufrufen
- bei Fehler Mission blockieren
- unzulässige Komponentenwechsel erkennen

### 10.5 WebUI

Pflicht:

- Vorschau aus Backend-Route rendern
- keine eigene Planungslogik
- Segmenttypen intern farblich differenzieren dürfen
- Komponentenwechsel als eigener Typ unterscheidbar, ohne technische Sprache dem Benutzer aufzuzwingen

## 11. WebUI-Spezifikation

### 11.1 Benutzeroberflächen

Map Editor

Benutzer kann:

- Perimeter anlegen
- No-Go-Zonen anlegen
- Dock setzen
- Zonen anlegen

Mission Planner

Benutzer kann:

- Zonen auswählen
- Reihenfolge festlegen
- Vorschau ansehen

Dashboard

Benutzer kann sehen:

- aktuelle Zone
- Fortschritt
- aktive Bahn oder aktueller Abschnitt
- bisher gefahrene Route
- Restzeit oder Status

### 11.2 Interne Darstellung

Die WebUI darf intern Segmentfarben verwenden, z. B. für:

- Randmähen
- Innenbahnen
- Übergänge innerhalb der Zone
- Übergänge zwischen Komponenten
- Dock
- Recovery

Diese Begriffe müssen dem Benutzer nicht als technische Klassen gezeigt werden.

## 12. Recovery-Spezifikation

### 12.1 Grundsatz

Recovery ist lokal.

Nicht erlaubt:

- komplette Mission neu interpretieren
- Zonenreihenfolge verlieren
- komplette Coverage neu aufbauen

Erlaubt:

- aktuelles Segment lokal umgehen
- auf gültigen Punkt der bestehenden Route zurückkehren
- danach normal fortsetzen

### 12.2 Recovery-Ziel

Recovery repariert nur das aktuelle Problem, nicht die gesamte Mission.

## 13. Validierung

### 13.1 Pflichtprüfungen

Vor Missionsstart muss RouteValidator mindestens prüfen:

- jeder Punkt innerhalb global erlaubter Fläche
- jede MOW-Transition innerhalb fachlich zulässiger Fläche
- kein Segment schneidet harte Exclusions
- kein Segment verlässt unzulässig den Perimeter
- Segmentfolge vollständig
- keine direkten Sprünge ohne Transit
- Route nicht leer
- Route konsistent mit Mission
- Wechsel zwischen Komponenten semantisch und topologisch konsistent

### 13.2 Startbedingung

Mission darf nur starten, wenn Validierung erfolgreich ist.

## 14. Definition of Done

Ein Task zur Missionsplanung ist nur dann fertig, wenn alle Punkte erfüllt sind.

Architektur:

- Es gibt genau eine kompilierte Missionsroute.
- Vorschau verwendet diese Route.
- Runtime verwendet diese Route.
- Dashboard verwendet diese Route.

Datenmodell:

- RoutePoint enthält semantic.
- RoutePoint enthält zoneId.
- RoutePoint enthält componentId.
- neue Punkte erhalten sinnvolle Semantik.

Planung:

- Coverage und Transit werden getrennt behandelt.
- Komponenten werden explizit behandelt.
- keine stillen Direktverbindungen bei Planner-Fehlern.
- jede Route wird vor Start validiert.
- ungültige Mission ist nicht startbar.

WebUI:

- keine eigene Planungslogik im Frontend.
- Vorschau rendert Backend-Daten.
- Dashboard zeigt Fortschritt gegen Backend-Route.

Runtime:

- Missionsstart nutzt geplante Route.
- Recovery wirkt lokal.
- Fortschritt bleibt konsistent.

## 15. Priorisierte Umsetzung

### Phase 1

- RouteSemantic, zoneId und componentId in RoutePoint
- MowRoutePlanner setzt diese Felder
- Vorschau serialisiert diese Daten
- WebUI rendert diese Route

### Phase 2

- CompiledMissionRoute als einzige Wahrheit etablieren
- startPlannedMowing(...) als Standardpfad anbinden
- Vorschau = Runtime erzwingen

### Phase 3

- RouteValidator implementieren
- Missionsstart nur bei gültiger Route erlauben

### Phase 4

- Working-Area-Komponenten explizit modellieren
- Komponenten-Reihenfolge definieren
- BETWEEN_COMPONENTS-Policy umsetzen

### Phase 5

- TransitPlanner explizit trennen
- Recovery lokalisieren

### Phase 6

- Dashboard auf segment- und routenbasierten Fortschritt umstellen

## 16. Konkrete Anweisung an den Coder

Diese Anweisung ist verbindlich:

Implementiere die Missionsplanung so, dass genau eine kompilierte Missionsroute erzeugt wird.
Diese Route ist die einzige Wahrheit für Vorschau, Laufzeit und Dashboard.
Die WebUI darf keine eigene Planungslogik enthalten.
Coverage und Transit sind getrennte interne Schritte.
Working-Area-Komponenten sind explizite Planungsobjekte.
Jede Route muss vor Missionsstart validiert werden.
Ungültige Transitionen dürfen nicht stillschweigend durch Direktverbindungen ersetzt werden.
RoutePoint ist mindestens um semantic, zoneId und componentId zu erweitern.
Die Runtime nutzt bevorzugt eine bereits geplante Route, nicht implizite Neugenerierung aus anderen Datenquellen.
Komponentenwechsel müssen explizit behandelt und semantisch markiert werden.

## 17. Abnahmetests

Der Coder muss mindestens diese Tests liefern.

### Test A - Einfache Rechteckzone

Erwartung:

- plausible Coverage
- keine ungültigen Transitionen
- Vorschau = Runtime

### Test B - Zone nahe Perimeter

Erwartung:

- keine Abkürzung über Perimeter
- Route validierbar

### Test C - Zone mit Exclusion

Erwartung:

- keine Transition durch Exclusion
- Coverage bleibt konsistent

### Test D - mehrere Zonen

Erwartung:

- definierte Reihenfolge
- saubere Übergänge
- Dashboard zeigt richtige aktive Zone

### Test E - Planner-Fehler

Erwartung:

- Mission wird als ungültig markiert
- kein stiller Direktsprung

### Test H - zwei echte Working-Area-Komponenten, global legal verbindbar

Beispiel:

- Zone wird durch No-Go in zwei Inseln geteilt
- außerhalb der Zone, aber innerhalb des Perimeters existiert ein legaler Transit

Erwartung:

- Übergang nur bei explizit erlaubter Policy
- Segment wird als TRANSIT_BETWEEN_COMPONENTS markiert
- Vorschau und Validator behandeln den Wechsel sichtbar und korrekt

### Test I - zwei echte Komponenten, nicht legal verbindbar

Erwartung:

- keine Fake-Verbindung
- Route invalid oder explizit in Teilmissionen gesplittet
- Missionsstart blockiert, wenn keine erlaubte Split-Policy aktiv ist

### Test J - mehrere Komponenten mit unterschiedlicher Reihenfolge

Erwartung:

- Reihenfolge bleibt stabil und reproduzierbar
- Planung springt nicht chaotisch zwischen Komponenten
- Reihenfolge wird zuerst lokal innerhalb der Komponenten optimiert