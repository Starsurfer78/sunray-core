Erstelle mir einen Plan um die Fehler im Code zu finden, was man genauer untersuchen sollte und was man unbedingt verbessern sollte
Nachgedacht für ein paar Sekunden

Hier ist ein belastbarer Prüfplan, der nicht rät, sondern die Fehler systematisch auftrennt.

Ziel

Die beiden Symptome sauber isolieren:

Coverage-Fehler
Radmähen / Stripe-Planung deckt die Zone nicht vollständig ab.
Transition-/Routing-Fehler
Um von A nach B zu kommen, plant das System einen fachlich falschen Übergang, teilweise mit Abkürzung über verbotene Bereiche.

Die Codebasis trennt diese beiden Probleme bereits implizit:

Coverage entsteht in MowRoutePlanner.cpp
Übergänge / Pfadplanung laufen über appendTransition(...) → map.previewPath(...) → Planner / GridMap / Costmap
1. Erst die Fehlerbilder hart trennen
1.1 Coverage isoliert prüfen

Untersuchen, ob die eigentlichen Mähsegmente schon falsch sind, bevor irgendein Planner-Übergang eingefügt wird.

Was genau prüfen

In MowRoutePlanner.cpp getrennt dumpen:

Ergebnis von buildEdgeRoute(...)
Ergebnis von buildStripeRoute(...)
finale Route ohne appendTransition(...)
finale Route mit appendTransition(...)
Warum

Wenn die Zone schon ohne Transitionen Lücken hat, liegt der Fehler im Coverage-Generator.
Wenn sie ohne Transitionen korrekt ist und erst mit Transitionen falsch wird, liegt der Fehler in der Übergangslogik.

Konkret loggen

Für jede erzeugte Teilroute:

Anzahl Punkte
Bounding Box
minimale Distanz zum Zonenrand
minimale Distanz zum Perimeter
ob Punkte außerhalb der erlaubten Fläche liegen
ob aufeinanderfolgende Segmente die Zone verlassen
1.2 Transitionen isoliert prüfen

Jeden Planner-Übergang einzeln prüfen.

Was genau prüfen

Vor und nach jedem appendTransition(...) loggen:

from
to
missionMode
planningMode
clearance
erzeugte Wegpunkte
Ergebnis von map.pathAllowed(...)
ob irgendein Segment Perimeter oder Exclusions schneidet
Warum

appendTransition(...) verwendet den allgemeinen Planner für MOW-Übergänge. Genau dort entsteht sehr wahrscheinlich das „kürzester Weg auf die andere Seite“-Verhalten.

2. Was im Coverage-Code genauer untersucht werden sollte
2.1 Headland/Infill-Anschluss

Das ist der erste Hauptverdächtige.

Codebereich
offsetPolygonRounded(...)
buildEdgeRoute(...)
buildStripeRoute(...)
Warum verdächtig

Mit deinen Daten gilt:

edgeMowing = true
edgeRounds = 1
stripWidth = 0.18

Damit läuft:

Headland bei ungefähr 0.09 m
Infill ab ungefähr 0.18 m

Das erzeugt konstruktiv eine potenzielle Lücke oder Überlappungs-/Clip-Artefakte zwischen Randfahrt und Stripe-Infill.

Unbedingt prüfen
Ist das Headland-Inset und das Infill-Inset geometrisch konsistent?
Gibt es einen ungemähten Ring zwischen Headland und erster Stripe?
Wird die erste Stripe zu aggressiv eingezogen?
Wird durch Rundung an Ecken Fläche verloren?
Testfälle
einfache Rechteckzone
schräge konvexe Zone
Zone mit enger Ecke
Zone dicht am Perimeter
Zone mit Exclusion nahe am Rand

Wenn schon das Rechteck nicht lückenlos ist, ist der Fehler fundamental.

2.2 Stripe-Intervalle und Clipping
Codebereich
polygonIntervalsAtY(...)
effectiveIntervalsAtY(...)
buildStripeSegments(...)
Warum verdächtig

Die Stripes werden nicht nur gegen die Zone berechnet, sondern zusätzlich gegen:

Perimeter
Exclusions

geschnitten.

Wenn Zone und Perimeter dicht zusammenliegen, können Randstreifen ungewollt abgeschnitten werden.

Unbedingt prüfen
Welche Intervalle liefert nur die Zone?
Welche Intervalle bleiben nach Schnitt mit Perimeter?
Welche verschwinden nach Subtraktion der Exclusions?
Werden sehr kleine Restintervalle stillschweigend verworfen?
Gibt es numerische Fehler an Polygonkanten und Eckpunkten?
Konkretes Debug-Format

Für jede Stripe-Zeile:

y
Rohintervalle aus Zone
nach Perimeter-Schnitt
nach Exclusion-Schnitt
resultierende Segmente in Weltkoordinaten

Damit sieht man sofort, wo Fläche verschwindet.

2.3 Nearest-Neighbour-Sortierung der Segmente
Codebereich
sortSegmentsNearestNeighbour(...)
Warum verdächtig

Diese Sortierung optimiert Reihenfolge nach Nähe, aber nicht nach Mählogik.
Dadurch können Segmente in eine Reihenfolge kommen, die fachlich ungünstig ist und unnötige oder problematische Übergänge erzeugt.

Unbedingt prüfen
Werden Segmente derselben Stripe sauber als Bahnfolge verbunden?
Springt die Sortierung auf ein „nächstes“ Segment, das geometrisch nah, aber fachlich falsch ist?
Erzeugt die Sortierung viele Transitionen durch schmale Stellen?
Verbesserung

Für Stripe-Mähen besser:

zeilenweise Reihenfolge beibehalten
nur innerhalb einer Zeile flippen
keine globale Nearest-Neighbour-Sortierung für Coverage-Segmente

Das wäre robuster und vorhersagbarer.

3. Was im Planner-/Transition-Code genauer untersucht werden sollte
3.1 appendTransition(...) für MOW ist zu allgemein
Problem

MOW-Transitionen werden wie allgemeine freie Pfade behandelt. Das ist fachlich zu grob.

Unbedingt prüfen
Wird bei MOW-Transitionen auf die Zone Rücksicht genommen oder nur auf „erlaubte Gesamtfläche“?
Darf der Planner legal durch andere Zonenbereiche fahren, obwohl die Coverage dort gerade nicht hin soll?
Wird nur perimeter/exclusion berücksichtigt, aber nicht die aktive Zone?
Erwartetes Problem

Der Planner kennt global „erlaubte Fläche“, aber nicht „bleib in dieser Mähzone“.
Dann sind Abkürzungen innerhalb der Gesamtfläche zwar technisch erlaubt, aber fachlich falsch.

Verbesserung

Für WayType::MOW:

nicht nur isInsideAllowedArea, sondern aktive Zone als harte Nebenbedingung
Übergänge nur innerhalb der aktuellen Coverage-Fläche oder derselben Zone planen
3.2 Pfadglättung kann fachlich problematisch sein
Codebereich
GridMap::smoothPath(...)
danach map.pathAllowed(candidatePath)
Warum prüfen

Auch wenn der A*-Pfad okay ist, kann Glättung Abkürzungen erzeugen, die fachlich schlecht sind, besonders an engen Polygonkanten.

Unbedingt prüfen
Wird nach dem Smoothing nur global validiert oder auch zonenbezogen?
Schneidet die geglättete Verbindung schmale Randbereiche?
Wird bei fast tangentialen Segmenten numerisch instabil validiert?
Verbesserung
Für MOW-Transitionen Smoothing einschränken oder deaktivieren
zusätzlich zonenbewusste Validierung nach dem Smoothing
3.3 Start/Ziel-Projektion auf freie Zellen
Codebereich
GridMap::planPath(...)
Problem

Wenn Ziel out-of-bounds oder blockiert ist, wird einfach die nächstgelegene freie Zelle gesucht. Das ist algorithmisch praktisch, kann aber fachlich falsch sein.

Unbedingt prüfen
Landet das Ziel dabei auf der falschen Seite einer Engstelle?
Wird dadurch eine „legale, aber unsinnige“ Zielzelle gewählt?
Ist diese Ersatzzelle noch in derselben Zone?
Verbesserung
nächstgelegene freie Zelle nur innerhalb der aktiven Zone suchen
für MOW nicht beliebig auf Ersatz-Ziele ausweichen
4. Was an der Map-/Runtime-Seite untersucht werden sollte
4.1 Unterschied Preview vs Runtime
Problem

Deine Map hat mow: [], aber zones sind vorhanden. Zur Laufzeit wird allMowRoute_ aus mow geladen.

Risiko
Vorschau zeigt eine generierte Route aus zones
Laufzeit folgt einer anderen oder leeren Route
Preview und Robotik verhalten sich dadurch unterschiedlich
Unbedingt prüfen
Wo genau wird vor Missionsstart aus zones eine echte RoutePlan erzeugt?
Nutzt Runtime garantiert dieselbe Route wie die Vorschau?
Wird startPlannedMowing(...) konsequent verwendet?
Verbesserung

Ein einziger Grundsatz:

eine Route generieren
dieselbe Route speichern
dieselbe Route fahren

Nicht Preview und Runtime separat berechnen.

4.2 pathAllowed(...) und Segmentvalidierung
Warum wichtig

Das ist die letzte Schutzschicht gegen verbotene Übergänge. Der Planner verlässt sich darauf.

Unbedingt prüfen
Prüft pathAllowed(...) wirklich jedes Segment fein genug?
Prüft es nur Perimeter/Exclusions oder auch Zonenbindung?
Gibt es Toleranzprobleme an Polygonkanten?
Werden Segmente exakt auf Polygongrenzen korrekt behandelt?
Verbesserung

pathAllowed(...) sollte zwei Modi haben:

global erlaubt
erlaubt innerhalb aktiver Zone
5. Konkrete Reihenfolge für die Fehlersuche
Phase A — Sichtbarkeit schaffen
Debug-Ausgabe für:
Headland-Route
Stripe-Route
finale Route vor Transitionen
finale Route nach Transitionen
Für jeden Punkt markieren:
in Zone?
in erlaubter Fläche?
Abstand zu Zone/Perimeter/Exclusion
Phase B — Minimalfälle bauen
Testkarten anlegen:
Rechteck ohne Exclusion
Rechteck mit Exclusion
schräge Zone nahe Perimeter
deine echte Karte
Für jede Karte vergleichen:
erwartete Coverage
erzeugte Coverage
Übergangssegmente
Phase C — Planner verifizieren
Jede appendTransition(...) einzeln loggen
Prüfen, ob Planner-Übergänge aktive Zone verlassen
Prüfen, ob Smoothing problematische Shortcuts erzeugt
Phase D — Runtime angleichen
Sicherstellen, dass Preview und Laufzeit dieselbe geplante Route verwenden
mow: []-Fallbacks und Generierungspfad sauber machen
6. Was man unbedingt verbessern sollte
Sofort verbessern
A. Coverage und Transitionen fachlich trennen

Coverage-Segmente dürfen nicht durch allgemeinen Free-Path-Planer „semantisch uminterpretiert“ werden.

B. MOW-Transitionen zonenbewusst machen

Nicht nur globale erlaubte Fläche, sondern aktive Zone als Constraint.

C. Preview und Runtime vereinheitlichen

Keine doppelte Logik. Ein Plannergebnis, ein Fahrplan.

D. Segmentreihenfolge deterministisch machen

Für Stripe-Mähen keine globale Nearest-Neighbour-Heuristik mehr, sondern zeilenbasierte Reihenfolge.

7. Wahrscheinlich wichtigste Hotspots im Code

In dieser Reihenfolge würde ich reingehen:

appendTransition(...) in MowRoutePlanner.cpp
buildStripeSegments(...) in MowRoutePlanner.cpp
Headland/Infill-Inset-Logik in buildEdgeRoute(...) / buildStripeRoute(...)
GridMap::planPath(...) Zielersatzzelle + smoothPath(...)
Map::pathAllowed(...) und previewPath(...)
Runtime-Nutzung von startPlannedMowing(...) statt impliziter Neuinterpretation
8. Praktischer Endzustand, auf den du hinarbeiten solltest

Am Ende sollte das System so aussehen:

Zone → Coverage-Segmente deterministisch
Coverage-Segmente → nur lokale, zonenbewusste Übergänge
Vorschau = gespeicherte Route = gefahrene Route
jede Route validierbar gegen Perimeter, Exclusions und aktive Zone
Debug-Export mit allen Teilrouten und Transitionen

Das würde die Klasse von Fehlern deutlich reduzieren.

Wenn du willst, mache ich dir daraus im nächsten Schritt eine konkrete Debug-Checkliste mit Log-Ausgaben und Testfällen, die du direkt im Code abarbeiten kannst.

Wie sollte es fachlich richtig ablaufen um auch große komplexe Gärten und Zonen richtig zu mähen?
Nachgedacht für ein paar Sekunden

Fachlich richtig sollte das nicht als „ein bisschen Bahnen erzeugen und dazwischen irgendwie A* fahren“ ablaufen, sondern als mehrstufige Planungs-Pipeline mit klar getrennten Ebenen.

Gerade bei großen, verwinkelten Gärten ist diese Trennung entscheidend:

1. Weltmodell sauber trennen

Es braucht drei verschiedene Flächenmodelle, nicht nur eine einzige „erlaubte Fläche“:

A. Globale erlaubte Fläche
Perimeter minus harte Exclusions.
Das ist nur die Frage: Wo darf der Roboter grundsätzlich fahren?

B. Arbeitsfläche der aktuellen Mission
Zum Beispiel nur Zone 3 oder Zone 3 + 4.
Das ist die Frage: Wo soll er in dieser Mission mähen?

C. Fahrkorridor für Übergänge
Nicht jeder legale Bereich ist automatisch sinnvoll für Verbindungsfahrten.
Das ist die Frage: Wo darf er zwischen Teilstücken transitieren, ohne fachlich Unsinn zu machen?

Dein aktueller Code hat global schon viel davon vorbereitet, aber für MOW-Übergänge ist die aktive Zone als harte fachliche Nebenbedingung offenbar noch nicht stark genug verankert. Map kennt Perimeter, Exclusions und zones, während der Planner primär global auf erlaubter Fläche arbeitet.

2. Zuerst Flächen zerlegen, dann Bahnen erzeugen

Ein großer komplexer Garten sollte nicht direkt als eine einzige Stripe-Fläche behandelt werden.

Fachlich richtig ist:

Schritt 1: Arbeitsfläche vorbereiten
Perimeter nehmen
harte Exclusions abziehen
optional weiche No-Go-Zonen separat markieren
aktive Zone mit globaler erlaubter Fläche schneiden

Ergebnis: eine bereinigte Mähfläche

Schritt 2: Fläche in mähbare Teilflächen zerlegen

Große komplexe Gärten enthalten:

Engstellen
Ausbuchtungen
Inseln
lange schmale Arme
Teilbereiche, die nur über schmale Passagen verbunden sind

Diese Flächen sollte man in Subregionen zerlegen, zum Beispiel:

nach Konnektivität
nach Engstellen
nach Orientierung
nach sinnvoller Stripe-Richtung

Nicht eine globale Stripe-Heuristik für alles.

Schritt 3: Pro Teilfläche ein lokales Coverage-Muster erzeugen

Erst dann:

Headland / Randfahrt
Stripe / Boustrophedon
ggf. Spiral oder Sondermuster für kleine Teilflächen

Das jetzige MowRoutePlanner macht schon Headland + Stripe + Transitionen, aber fachlich robuster wäre erst eine explizite Zerlegung in Teilflächen und dann pro Teilfläche eine eigene Coverage-Planung.

3. Coverage und Transit strikt trennen

Das ist der wichtigste Punkt.

Ein Mähsystem hat zwei verschiedene Wegtypen:

Coverage-Segmente

Die eigentlichen Mähbahnen.
Regeln:

vollständig
systematisch
überlappungsarm
mit definierter Breite
keine unnötigen Leerfahrten
Transit-Segmente

Verbindungen zwischen Coverage-Teilen.
Regeln:

sicher
legal
kurz, aber nicht auf Kosten der Fachlogik
möglichst nicht quer durch noch ungemähte oder fachlich falsche Bereiche

Diese beiden Segmentarten dürfen nicht dieselbe Logik und dieselben Constraints haben.

Bei dir werden Transitionen aktuell per allgemeinem Planner ergänzt. Genau das ist für MOW fachlich zu grob.

Fachlich richtig wäre:

Coverage-Segmente aus Coverage-Planer
Transit-Segmente aus zonenbewusstem Verbindungsplaner
beide klar markiert
beide unterschiedlich validiert
4. Für große Gärten braucht es Hierarchie

Ein großer Garten darf nicht nur „Liste von Punkten“ sein.

Fachlich richtig ist eine Hierarchie wie diese:

Ebene 1: Mission

Welche Zonen heute? In welcher Reihenfolge? Mit welchen Prioritäten?

Ebene 2: Zone

Welche Geometrie, welches Muster, welche Schnittbreite, welche Randrunden?

Ebene 3: Teilfläche innerhalb der Zone

Welche lokalen Stripe-Richtungen? Welche Engstellen? Welche Subregionen?

Ebene 4: Segment

Konkrete Fahrsegmente:

mow
turn
transit
docking approach
recovery
Ebene 5: Tracking

Stanley/Pure Pursuit/MPC fährt nur das Segment ab.

Der LineTracker gehört fachlich ganz unten hin. Der darf keine Planungsprobleme kompensieren müssen. Er fährt nur sauber nach.

5. Zonenreihenfolge darf nicht geometrischer Zufall sein

Für komplexe Gärten braucht es eine bewusste Missionslogik.

Nicht:

„nächster Punkt“
„nächstes Stripe-Segment“
globale Nearest-Neighbour-Heuristik

Sondern:

erst Zone A
dann deren Teilflächen sinnvoll
dann definierter Wechsel in Zone B
danach Dock oder nächste Mission

Die momentane Segmentsortierung im Coverage-Teil ist als einfache Heuristik okay, aber für komplexe Flächen nicht fachlich stabil genug.

Besser wäre:

feste Reihenfolge pro Teilfläche
definierte Ein- und Austrittspunkte
Zonenübergänge als eigene Planungsaufgabe
6. Headland und Infill müssen geometrisch lückenlos zusammenpassen

Randfahrt und Innenbahnen dürfen nicht „ungefähr passen“, sondern müssen mathematisch sauber gekoppelt sein.

Fachlich richtig:

Headland-Breite explizit definieren
Infill beginnt exakt an der Innenkante des Headlands
keine impliziten halben Stripbreiten, wenn das Lücken oder Doppelmähen erzeugt
Eckenbehandlung robust gegen enge Winkel
Clipping an Exclusions ohne Verlust kleiner Restflächen

Gerade bei komplexen Gärten ist sonst typisch:

schmaler ungemähter Ring
abgeschnittene Randstücke
verloren gegangene Mini-Segmente
unnötige Kreuzfahrten

Das ist bei dir ein sehr plausibler Schwachpunkt.

7. Übergänge müssen zonenbewusst sein

Für MOW-Transitionen reicht es nicht, nur auf „legal“ zu prüfen.

Fachlich richtig sollte der Planner bei MOW unterscheiden:

innerhalb derselben Teilfläche

direkte Verbindung erlaubt, wenn sie die Coverage-Regeln nicht verletzt

zwischen Teilflächen derselben Zone

Übergang nur innerhalb dieser Zone oder definierter Transitfläche

zwischen verschiedenen Zonen

nur explizit erlaubte Zonenwechsel, nicht zufälliger Kurzweg

Rückkehr / Recovery

eigener Modus mit anderen Regeln

Sonst bekommt man genau das, was du beschrieben hast: geometrisch kurzer, aber fachlich falscher Weg.

8. Vorschau, Speicherung und Laufzeit müssen identisch sein

Das ist Pflicht.

Fachlich richtig:

Mission/Zonen wählen
vollständige Route generieren
dieselbe Route speichern
dieselbe Route fahren
dieselbe Route in der Vorschau zeigen

Nicht:

Vorschau aus Zonen berechnen
Laufzeit irgendwie anders
Export nochmal anders

Dein Code deutet schon an, dass diese Trennung noch nicht ganz sauber ist, weil zones und mow parallel existieren und startPlannedMowing(...) separat vorgesehen ist.

9. Recovery und Replanning nur lokal und kontrolliert

In großen Gärten gibt es immer Sonderfälle:

Hindernis neu entdeckt
GPS driftet
Zone teilweise blockiert
Passage zu eng

Dann darf Replanning nicht die Missionslogik zerstören.

Fachlich richtig:

nur lokales Replanning
nur für Transit, nicht komplette Coverage neu interpretieren
Rückkehr auf den geplanten Coverage-Pfad
kein globaler Kurzweg, nur weil er frei aussieht

Das jetzige Replanning über Planner/GridMap ist als Basis okay, aber für MOW sollte es viel restriktiver sein als für Dock/FREE.

10. Was eine robuste Zielarchitektur wäre

Für große komplexe Gärten würde ich fachlich dieses Modell anstreben:

A. Geometry Engine
Polygon-Operationen
Offset
Clipping
Engstellen erkennen
Fläche in Subregionen zerlegen
B. Coverage Planner
Headland
Stripe / Spiral
Musterwahl je Teilfläche
vollständige Coverage mit Qualitätsmetriken
C. Transit Planner
verbindet Coverage-Blöcke
zonenbewusst
regelbasiert
keine fachlich falschen Shortcuts
D. Mission Planner
Zonenreihenfolge
Prioritäten
Fortsetzen nach Pause
Teilmissionen
E. Tracking Controller
fährt nur Segmente ab
11. Qualitätskriterien, an denen man das messen sollte

Ein fachlich gutes System für komplexe Gärten sollte diese Kriterien erfüllen:

100 % der zulässigen Zielfläche abgedeckt
0 % Fahrt außerhalb aktiver erlaubter Fläche
0 fachlich falsche Zonenwechsel
minimale Leerfahrt
deterministische Vorschau
Vorschau = Runtime
Recovery ohne Planzerstörung
gleiche Qualität bei einfachen und komplexen Gärten
12. Praktisch übersetzt für dein Projekt

Für dein Projekt würde ich die Soll-Logik so formulieren:

zones + Perimeter + Exclusions → bereinigte Mähflächen
jede Zone in mähbare Teilflächen zerlegen
pro Teilfläche:
Headland
Infill
Teilflächen mit zonenbewussten Transitwegen verbinden
komplette Route als RoutePlan festschreiben
genau diese Route für Vorschau, Export und Laufzeit verwenden
Replanning nur lokal und nur innerhalb zulässiger Missionseinschränkungen

Das wäre fachlich sauber und skalierbar.

Wenn du willst, formuliere ich dir daraus als Nächstes eine Soll-Architektur für Sunray mit konkreten Modulen, Datenstrukturen und Planer-Regeln.

Ja mach das
Nachgedacht für ein paar Sekunden

Hier ist eine fachlich saubere Soll-Architektur für Sunray, damit auch große, komplexe Gärten mit Zonen robust geplant und gemäht werden.

Ich beschreibe sie so, dass klar wird:

welche Module es geben sollte
welche Daten jedes Modul braucht
welche Regeln dort gelten
wo deine aktuelle Architektur schon passt
wo du sie umbauen solltest
1. Grundprinzip

Sunray sollte intern nicht mit einer einzigen „Liste von Mähpunkten“ arbeiten, sondern mit einer Planungskette aus mehreren Ebenen:

Map Model
Geometrie und Regeln der Welt
Mission Model
Was heute gemäht werden soll
Coverage Decomposition
Zerlegung in mähbare Teilflächen
Coverage Planner
Headland + Infill je Teilfläche
Transit Planner
Verbindungswege zwischen Coverage-Blöcken
Route Compiler
Aus allen Teilplänen eine deterministische Fahrroute erzeugen
Execution Engine
Genau diese Route abfahren
Recovery/Replan Layer
Nur lokal eingreifen, ohne die Missionslogik zu zerstören

Dein aktueller Code hat Teile davon schon:

Map als Weltmodell
MowRoutePlanner als Coverage-/Transition-Mischung
Planner / GridMap / Costmap für Pfadplanung
LineTracker für Ausführung

Das Problem ist vor allem: Coverage und Transit sind noch nicht klar genug getrennt.

2. Zielbild der Module
A. MapModel

Verantwortung: statische Weltgeometrie

Enthält:

Perimeter
harte Exclusions
weiche Exclusions
Dock
Zonen
optionale feste Transit-Korridore
Planner-Parameter

Das ist heute im Wesentlichen Map.

Verbesserung

Map sollte intern drei Flächen sauber unterscheiden:

globalAllowedArea
zoneWorkingArea(zoneId)
transitAllowedArea(mission, from, to)

Heute gibt es global vor allem isInsideAllowedArea(...), also global legal / nicht legal. Für MOW reicht das fachlich nicht.

B. MissionModel

Verantwortung: was in dieser Mission tatsächlich zu tun ist

Enthält:

ausgewählte Zonen
Reihenfolge der Zonen
Prioritäten
Muster pro Zone
Überspringregeln
Startstrategie
Dock-Strategie

Beispielstruktur:

struct MissionPlan {
    std::vector<std::string> zoneSequence;
    bool useEdgeMowing = true;
    bool allowInterZoneTransit = true;
    float preferredTransitCost = ...;
};
Warum nötig

Die Map beschreibt die Welt.
Die Mission beschreibt den Arbeitsauftrag.

Aktuell wird das im Code teilweise vermischt, etwa wenn Vorschau direkt aus zones generiert wird.

C. CoverageAreaBuilder

Verantwortung: aus Zone + globaler Geometrie eine echte mähbare Arbeitsfläche bauen

Eingaben:

Zonenpolygon
Perimeter
Exclusions
Clearance
ggf. Mäherbreite / Sicherheitsabstand

Ausgabe:

eine oder mehrere bereinigte Teilflächen
Aufgabe

Für jede Zone:

Zone normalisieren
mit Perimeter schneiden
Exclusions abziehen
Clearance anwenden
Engstellen prüfen
bei Bedarf in Subregionen zerlegen
Warum nötig

Dein aktueller MowRoutePlanner schneidet Intervalle zur Laufzeit gegen Perimeter und Exclusions, aber fachlich robuster wäre eine explizit vorbereitete Coverage-Fläche pro Zone.

D. CoverageDecomposer

Verantwortung: komplexe Flächen in mähbare Blöcke zerlegen

Das ist der wichtigste Architektur-Baustein für große Gärten.

