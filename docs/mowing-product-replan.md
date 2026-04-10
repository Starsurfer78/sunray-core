# Produkt-Neuplanung: Mähablauf ohne Altlasten

## Ziel

Dieses Dokument definiert den neuen Soll-Zustand für Kartenmodus, Missionsmodus, Dashboard und Runtime.

Der Fokus ist bewusst produktorientiert:

- einfaches, normales Verhalten für einen Mähroboter
- klare Trennung zwischen Karte, Mission und Live-Betrieb
- eine sichtbare Wahrheit für Vorschau, Start und Dashboard
- lokale Hindernisumfahrung ohne Neubau der ganzen Mission

Wichtig:

- Die Werkzeuge der aktuellen Karten-Seite bleiben erhalten.
- Die Werkzeuge der aktuellen Missions-Seite bleiben erhalten.
- Neu geplant werden Datenverantwortung, Planerlogik und Live-Darstellung.

## Was ausdrücklich bleiben soll

### Karten-Seite

Die aktuellen Werkzeuge zum Anlegen und Bearbeiten bleiben als Bedienkonzept bestehen:

- Perimeter anlegen und bearbeiten
- No-Go-Zonen anlegen und bearbeiten
- Dock setzen und bearbeiten
- Karte speichern und laden

Diese Werkzeuge sind fachlich sinnvoll und benutzerverständlich.

### Missions-Seite

Die aktuellen Werkzeuge zum Anlegen und Bearbeiten von Zonen bleiben als Bedienkonzept bestehen:

- Zone anlegen und bearbeiten
- Zonenparameter setzen
- aktive Zone auswählen
- Vorschau für die aktive Zone anzeigen
- Mission speichern

Auch dieses Werkzeugmodell ist fachlich richtig und muss nicht neu erfunden werden.

## Sichtbarer Produktablauf

### 1. Karte

Benutzerablauf:

- Perimeter anlegen
- Hard-No-Go-Zonen einzeichnen
- Dockpfad und Dock setzen (mind. 4 Punkte)
- Karte speichern

Ergebnis:

- eine statische Karte

Die Karte beantwortet nur eine Frage:

- Wo darf grundsätzlich gefahren und gemäht werden?

### 2. Mission

Benutzerablauf:

- Karte laden
- Zone einzeichnen
- Zoneneinstellungen setzen
- Vorschau nur für die aktive Zone anzeigen
- speichern
- nächste Zone anlegen oder fertig sein

Ergebnis:

- eine Mission mit einer oder mehreren Zonen
- für jede Zone ein konkreter Bahnplan

Die Mission beantwortet nur eine Frage:

- Wie soll diese Fläche konkret gemäht werden?

### 3. Start und Dashboard

Benutzerablauf:

- Mission starten

Dashboard zeigt:

- alle Zonen der Mission
- alle geplanten Bahnen der Mission
- bereits gefahrene Bahnen gestrichelt
- noch offene Bahnen durchgezogen
- Roboterposition
- aktuelles Ziel auf der Bahn

Das Dashboard beantwortet nur eine Frage:

- Was passiert gerade live?

### 4. Hindernisfall

Benutzerablauf:

- Roboter erkennt Hindernis
- Hindernis erscheint sofort auf der Karte
- Roboter fährt lokal darum herum
- eigentliche Mission bleibt sichtbar gleich

Dashboard zeigt weiter:

- dieselbe Mission
- denselben Plan
- zusätzlich den lokalen Umweg

### 5. Sofort mähen

Benutzerablauf:

- Zone anklicken
- Button Jetzt mähen

Ergebnis:

- nur diese Zone wird geplant oder aktiviert
- nur diese Zone wird gefahren

## Harte Produktregeln

### Regel 1: Karte und Mission sind getrennt

Karte enthält nur:

- Perimeter
- Hard-No-Go
- Dock
- statische Grundgeometrie

Mission enthält nur:

- Zonen
- Zonenparameter
- geplante Bahnen pro Zone
- Reihenfolge der Zonen innerhalb der Mission

Nicht erlaubt:

- Zonen in der Karte als primäre statische Wahrheit behandeln
- Missionsplanung aus impliziten Kartendaten ableiten, die nicht zur Mission gehören

### Regel 2: Vorschau ist der echte Fahrplan