Eingabe
bereinigte Zonenfläche
Ausgabe
Liste von CoverageCell oder CoverageRegion

Beispiel:

struct CoverageRegion {
    std::string id;
    PolygonPoints polygon;
    float preferredAngleDeg;
    bool requiresSpecialEntry;
};
Zerlegungskriterien
Konnektivität
Engstellen
Orientierung
sehr schmale Arme
stark unterschiedliche lokale Geometrie
Warum nötig

Ein komplexer Garten ist fast nie fachlich sinnvoll als eine einzige Stripe-Fläche behandelbar.

Dein aktueller Planer erzeugt Stripes direkt aus der ganzen Zone. Das ist für kleine Flächen okay, aber für große verwinkelte Gärten zu grob.

E. CoveragePlanner

Verantwortung: je Teilfläche die eigentlichen Mähbahnen erzeugen

Nicht Transit. Nur Mähmuster.

Eingabe
CoverageRegion
Zoneneinstellungen
Stripbreite
Randrunden
Pattern-Typ
Ausgabe
CoverageBlock

Beispiel:

enum class SegmentKind { MOW, TURN, TRANSIT, DOCK, RECOVERY };

struct PlannedSegment {
    SegmentKind kind;
    Point start;
    Point end;
    bool reverseAllowed;
    float targetSpeed;
};

struct CoverageBlock {
    std::string regionId;
    std::vector<PlannedSegment> segments;
};
Intern getrennt erzeugen
Headland / Randbahnen
Infill / Stripe
Turn-Segmente innerhalb des Blocks
Wichtige Regel

Ein CoverageBlock muss fachlich für sich vollständig sein:

keine Lücken
definierter Start- und Endpunkt
bekannte Orientierung
keine zufälligen globalen Sprünge
Was du ändern solltest

Im jetzigen MowRoutePlanner sollten Headland und Stripe nicht nur Punktlisten sein, sondern semantisch markierte Segmente. Dann kann man später unterscheiden:

das ist Mähfahrt
das ist lokale Wendefahrt
das ist externer Transit

Derzeit läuft zu viel noch als einfache Wegpunktfolge.

F. TransitPlanner

Verantwortung: Coverage-Blöcke sinnvoll verbinden

Das ist ein eigenes Modul. Nicht Teil des Coverage-Planers.

Eingabe
Endpunkt von Block A
Startpunkt von Block B
aktive Zone
Mission
erlaubte Transitfläche
Ausgabe
Transit-Segmente
Zentrale Regel

Der TransitPlanner muss zonenbewusst sein.

Er braucht mindestens drei Modi:

WithinRegion
direkte Verbindung innerhalb derselben Teilfläche
WithinZone
Verbindung zwischen zwei Teilflächen derselben Zone
InterZone
Verbindung zwischen verschiedenen Zonen

Heute läuft so etwas zu allgemein über appendTransition(...) → previewPath(...). Das ist für MOW fachlich zu schwach.

Konkrete Regelvorschläge

Für WayType::MOW:

Transit innerhalb derselben Zone bevorzugen
Verlassen der Zone nur, wenn explizit erlaubt
Schneiden noch ungemähter sensibler Bereiche möglichst vermeiden
weiche Transitkosten nahe Perimeter/Exclusion erhöhen
Ersatz-Zielzellen nur innerhalb derselben Zone suchen

Das wäre deutlich robuster als allgemeines FREE-Routing.

G. RouteCompiler

Verantwortung: aus Coverage + Transit eine deterministische Gesamtstrecke bauen

Eingabe
MissionPlan
CoverageBlocks
TransitPlans
Ausgabe
ein vollständiger RoutePlan
Aufgabe
Blöcke in definierter Reihenfolge zusammenfügen
Transit dazwischen einsetzen
Segmentarten markieren
Geschwindigkeiten setzen
Reverse-Regeln setzen
Start-/Fortsetzpunkte definieren
Zentrale Regel

Preview, Export und Laufzeit müssen denselben kompilierten Plan verwenden.

Dein Code deutet schon an, dass startPlannedMowing(...) genau dafür gedacht ist. Das sollte zur Hauptstrecke werden, nicht die Ausnahme.

H. ExecutionEngine

Verantwortung: die kompilierten Segmente exakt abfahren

Eingabe
CompiledRoute
Aufgabe
aktives Segment verwalten
Segmentabschluss erkennen
nächstes Segment aktivieren
Tracking-Modus je Segment wählen
Recovery anstoßen
Segmentarten unterschiedlich behandeln

Der Controller muss wissen, was er fährt:

MOW: präzise Linienfahrt, Mähwerk aktiv
TURN: geringere Geschwindigkeit, andere Toleranzen
TRANSIT: keine Coverage-Interpretation
DOCK: andere Regeln
RECOVERY: Sicherheitsmodus

Der LineTracker bleibt unten drin ein Pfadfolger. Er sollte nicht raten müssen, warum ein Segment existiert.

I. RecoveryPlanner

Verantwortung: lokale Störungen beheben, ohne den Missionsplan zu zerstören

Fälle
Hindernis
Blockade
schlechter GPS-Fix
Segment nicht erreichbar
Robot außerhalb Sollpfad
Regeln
möglichst nur lokales Replanning
Rückkehr auf das aktuelle Segment oder den aktuellen Block
keine komplette Neuinterpretation der Mission
kein spontaner Zonenwechsel ohne Freigabe

Das jetzige Replanning über Planner / GridMap ist als Basis brauchbar, aber für MOW zu wenig semantisch.

3. Datenmodell, das Sunray intern haben sollte

Ich würde intern nicht nur RoutePoint verwenden, sondern ein semantisch stärkeres Modell.

Vorschlag
enum class RouteSemantic {
    COVERAGE_EDGE,
    COVERAGE_INFILL,
    TURN_LOCAL,
    TRANSIT_WITHIN_ZONE,
    TRANSIT_INTER_ZONE,
    DOCK_APPROACH,
    RECOVERY
};

struct PlannedWaypoint {
    Point p;
    RouteSemantic semantic;
    std::string zoneId;
    std::string regionId;
    bool reverseAllowed;
    bool reverse;
    bool slow;
    float clearance_m;
    float targetSpeed;
};
Warum

Dann kannst du:

im Debug sehen, welcher Segmenttyp falsch ist
zonenbewusste Regeln erzwingen
Preview farblich korrekt rendern
Laufzeitverhalten pro Segmenttyp anpassen

Das aktuelle RoutePoint ist für erste Schritte okay, aber semantisch zu arm für komplexe Missionen.

4. Empfohlene Planungs-Pipeline für Sunray
Schritt 1 — Map laden

MapModel

Schritt 2 — Mission auswählen

MissionModel

Schritt 3 — pro Zone Arbeitsfläche aufbereiten

CoverageAreaBuilder

Schritt 4 — Fläche in Teilflächen zerlegen

CoverageDecomposer

Schritt 5 — pro Teilfläche Coverage erzeugen

CoveragePlanner

Schritt 6 — Coverage-Blöcke sortieren

nicht global per einfacher Nearest-Neighbour-Heuristik, sondern:

zonenweise
regionenweise
mit definierter Strategie
Schritt 7 — Übergänge planen

TransitPlanner

Schritt 8 — alles zu einer Gesamtstrecke kompilieren

RouteCompiler

Schritt 9 — diese Route speichern und fahren

ExecutionEngine

Schritt 10 — lokale Störungen behandeln

RecoveryPlanner

5. Welche Regeln du fachlich festschreiben solltest
Regel 1

Coverage-Pläne dürfen nicht implizit durch Transit-Replanner verändert werden.

Regel 2

MOW-Transitionen sind nie allgemeine FREE-Pfade.

Regel 3

Jeder Übergang kennt Zone und Region.

Regel 4

Preview = Export = Runtime.

Regel 5

Jede Coverage-Region hat definierte Ein-/Austrittspunkte.

Regel 6

Die Segmentreihenfolge ist deterministisch.

Regel 7

Replanning ist lokal und semantisch begrenzt.

6. Konkrete Umbauempfehlung für deinen aktuellen Code
Stufe 1 — ohne kompletten Rewrite

Ziel: schnell stabiler werden

1. MowRoutePlanner in zwei logische Teile trennen
CoveragePlanner
TransitionPlanner

Heute ist das vermischt.

2. appendTransition(...) zonenbewusst machen

Nicht nur previewPath(...), sondern:

aktive Zone übergeben
nur innerhalb zulässiger Transitfläche planen
MOW-spezifische Constraints erzwingen
3. RoutePoint semantisch erweitern

Mindestens:

zoneId
segmentKind
regionId
4. startPlannedMowing(...) zum Standard machen

Vorschau erzeugt Route → exakt diese Route fahren.

Stufe 2 — mittlerer Umbau

Ziel: große Gärten robust unterstützen

5. expliziten CoverageDecomposer einführen

Teilflächen statt Gesamtzone

6. sortSegmentsNearestNeighbour(...) ersetzen

Zeilen-/regionenbasierte deterministische Reihenfolge statt globaler NN-Heuristik.

7. TransitPlanner mit Modi bauen
same region
same zone
inter-zone
Stufe 3 — saubere Langzeitarchitektur

Ziel: professionell skalierbar

8. CompiledMissionRoute als eigenes Objekt

Nicht nur Map mit Punktlisten, sondern eigener Planungszustand

9. Recovery und Runtime explizit trennen

Runtime fährt
Recovery repariert lokal
Missionplanung bleibt stabil

10. Debug-/Export-Format ausbauen

Exportieren:

Coverage-Blöcke
Transitionen
Semantik
Zone/Region pro Segment
7. Minimal sinnvolle Modulstruktur in Dateien

Ich würde ungefähr so schneiden:

core/navigation/
  MapModel.h/.cpp
  MissionModel.h/.cpp
  CoverageAreaBuilder.h/.cpp
  CoverageDecomposer.h/.cpp
  CoveragePlanner.h/.cpp
  TransitPlanner.h/.cpp
  RouteCompiler.h/.cpp
  RecoveryPlanner.h/.cpp
  ExecutionRoute.h/.cpp
  LineTracker.h/.cpp

Wenn du weniger umbauen willst:

Map.h/.cpp
MowCoveragePlanner.h/.cpp
MowTransitPlanner.h/.cpp
CompiledRoute.h/.cpp
Planner.h/.cpp
GridMap.h/.cpp
Costmap.h/.cpp
LineTracker.h/.cpp
8. Empfohlener Migrationsweg für Sunray
Phase 1

Nur Architektur-Schnitt sauberziehen:

Coverage vs Transit trennen
Route-Semantik ergänzen
Preview = Runtime erzwingen
Phase 2

Teilflächenzerlegung einführen:

komplexe Zonen robust machen
planbare Ein-/Austrittspunkte
Phase 3

TransitPlanner fachlich aufwerten:

zonenbewusst
missionsbewusst
keine fachlich falschen Kurzwege
Phase 4

Recovery robust machen:

Hindernisse
Engstellen
Wiedereinstieg in Coverage
9. Endzustand, der fachlich richtig wäre

Wenn Sunray sauber aufgestellt ist, sollte der Ablauf so sein:

User wählt Zonen
System baut pro Zone die bereinigte Arbeitsfläche
Komplexe Flächen werden in Teilflächen zerlegt
Jede Teilfläche bekommt Headland + Infill
Teilflächen werden in definierter Reihenfolge verbunden
Es entsteht eine vollständige, semantisch markierte Route
Diese Route wird angezeigt, gespeichert und gefahren
Laufzeit-Replanning darf nur lokal helfen, nicht die Missionslogik umschreiben

Das wäre fachlich robust und für große Gärten tragfähig.

Wenn du willst, mache ich dir daraus als Nächstes eine konkrete Refactoring-Roadmap für die bestehenden Sunray-Dateien, also: welche vorhandene Datei bleibt, welche wird gesplittet und welche neue Klasse kommt dazu.

Ich kann mir die Mähvorschau danach aber immer noch in der WebUi im Missionplanner anschauen und sie im Dashboard als Fortschrittsanzeige?
Nachgedacht für ein paar Sekunden

Ja. Das sollte sogar besser werden, nicht schlechter.

Wenn du die Architektur sauber trennst, ist die Vorschau in der WebUI und die Fortschrittsanzeige im Dashboard ein natürlicher Teil davon.

Wie es fachlich sauber laufen sollte

Es gibt dann nicht mehr:

eine Vorschau, die irgendwie aus Zonen live zusammengebastelt wird
und separat eine Laufzeitroute, die anders aussieht

Sondern:

Mission/Zonen auswählen
daraus eine vollständige geplante Route erzeugen
diese Route speichern
genau diese Route in der WebUI anzeigen
genau diese Route vom Roboter abfahren
Fortschritt gegen genau diese Route berechnen

Dann gilt:

Vorschau = gespeicherter Plan = gefahrene Route = Fortschrittsbasis

Das ist der eigentliche Sollzustand.

Für die WebUI bedeutet das

Der Missionplanner zeigt nicht mehr nur abstrakt:

Zone
Stripe-Idee
Randfahrt ungefähr

sondern eine echte kompilierte Missionsroute mit Segmenttypen wie:

Randmähen
Innenbahnen
Übergänge innerhalb Zone
Übergänge zwischen Zonen
Dock-/Rückweg

So kann die WebUI exakt das rendern, was später wirklich gefahren wird.