Die Vorschau ist kein Entwurf und kein Schätzwert.

Die Vorschau ist die sichtbare Form des echten Plans, der später gefahren wird.

Das bedeutet:

- Vorschau und Start benutzen denselben Plan
- Dashboard zeigt denselben Plan
- es gibt keinen zweiten versteckten Laufzeitplan

### Regel 3: Hindernisse ändern nicht die Mission

Wenn ein Hindernis auftaucht:

- nur lokaler Umweg
- keine neue Missionsinterpretation
- kein Neubau aller Zonen
- kein globales Reordering der Mission

Die Mission bleibt fachlich unverändert.

### Regel 4: Nur eine sichtbare Wahrheit im Dashboard

Das Dashboard kennt nur diese Layer:

- geplante Bahnen
- bereits gefahrene Bahnen
- aktuelle Roboterposition
- aktuelles Ziel
- spontane Hindernisse
- lokaler Umweg, falls aktiv

Nicht sichtbar gemacht werden:

- Komponenten
- PlannerContext
- Costmap
- A*
- Validator
- technische Transition-Semantik
- interne Recovery-Begriffe

## Minimales internes Zielmodell

Für das gewünschte Verhalten reicht dieses interne Modell.

### 1. Map

Statische Welt:

- perimeter
- hardNoGo
- dock

Keine Missionslogik.

### 2. Mission

Fachlicher Auftrag:

- geordnete Liste von Zonen
- Zonenparameter
- Referenz auf die Karte

### 3. ZonePlan

Kompiliertes Ergebnis einer einzelnen Zone:

- zoneId
- Zonenparameter zum Planungszeitpunkt
- vollständige Bahnfolge dieser Zone
- Startpunkt
- Endpunkt

Wichtig:

- Eine Zone erzeugt genau einen ZonePlan.

### 4. MissionPlan

Kompiliertes Ergebnis einer Mission:

- geordnete Liste von ZonePlans
- Verbindungen zwischen den Zonen
- vollständige globale Bahnfolge

Wichtig:

- Vorschau, Start und Dashboard benutzen MissionPlan.

### 5. RuntimeState

Live-Zustand der Ausführung:

- aktiver Plan
- aktuelles Segment
- nächstes Ziel
- bereits gefahrene Segmente oder Punkte
- lokaler Detour, falls nötig

### 6. LocalDetour

Temporäre lokale Umfahrung:

- Hindernisreferenz
- temporäre Zusatzroute
- Wiedereinstiegspunkt in den Originalplan

Wichtig:

- LocalDetour ersetzt nicht den MissionPlan.

## Planungslogik pro Zone

### Grundsatz

Eine Zone wird nicht live interpretiert, sondern in eine konkrete Bahnfolge kompiliert.

### Schrittfolge

1. Zone auf erlaubte Fläche beschneiden

- Zone ∩ Perimeter − Hard-No-Go

2. Mähbare Teilflächen bestimmen

- falls No-Go oder Geometrie die Fläche trennt, entstehen mehrere Teilflächen

3. Für jede Teilfläche ein lokales Bahnenbild erzeugen

- klassisches Stripe- oder anderes Zonenmuster
- lokal konsistente Reihenfolge

4. Teilfläche lokal sinnvoll fertig mähen

- nicht nach jedem halben Streifen auf die andere Seite springen
- nicht global chaotisch über die Zone wechseln

5. Erst danach nächste Teilfläche anbinden

- nur über kurze, plausible Verbindungen
- falls nicht plausibel verbindbar: Fehler oder Split-Entscheidung

## Fachregel für No-Go-Zonen

Wenn eine No-Go-Zone Bahnen teilt, dann gilt produktseitig:

- der Roboter mäht erst die aktuelle Seite sinnvoll weiter
- er springt nicht nach jeder geteilten Bahn auf die andere Seite
- Querwechsel über die gesamte Zone sind Ausnahme, nicht Standard

Das bedeutet intern:

- Stripe-Teilstücke müssen lokal gruppiert werden
- Verbindungen werden blockweise geplant, nicht blind segmentweise
- die lokale Mählogik ist wichtiger als globale Segmentverkettung

## Dashboard-Modell

### Muss sichtbar sein

- Mission und Zonen
- geplante Bahnen
- bereits gefahrene Bahnen
- Roboterposition
- aktuelles Ziel
- spontane Hindernisse
- lokaler Umweg, wenn aktiv