Für das Dashboard bedeutet das

Die Fortschrittsanzeige sollte nicht nur aus „aktueller Mähpunktindex / Gesamtpunkte“ bestehen, sondern aus einem kleinen Missionsstatusmodell.

Zum Beispiel:

aktive Mission
aktuelle Zone
aktueller Teilbereich
aktuelles Segment
bereits gefahrene Distanz
gemähte Teilfläche
verbleibende geschätzte Zeit

Minimal reicht schon:

totalSegments
completedSegments
activeSegment
activeZoneId
progressPercent

Besser wäre zusätzlich:

completedCoverageArea_m2
remainingCoverageArea_m2
transitDistance_m
coverageDistance_m

Dann bekommst du im Dashboard nicht nur „er ist bei 43 %“, sondern sinnvollere Infos wie:

Zone 1
Innenbahnen
43 % abgeschlossen
noch 18 min
aktuell Transit zur nächsten Teilfläche
Was technisch dafür nötig ist

Die geplante Route muss semantisch markiert sein. Also nicht nur eine rohe Liste von Punkten.

Zum Beispiel pro Wegpunkt oder Segment:

segmentKind
zoneId
regionId
isMowing
isTransit
plannedLength
completedLength

Dann kann die WebUI:

unterschiedliche Farben zeichnen
Fortschritt segmentweise einblenden
aktive Zone hervorheben
bereits gefahrene Bahn markieren
Praktisch für deine WebUI

Die Vorschau könnte so aufgebaut sein:

Missionplanner

zeigt die gesamte geplante Route

Randfahrt grün
Innenbahnen blau
Transit orange
Dock violett
aktive Teilfläche hervorgehoben
Dashboard

zeigt die Ist-Situation

bisher gefahrene Route dick markiert
aktuelles Zielsegment hervorgehoben
restliche Route blasser
Statusdaten daneben
Der große Vorteil

Gerade bei deinen jetzigen Problemen ist das wichtig:

Wenn Vorschau und Laufzeit aus derselben kompilierten Route kommen, siehst du sofort:

ob die Zone unvollständig geplant wurde
ob ein Übergang fachlich falsch ist
ob Runtime davon abweicht

Heute ist genau das schwerer, wenn Preview, Export und Runtime nicht identisch sind.

Was du vermeiden solltest

Nicht wieder in diese Richtung gehen:

WebUI berechnet eigene Vorschau
Backend berechnet andere Route
Runtime plant nochmal neu
Dashboard zeigt nur Punktindex

Dann bekommst du wieder drei Wahrheiten.

Es sollte genau eine Wahrheit geben: die kompilierte Missionsroute.

Fazit

Ja, du kannst dir die Mähvorschau weiter in der WebUI anschauen und im Dashboard als Fortschritt nutzen.
Eigentlich sollte die Architektur genau darauf hinauslaufen, dass das zuverlässiger und genauer wird als jetzt.

Der richtige Weg ist:

Mission kompilieren
Route speichern
Route rendern
Route fahren
Fortschritt gegen Route messen

Wenn du willst, skizziere ich dir als Nächstes das Datenformat für diese kompilierten Routensegmente, damit WebUI, Backend und Roboter alle dieselbe Struktur verwenden.

Ja und die Infos wie Geojson export aufgebaut sein muss
Nachgedacht für 6s

Ja. Dafür solltest du zwei Formate trennen:

internes Missions-/Routenformat für Backend, Runtime und Dashboard
GeoJSON-Exportformat für WebUI, Debugging und externe Tools

GeoJSON ist sehr gut für Anzeige und Debug, aber als einziges internes Arbeitsformat zu schwach. Dein aktuelles internes Map-JSON enthält schon mehr Semantik als GeoJSON, etwa zones, planner, dockMeta, exclusionMeta.

1. Was intern die eine Wahrheit sein sollte

Es sollte neben Map ein kompiliertes Missionsobjekt geben, etwa:

{
  "version": 1,
  "mapId": "garden-01",
  "missionId": "mission-2026-04-06-001",
  "createdAt": "2026-04-06T12:00:00Z",
  "planner": {
    "stripWidth": 0.18,
    "clearance": 0.25
  },
  "zones": [
    {
      "zoneId": "zone-1775490502231-lmpbub",
      "order": 1,
      "name": "Zone 1"
    }
  ],
  "route": {
    "segments": [
      {
        "id": "seg-0001",
        "kind": "coverage_edge",
        "zoneId": "zone-1775490502231-lmpbub",
        "regionId": "region-01",
        "reverseAllowed": false,
        "reverse": false,
        "slow": false,
        "clearance_m": 0.25,
        "targetSpeed_mps": 0.8,
        "points": [[x1,y1],[x2,y2],[x3,y3]]
      },
      {
        "id": "seg-0002",
        "kind": "coverage_infill",
        "zoneId": "zone-1775490502231-lmpbub",
        "regionId": "region-01",
        "reverseAllowed": false,
        "reverse": false,
        "slow": false,
        "clearance_m": 0.25,
        "targetSpeed_mps": 1.0,
        "points": [[x1,y1],[x2,y2]]
      },
      {
        "id": "seg-0003",
        "kind": "transit_within_zone",
        "zoneId": "zone-1775490502231-lmpbub",
        "regionId": "region-01",
        "reverseAllowed": false,
        "reverse": false,
        "slow": false,
        "clearance_m": 0.25,
        "targetSpeed_mps": 0.6,
        "points": [[x1,y1],[x2,y2],[x3,y3]]
      }
    ]
  }
}
2. Welche Segmentarten du brauchst

Mindestens diese:

coverage_edge
coverage_infill
turn_local
transit_within_zone
transit_inter_zone
dock_approach
recovery

Das ist wichtig, weil dein aktueller Code fachlich unterschiedliche Dinge noch zu ähnlich behandelt: Coverage aus MowRoutePlanner, Übergänge über appendTransition(...), allgemeines Routing über Planner/GridMap, Tracking über LineTracker.

3. Welche Felder pro Segment Pflicht sein sollten

Ich würde diese Felder fest vorgeben:

{
  "id": "seg-0001",
  "kind": "coverage_infill",
  "zoneId": "zone-1",
  "regionId": "region-a",
  "sequence": 12,
  "reverseAllowed": false,
  "reverse": false,
  "slow": false,
  "isMowing": true,
  "clearance_m": 0.25,
  "targetSpeed_mps": 1.0,
  "plannedLength_m": 4.82,
  "points": [[9.4,6.8],[10.1,6.8]]
}

Zusätzlich für Laufzeit/Fortschritt:

{
  "runtime": {
    "status": "active",
    "completedSegments": 11,
    "activeSegmentId": "seg-0001",
    "completedLength_m": 37.2,
    "completedArea_m2": 51.4,
    "progressPercent": 43.7
  }
}
4. Wie die WebUI damit arbeiten sollte