### Muss nicht sichtbar sein

- warum der Planner intern etwas so gewählt hat
- ob eine Verbindung intern als Komponente, Transit oder Recovery geführt wird

## Soll-Verhalten bei Startarten

### Normaler Missionsstart

- MissionPlan wird geladen oder kompiliert
- genau dieser Plan wird gefahren

### Jetzt mähen

- selektierte Zone wird zu genau einem ZonePlan kompiliert oder aus Cache geladen
- exakt dieser ZonePlan wird gefahren

### Wiederaufnahme

- Runtime setzt auf dem bestehenden Plan auf
- keine komplette Neuberechnung ohne ausdrücklichen Grund

## Modulverantwortung im Zielsystem

### Map Editor

Verantwortung:

- statische Karte erzeugen und bearbeiten

Keine Verantwortung:

- Missionslogik
- Fahrplanlogik

### Mission Editor

Verantwortung:

- Zonen erzeugen und bearbeiten
- Zonenparameter setzen
- aktive Zone vorschauen
- Mission speichern

Keine Verantwortung:

- eigene Geometrieplanung im Frontend
- eigene Laufzeitlogik

### Planner Backend

Verantwortung:

- ZonePlan erzeugen
- MissionPlan erzeugen
- Plan validieren
- Plan an UI und Runtime liefern

### Runtime

Verantwortung:

- vorhandenen Plan abfahren
- Fortschritt führen
- lokale Umwege fahren
- auf Originalplan zurückkehren

Keine Verantwortung:

- Mission bei jedem Problem neu interpretieren

### Dashboard

Verantwortung:

- Plan und Live-Zustand rendern

Keine Verantwortung:

- Plan neu rechnen
- UI-seitig Bahnen erzeugen

## Technische Leitplanken für die spätere Umsetzung

### Muss gelten

1. Vorschau = Plan = Fahrroute
2. Karte bleibt statisch
3. Mission bleibt fachlich stabil
4. Hindernisse erzeugen nur lokale temporäre Abweichungen
5. Frontend rendert Backend-Daten

### Darf nicht mehr passieren

1. mehrere konkurrierende Wahrheiten für dieselbe Route
2. globale Neuinterpretation einer Zone wegen eines lokalen Problems
3. chaotisches Hin- und Herspringen zwischen Teilflächen
4. Dashboard und Vorschau zeigen unterschiedliche fachliche Zustände
5. technische Innenbegriffe dominieren die Benutzeroberfläche

## Priorisierte Neuplanung

### Phase 1: Verantwortungen festziehen

- Map nur statische Geometrie
- Mission nur Zonen und Parameter
- Plan als eigenes Ergebnisobjekt definieren

### Phase 2: Einzelzone stabil machen

- eine Zone muss deterministisch zu genau einer Bahnfolge werden
- lokale Mähreihenfolge muss plausibel sein

### Phase 3: Mission aus Zonen zusammensetzen

- ZonePlans geordnet verbinden
- keine implizite zweite Route erzeugen

### Phase 4: Dashboard strikt auf Plan ausrichten

- geplante Bahnen
- gefahrene Bahnen
- Zielpunkt
- Robotersymbol

### Phase 5: Lokale Hindernisumfahrung sauber isolieren

- LocalDetour als temporäre Laufzeitüberlagerung
- Rückkehr auf den ursprünglichen Plan

## Entscheidungssatz

Die Vorschau ist nicht ein Zusatzfeature.
Sie ist die sichtbare Form des echten Fahrplans.

Wenn das System diesen Satz ernst nimmt, werden Kartenmodus, Missionsmodus, Dashboard und Runtime automatisch einfacher und konsistenter.

## Ergebnis dieses Dokuments

Dieses Dokument ersetzt nicht die vorhandenen Werkzeuge der Karten- und Missionsseite.

Es definiert stattdessen:

- was diese Werkzeuge fachlich erzeugen sollen
- welche Datenhoheit Karte, Mission und Runtime haben
- wie Vorschau, Start und Dashboard zusammenhängen
- wie Hindernisse sich verhalten sollen

Damit ist die spätere technische Umsetzung an einem einfachen Produktablauf ausgerichtet, nicht an gewachsenen Planner-Altlasten.