Die WebUI sollte nicht live aus zones neu raten, sondern diese kompilierten Segmente rendern.

Dann kannst du:

coverage_edge grün zeichnen
coverage_infill blau
transit_within_zone orange
dock_approach violett
aktive Segmente dicker hervorheben
abgeschlossene Segmente anders färben

Das passt auch besser als nur mow: [] oder rohe Punktlisten, weil dein aktuelles Map-Format zwar zones und mow kennt, aber nicht genug Segmentsemantik für eine gute UI transportiert.

5. Wie der GeoJSON-Export aufgebaut sein sollte

GeoJSON sollte ein Darstellungs- und Debugformat sein.
Am besten als FeatureCollection mit sauber getrennten Layern.

Empfohlene Top-Level-Struktur
{
  "type": "FeatureCollection",
  "name": "sunray_mission_export",
  "properties": {
    "version": 1,
    "mapId": "garden-01",
    "missionId": "mission-2026-04-06-001",
    "createdAt": "2026-04-06T12:00:00Z",
    "coordinateSystem": "local_meters"
  },
  "features": []
}
6. Welche Feature-Typen rein sollten
A. Perimeter
{
  "type": "Feature",
  "properties": {
    "featureKind": "perimeter"
  },
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[x,y],[x,y],...]]
  }
}
B. Exclusions
{
  "type": "Feature",
  "properties": {
    "featureKind": "exclusion",
    "exclusionType": "hard",
    "clearance_m": 0.25
  },
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[x,y],[x,y],...]]
  }
}
C. Zones
{
  "type": "Feature",
  "properties": {
    "featureKind": "zone",
    "zoneId": "zone-1775490502231-lmpbub",
    "order": 1,
    "name": "Zone 1",
    "pattern": "stripe",
    "edgeMowing": true,
    "edgeRounds": 1,
    "stripWidth": 0.18,
    "clearance_m": 0.25
  },
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[x,y],[x,y],...]]
  }
}
D. Dock route / Dock points
{
  "type": "Feature",
  "properties": {
    "featureKind": "dock_route"
  },
  "geometry": {
    "type": "LineString",
    "coordinates": [[x,y],[x,y],[x,y]]
  }
}
E. Coverage regions

Optional, aber sehr nützlich für Debugging.

{
  "type": "Feature",
  "properties": {
    "featureKind": "coverage_region",
    "zoneId": "zone-1",
    "regionId": "region-01"
  },
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[x,y],[x,y],...]]
  }
}
F. Compiled route segments

Das ist der wichtigste Teil.

Jedes Segment als eigenes LineString-Feature:

{
  "type": "Feature",
  "properties": {
    "featureKind": "route_segment",
    "segmentId": "seg-0003",
    "sequence": 3,
    "kind": "transit_within_zone",
    "zoneId": "zone-1775490502231-lmpbub",
    "regionId": "region-01",
    "reverseAllowed": false,
    "reverse": false,
    "slow": false,
    "isMowing": false,
    "clearance_m": 0.25,
    "targetSpeed_mps": 0.6,
    "plannedLength_m": 2.43
  },
  "geometry": {
    "type": "LineString",
    "coordinates": [[x,y],[x,y],[x,y]]
  }
}
G. Start-/Endpunkte

Optional als Point-Features:

{
  "type": "Feature",
  "properties": {
    "featureKind": "route_marker",
    "markerKind": "segment_start",
    "segmentId": "seg-0003",
    "sequence": 3
  },
  "geometry": {
    "type": "Point",
    "coordinates": [x,y]
  }
}
7. Was unbedingt in GeoJSON rein muss

Für dein Projekt würde ich sagen: Pflichtfelder für Route-Segmente sind:

featureKind
segmentId
sequence
kind
zoneId
regionId
reverseAllowed
reverse
slow
isMowing
clearance_m
plannedLength_m

Ohne diese Metadaten wird GeoJSON wieder nur eine „Linie auf der Karte“, aber keine saubere Missionsdarstellung.

8. Was nicht in GeoJSON die einzige Wahrheit sein sollte

GeoJSON ist gut für:

WebUI
Debugging
Vergleich Preview vs Runtime
Export

GeoJSON ist schlecht als einziges internes Planungsformat für:

Fortschritt mit feiner Semantik
Zustand des aktiven Segments
Recovery-Zustände
Planner-interne Zusatzinfos
robuste Versionierung

Deshalb:

intern: kompiliertes Missions-JSON
extern/UI: GeoJSON-Projection desselben Plans
9. Wie Fortschritt im GeoJSON abgebildet werden kann

Es gibt zwei gute Wege.

Weg A: statisches GeoJSON + separates Runtime-Statusobjekt

Sauberer Ansatz.

{
  "missionId": "mission-2026-04-06-001",
  "activeSegmentId": "seg-0003",
  "completedSegments": ["seg-0001","seg-0002"],
  "progressPercent": 43.7,
  "robotPose": [12.3, 7.1, 1.57]
}

Die UI färbt dann Segmente je Status.

Weg B: GeoJSON-Features mit Status anreichern

Geht auch, aber mischt Plan und Laufzeit stärker.

"properties": {
  "featureKind": "route_segment",
  "segmentId": "seg-0003",
  "status": "active"
}

Ich würde A bevorzugen.

10. Was du gegenüber deinem jetzigen Export verbessern solltest

Dein bisheriger Export war als Map-Export okay, aber für Missionsdebugging unvollständig, besonders wenn nur Perimeter/Exclusions/Dock sichtbar sind. Dein internes Format kann schon deutlich mehr, etwa zones und Planer-Metadaten.

Ich würde künftig zwei Exporte anbieten:

map_export.json

Für Bearbeitung/Speicherung:

perimeter
exclusions
zones
dock
planner
dockMeta
exclusionMeta
mission_export.geojson

Für Anzeige/Debug:

perimeter feature
exclusion features
zone features
coverage region features
compiled route segment features
dock route feature

Dann ist sofort sichtbar:

welche Zone geplant wurde
welche Bahnen Coverage sind
welche Stücke Transit sind
ob der Planner fachlich Unsinn gebaut hat
11. Konkreter Minimalstandard für Sunray

Wenn du es pragmatisch halten willst, würde ich ab jetzt mindestens das festlegen:

Intern speichern
Map-JSON wie jetzt, aber mit sauber erzeugter compiledMissionRoute
In GeoJSON exportieren
perimeter
exclusions
zones
dock_route
route_segment Features für alle geplanten Segmente
Segment-Eigenschaften
segmentId
sequence
kind
zoneId
reverse
slow
isMowing

Das reicht schon, damit WebUI und Dashboard sauber arbeiten können.