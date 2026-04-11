## Mobile App Refactoring

Status-Legende:

- umgesetzt: im Flutter-Code erkennbar umgesetzt
- teilweise umgesetzt: Richtung ist im Code erkennbar, aber nicht vollstaendig oder nicht hart durchgesetzt
- offen: im Zielbild beschrieben, aber im Code noch nicht ausreichend umgesetzt

Stand 2026-04-11:

- Dashboard-first Struktur umgesetzt
- Karten-Setup-Wizard umgesetzt
- Add-more und Edit-Trennung umgesetzt
- zustandsabhaengiges Dashboard umgesetzt
- Zeitplan-Flow als eigener Verwaltungsablauf umgesetzt
- sekundaere Bereiche fuer Benachrichtigungen und Einstellungen teilweise umgesetzt

### Zielbild

Status: umgesetzt

Die Mobile App orientiert sich als Zielbild im Alltag an der Navimow-App:

- kartenzentriert
- ruhig und aufgeraeumt
- wenige starke Hauptaktionen
- geringe Navigationstiefe im taeglichen Gebrauch
- klare Trennung zwischen Betrieb, Bearbeitung und Einrichtung

Die Karte ist die Hauptarbeitsflaeche; das Dashboard ist jetzt auch der tatsaechliche erste Screen der App.

## Produktprinzipien

### 1. Ein starker Hauptscreen

Status: umgesetzt

Es gibt einen dominanten Alltagsscreen: das Dashboard mit Karte.

Im aktuellen Code ist dieser Alltagsscreen jetzt auch als echter App-Einstieg durchgesetzt.

Von dort aus erreicht der Nutzer:

- Status
- Roboterposition
- Kartenkontext
- Zonenauswahl
- Maehstart
- Stop und Dock
- Planung
- Bearbeitung und Konfiguration

### 2. Karte ist Arbeitsflaeche, nicht Dekoration

Status: umgesetzt

Die Karte zeigt im Alltag nur die relevanten Elemente:

- Perimeter
- No-Go-Bereiche
- Dock
- Zonen
- Roboterposition
- aktiven Aufgabenkontext

Bearbeitungsgriffe und Werkzeuglogik sind nicht mehr dauerhafte Standardansicht.

### 3. Betrieb vor Konfiguration

Status: teilweise umgesetzt

Der Alltag fokussiert auf:

- jetzt maehen
- Zone auswaehlen
- stoppen
- zur Station schicken

Zeitplaene, Karteneditor, Statistik und Einstellungen sind sekundaer erreichbar.

### 4. Klare Modi statt Mischzustaende

Status: teilweise umgesetzt

Die App trennt bereits deutlich besser zwischen:

- Ansehen
- Hinzufuegen
- Bearbeiten
- Ausfuehren

Dashboard, Setup und Editor sind weitgehend getrennt, auch wenn der Gesamteinstieg noch nicht voll auf das Zielbild ausgerichtet ist.

### 5. Ruhiger Default-Zustand

Status: umgesetzt

Im Standardzustand gilt:

- keine dauerhaften Punktgriffe
- keine grosse Werkzeugleiste
- keine ueberladenen Kartenoverlays
- klare Primaeraktion

## Informationsarchitektur

Status: teilweise umgesetzt

Die App gliedert sich erkennbar in drei fachliche Phasen, auch wenn die Grenzen in der Bedienung noch nicht ueberall voll ausgeschaerft sind.

### A. Setup

Status: umgesetzt

Beinhaltet:

- Roboter finden und verbinden
- Empty Dashboard
- Karte anlegen
- Perimeter definieren
- No-Go-Bereiche anlegen
- Dock festlegen
- Validierung
- Speichern

### B. Konfiguration

Status: teilweise umgesetzt

Beinhaltet:

- Zonen anlegen
- Zonen bearbeiten
- Zonennamen pflegen
- Zeitplaene verwalten
- spaetere Kartenanpassungen

### C. Betrieb

Status: umgesetzt

Beinhaltet:

- Live-Karte
- Roboterstatus
- aktive oder naechste Aufgabe
- Zonenauswahl
- Maehstart
- Stop und Dock
- Warnungen und Probleme

## Navigationsmodell

Status: umgesetzt

Die App ist keine gleichgewichtete Tab-App mehr.

### Primaernavigation

Status: umgesetzt

Aktueller primaerer Einstieg:

- Dashboard

### Sekundaernavigation

Status: umgesetzt

Sekundaere Bereiche werden bereits aus dem Dashboard oder ueber Flows geoeffnet:

- Kartensetup
- Kartenbearbeitung
- Statistik
- Benachrichtigungen
- Einstellungen

### Interaktionsmittel

Status: umgesetzt

Bevorzugt werden:

- Bottom Sheets
- Fullscreen-Flows
- modale Editor-Zustaende

## Der wichtigste Screen: Dashboard

Status: umgesetzt

Das Dashboard ist die zentrale Ansicht fuer Alltag und Kontrolle und zugleich der erste Einstiegspunkt der App.

### Inhalt

Status: umgesetzt

- grosse Karte als Hauptflaeche
- Roboterposition
- Perimeter, Zonen, No-Go, Dock
- kompakter Statusbereich
- missionsbezogener Fokus
- Primaeraktionen

### Primaeraktionen

Status: umgesetzt

- Maehen
- Stop
- Dock
- Planung oeffnen
- Karte bearbeiten oder anlegen je nach Zustand

### Verhalten

Status: teilweise umgesetzt

- Tap auf Zone selektiert und hebt hervor
- Tap auf aktive Mission oeffnet Detailsheet
- Quick Actions oeffnen sekundaere Bereiche
- Status und Aufbau reagieren auf den Systemzustand

### Gestaltung

Status: umgesetzt

- Karte dominiert die Ansicht
- kompakte Status- und Aktionslayer
- ruhige Bedienelemente
- Daumen-nahe Hauptaktionen

## Empty Dashboard

Status: umgesetzt

Wenn noch keine Karte vorhanden ist, fuehrt das Dashboard in den naechsten sinnvollen Schritt.

### Inhalt

Status: umgesetzt

- Roboterstatus
- Verbindung
- Akku
- Hinweis auf fehlende Karte
- starke Primaeraktion zum Kartensetup
- sekundaere Aktionen ueber Quick Actions

### Ziel

Status: umgesetzt

Im Dashboard erkennt der Nutzer:

- ob das Geraet erreichbar ist
- was als naechstes zu tun ist

## Kartenmodell in der App

Status: umgesetzt

Die Karte bleibt visuell das Zentrum der App.

### Sichtbar im Normalmodus

Status: umgesetzt

- Hauptflaeche
- No-Go-Bereiche
- Dock
- Zonen
- Roboterposition
- relevanter Aufgabenkontext

### Nicht dauerhaft sichtbar

Status: umgesetzt

- Editierpunkte
- geometrische Hilfslinien
- Debug-Informationen
- grosse technische Infobloecke
- volle Werkzeugleisten

## Setup-Flow fuer die Karte

Status: teilweise umgesetzt

Das Anlegen der Karte ist bereits als gefuehrter Flow angelegt, die fachliche Haertung ist aber noch nicht vollstaendig.

### Schritt 1: Hauptflaeche

Status: teilweise umgesetzt

- Punkte setzen
- letzten Punkt loeschen
- Umriss schliessen

### Schritt 2: No-Go-Bereiche

Status: umgesetzt

No-Go-Bereiche werden als eigener Wizard-Schritt ergaenzt.

### Schritt 3: Dock

Status: umgesetzt

Der Dockpunkt wird als eigener Wizard-Schritt gesetzt.

### Schritt 4: Validierung

Status: teilweise umgesetzt

Vor dem Speichern wird geprueft:

- Perimeter vorhanden
- No-Go-Bereiche gueltig oder leer
- Dock vorhanden
- vorhandene Zonen mindestens strukturell konsistent

### Schritt 5: Speichern

Status: teilweise umgesetzt

Zielbild: Die Basiskarte soll erst am Ende des Setup-Ablaufs uebernommen werden.

Ist-Zustand: Speichern ist aktuell auch ausserhalb des finalen Setup-Schritts erreichbar.

## Spaetere Kartenbearbeitung

Status: umgesetzt

Nach dem Setup gibt es einen getrennten Bearbeitungsmodus.

### Normalzustand des Kartenscreens

Status: umgesetzt

Der ruhige Zustand zeigt vor allem zwei Wege:

- Add more
- Edit

### Add more

Status: teilweise umgesetzt

Bottom Sheet fuer neue Kartenelemente:

- Zone
- No-Go-Bereich
- Dock
- Boundary fuer initiale Grundflaeche als Zielbild, derzeit aber nicht sauber separat umgesetzt

### Edit

Status: umgesetzt

Bewusster Bearbeitungsmodus fuer:

- Objekt auswaehlen
- Punkte sichtbar machen
- verschieben
- loeschen
- Bearbeitung beenden

## Zonen

Status: teilweise umgesetzt

Zielbild: Zonen bauen auf der gueltigen Basiskarte auf.

Ist-Zustand: Diese Regel ist jetzt fuer die App-Interaktion deutlich haerter abgesichert, weil Zonenanlage erst nach vorhandenen Perimeter- und Dock-Daten freigeschaltet wird.

### Warum

Status: teilweise umgesetzt

Zonen sind bereits als steuerbare Arbeitsbereiche auf Basis der vorhandenen Karte angelegt.

### Zonen-Flow

Status: teilweise umgesetzt

- Zonenuebersicht im Planen-Bereich
- Zone anlegen im Kartenflow
- Zone bearbeiten im Edit-Flow
- Zonennamen direkt pflegen

### Erste Zoneneinstellungen

Status: teilweise umgesetzt

Reduziert auf einen einfachen Einstieg:

- Name
- Auswahl in Missionen
- Muster und Einsatzkontext ueber Mission

### Darstellung auf der Karte

Status: teilweise umgesetzt

- sichtbare, aber nicht dominante Flaechen
- klare Auswahl per Tap
- Hervorhebung der aktiven Zone

## Zeitplaene

Status: teilweise umgesetzt

Zeitplaene sind im aktuellen Produktfluss nach Karte und Zonen vorgesehen und im Planen-Bereich verortet.

### Moegliche Ziele

Status: teilweise umgesetzt

- gesamte Missionsflaeche
- einzelne oder mehrere Zonen ueber Missionsauswahl

### Flow

Status: umgesetzt

- Uebersicht im Planen-Bereich
- Zeitplan anlegen per Bottom Sheet
- Zeitplan bearbeiten per Bottom Sheet

### Inhalt eines Zeitplans

Status: teilweise umgesetzt

- Tage
- Uhrzeit
- Wiederholung
- Zielbereich ueber Mission
- Muster
- Bedingungen

## Betriebs-Dashboard im Detail

Status: teilweise umgesetzt

Das Betriebs-Dashboard ist bereits als Alltagsansicht nutzbar, bildet das Zielbild aber noch nicht in allen Bedienfaellen voll ab.

### Sichtbar

Status: teilweise umgesetzt

- Karte
- Roboterposition
- Status
- Akku
- Verbindung
- aktuelle oder naechste Aufgabe

### Aktionen

Status: teilweise umgesetzt

- Mission starten
- Stop
- Dock
- zustandsbezogene Folgeaktionen

### Interaktion mit Zonen

Status: umgesetzt

- Zone antippen
- Zone wird hervorgehoben
- danach Aktion oder Kontext ausloesen

## Zustaende der App

Status: teilweise umgesetzt

Die App kennt bereits mehrere dieser Zustaende, aber nicht alle sind als klar unterscheidbare UX-Lage gleich stark ausgearbeitet.

### Hauptzustaende

Status: teilweise umgesetzt

- kein Geraet verbunden
- verbunden, keine Karte
- Karte vorhanden, keine Zonen
- Karte und Zonen vorhanden, kein Zeitplan
- betriebsbereit
- maeht gerade
- pausiert
- Fehlerzustand
- offline oder stale

### UX-Regel

Status: umgesetzt

Das Dashboard passt seinen Aufbau bereits sichtbar an den Zustand an.

## Visuelles und Interaktionssystem

Status: teilweise umgesetzt

Die gestalterische Richtung ist im Code klar erkennbar, aber noch nicht in jedem Detail konsequent abgeschlossen.

### Gewuenscht

Status: teilweise umgesetzt

- klare Hierarchie
- grosse Kartennutzung
- wenige Farben mit klarer Bedeutung
- starke Primaeraktion
- ruhige Bottom Sheets
- deutliche Trennung zwischen View und Edit

### Nicht gewuenscht

Status: teilweise umgesetzt

- zu viele Haupttabs
- zu viele gleichzeitige Overlays
- technische Startscreens
- gemischte Bearbeiten-und-Hinzufuegen-Zustaende
- dauerhafte Infokarten auf der Karte
- ueberladene Toolleisten

## Zielstruktur der App

Status: teilweise umgesetzt

Die folgende Struktur ist groesstenteils erkennbar, an einigen Stellen aber noch eher Zielbild als vollendeter Ist-Zustand.

### 1. Dashboard

Status: teilweise umgesetzt

Zentrale Ansicht fuer Alltag und perspektivisch auch fuer den Einstieg.

### 2. Karten-Flow

Status: teilweise umgesetzt

Perimeter, No-Go, Dock, Validierung, Speichern.

### 3. Zonen-Flow

Status: umgesetzt

Zonen anlegen, bearbeiten und eine einfache Konfiguration sind vorhanden.

### 4. Zeitplaene

Status: teilweise umgesetzt

Automatisierte Maehvorgaenge verwalten ist moeglich und laeuft aktuell ueber den Missionskontext.

### 5. Kartenbearbeitung

Status: teilweise umgesetzt

Spaetere Aenderungen ueber Add more und Edit.

### 6. Einstellungen und Benachrichtigungen

Status: teilweise umgesetzt

Sekundaere Bereiche ausserhalb des Kernflusses sind vorhanden, aber funktional noch eher schlank.

## Abgleich mit oeffentlich sichtbaren Navimow-Mustern

Status: teilweise umgesetzt

Die umgesetzte Richtung folgt bereits mehreren dieser stabilen Muster:

- Dashboard als fachliches Zentrum
- Karte dominiert visuell
- kompakte Aktionsebenen
- gefuehrtes Setup
- sekundaere Verwaltung

### Konsequenz fuer unser Zielbild

Status: teilweise umgesetzt

- weniger Root-Navigation
- mehr Dashboard
- mehr kontextuelle Sheets
- weniger dauerhafte Verwaltungstabs
- klarere Setup- und Edit-Modi

## Konkrete Leitentscheidung

Status: teilweise umgesetzt

Als Produktziel gilt bei Unsicherheit weiterhin als Default:

- weniger Menutiefe
- mehr Karte
- weniger gleichzeitige Aktionen
- klarerer Zustand
- zuerst Alltag, dann Konfiguration

## Ein-Satz-Zielbild

Status: teilweise umgesetzt

Die App entwickelt sich in Richtung einer ruhigen, kompakten, kartenzentrierten Maehroboter-App mit starkem Dashboard, getrenntem Mapping-Flow, einfacher Planung und schneller Bedienung im Alltag.

## Konkrete Screen-Map

Status: umgesetzt

Die folgende Screen-Map beschreibt den aktuellen Aufbau mit Zielbildcharakter in einzelnen Bereichen.

### A. Root-Ebene und Sekundaerbereiche

Status: umgesetzt

- Dashboard
- Discovery / Connect
- Empty Dashboard
- Map Setup Flow
- Map Edit Flow
- Planen
- Statistik
- Benachrichtigungen
- Einstellungen
- Service

### B. Overlays und Bottom Sheets

Status: umgesetzt

- Quick Actions Sheet
- Mission Detail Sheet
- Schedule Sheet
- Add More Sheet

### C. Zielnavigation

Status: teilweise umgesetzt

Aktuell vorhandene Hauptnavigation:

- Dashboard
- Planen
- Service

## Refactoring-Backlog fuer die Flutter-App

Status: umgesetzt

Die Phasen unten waren als Arbeitsrichtung definiert; ihr Abschlussgrad ist im Code unterschiedlich weit fortgeschritten.

### Phase 1: Navigation vom Produktziel her neu ordnen

Status: umgesetzt

### Phase 2: Dashboard produktfaehig machen

Status: umgesetzt

### Phase 3: Karteneditor auf klare Modi umbauen

Status: umgesetzt

### Phase 4: Missionsscreen in Zonen- und Planungsbereiche aufteilen

Status: umgesetzt

### Phase 5: Service und Statistik in die zweite Reihe schieben

Status: umgesetzt

### Phase 6: UX-Polish auf Navimow-Niveau bringen

Status: teilweise umgesetzt

## Reihenfolge der Umsetzung

Status: teilweise umgesetzt

Die wesentlichen Refactoring-Schritte wurden weitgehend in der Zielreihenfolge begonnen oder umgesetzt, aber nicht alle Phasen sind fachlich abgeschlossen.

## Harte UX-Entscheidung fuer dieses Refactoring

Status: teilweise umgesetzt

Im Zweifel soll die kartenzentrierte, ruhigere und alltagsnaehere Produktlogik gewinnen; im aktuellen Code ist diese Richtung klar erkennbar, aber noch nicht ueberall voll durchgesetzt.

## Definition of Done fuer das Refactoring

Status: teilweise umgesetzt

Aktueller Stand: technisch wichtige Grundlagen sind vorhanden, aber mehrere Punkte gelten eher als Arbeitsziel als als vollstaendig erreichte Definition of Done.

- Dashboard ist das Zentrum
- Karte ist die Hauptarbeitsflaeche
- Setup und Edit sind getrennt
- Planung ist reduziert und klar gegliedert
- sekundaere Bereiche sind ausgelagert
- Flutter Analyzer ist gruen

## TODO nach Codeabgleich 2026-04-11

Status: offen

Diese TODO orientiert sich am Original-Zielbild und dokumentiert fuer jeden Anspruch, was im Flutter-Code heute wirklich belegt ist.

### 1. Grundidee der App

- `umgesetzt` Karte ist eine zentrale Arbeitsflaeche im Dashboard.
- `umgesetzt` Status, Karte, Zonen, Maehstart, Dock und Planung sind von dort erreichbar.
- `offen` Zeitplaene werden nicht direkt vom Dashboard aus verwaltet, sondern ueber den Planen-Bereich.
- `umgesetzt` Die App wirkt kompakter und ruhiger als vorher.
- `umgesetzt` Das Produktziel einer direkten Geraeteoberflaeche ohne vorgeschalteten Technik-Einstieg ist jetzt deutlich besser getroffen, weil der Start auf dem Dashboard liegt.

### 2. Leitprinzipien des UX

- `umgesetzt` Ein starker Hauptscreen ist vorhanden und als echter App-Einstieg durchgesetzt.
- `teilweise umgesetzt` Wenige starke Primaeraktionen sind im Dashboard sichtbar.
- `offen` Pause und Fortsetzen sind im Dashboard nicht als klarer eigener Alltagsfluss erkennbar.
- `umgesetzt` Die Karte ist im Alltag klar der zentrale Arbeitsbereich.
- `teilweise umgesetzt` Zusatzfunktionen werden ueber Quick Actions, weitere Screens und Sheets geoeffnet.
- `umgesetzt` Der Standardzustand ist vergleichsweise ruhig, ohne dauerhafte Editiergriffe.
- `umgesetzt` Das Ziel "keine gleichwertigen Tabs" ist erreicht, weil die gleichrangige Bottom-Navigation entfernt wurde.

### 3. Hauptfluss der App

- `umgesetzt` App starten -> Roboter hinzufuegen -> Empty Dashboard ist im Code als moeglicher Ablauf erkennbar.
- `umgesetzt` Karten-Setup mit Grundflaeche, Ausschlussflaechen, Dock, Validierung und Speichern existiert als Flow.
- `teilweise umgesetzt` Zonen anlegen nach dem Setup ist vorgesehen, aber nicht technisch strikt an eine fertige Basiskarte gekoppelt.
- `offen` Ein eigener Schritt fuer Zoneneinstellungen ist nur rudimentaer vorhanden.
- `umgesetzt` Zeitplaene haben einen eigenen Verwaltungsablauf.
- `teilweise umgesetzt` Betriebs-Dashboard mit gesamte Karte maehen oder Zone maehen ist nur teilweise sichtbar.

### 4. Nutzungsphasen

- `umgesetzt` Setup als eigene Phase ist im Code klar erkennbar.
- `teilweise umgesetzt` Konfiguration ist vorhanden, aber Zoneneinstellungen sind noch nicht so klar ausgearbeitet wie im Zielbild.
- `umgesetzt` Betrieb als eigener Alltagsmodus ist im Dashboard klar angelegt.

### 5. Navigationsstruktur

- `umgesetzt` Die App folgt jetzt deutlich staerker dem Ziel "ein Hauptscreen statt klassische Multi-Tab-App".
- `umgesetzt` Das Dashboard ist jetzt der eindeutige zentrale Einstiegspunkt.
- `umgesetzt` Sekundaere Navigation ueber Quick Actions, Fullscreen-Screens und Bottom Sheets ist vorhanden.
- `umgesetzt` Bottom Sheets und Bearbeitungsmodi werden als Kontextnavigation genutzt.

### 6. Dashboard

- `umgesetzt` Karte, Roboterposition, Status, Akku und Verbindungszustand sind sichtbar.
- `teilweise umgesetzt` Zonen sind sichtbar und auswaehlbar.
- `teilweise umgesetzt` Aktuelle oder naechste Aufgabe wird nur teilweise transportiert.
- `umgesetzt` MOW und HOME beziehungsweise Stop und Dock sind als Kernaktionen vorhanden.
- `offen` Pause und Fortsetzen fehlen als klar ausgepraegte Standardaktionen.
- `teilweise umgesetzt` Tap auf Zone fuehrt zu Auswahl und Kontext, aber nicht konsistent zu einem klaren Zone-Detail- oder Startmuster.
- `offen` Tap auf Status -> Statusdetails ist so nicht als eigener Flow umgesetzt.
- `umgesetzt` Tap auf Karte bearbeiten fuehrt in den Editor-Flow.
- `umgesetzt` Der Screen bleibt im Normalzustand weitgehend ruhig und kompakt.

### 7. Empty Dashboard

- `umgesetzt` Ein zustandsabhaengiger Zustand fuer "verbunden, aber keine Karte" ist vorhanden.
- `umgesetzt` Der Nutzer bekommt Hinweis auf fehlende Karte und eine starke Aktion zum Kartensetup.
- `teilweise umgesetzt` Sekundaere Hilfsaktionen sind vorhanden, aber nicht exakt in der urspruenglich beschriebenen Form.
- `umgesetzt` Das Ziel "kein techniklastiger Startscreen" ist jetzt weitgehend erreicht, weil Discovery nicht mehr der erste Screen ist.

### 8. Kartenaufbau

- `umgesetzt` Hauptflaeche, Ausschlussflaechen, Dock, Zonen und Roboterposition sind sichtbar.
- `umgesetzt` Editierpunkte und Werkzeuge sind nicht dauerhaft im Normalmodus sichtbar.
- `umgesetzt` Die Karte wirkt im Normalmodus relativ clean.

### 9. Map Setup Flow

- `umgesetzt` Grundflaeche anlegen ist vorhanden.
- `umgesetzt` Letzten Punkt loeschen und weitere Bearbeitungsschritte sind vorhanden.
- `teilweise umgesetzt` Umriss abschliessen ist nur ueber die vorhandene Editorlogik implizit, nicht als besonders klarer Produktschritt.
- `umgesetzt` Ausschlussflaechen sind eigener Setup-Schritt.
- `teilweise umgesetzt` Dock wird gesetzt, aber ein Docking-Pfad im Sinne des Originaltexts ist nicht erkennbar.
- `teilweise umgesetzt` Validierung existiert, aber nur als einfache Strukturpruefung.
- `offen` Dock erreichbar und Karte fachlich brauchbar werden nicht tief geprueft.
- `offen` "Erst danach wird die Karte gespeichert" ist nicht hart abgesichert, weil Speichern auch vorher moeglich ist.

### 10. Kartenbearbeitung

- `umgesetzt` Normalzustand mit den klaren Aktionen Add more und Edit ist vorhanden.
- `umgesetzt` Add more enthaelt Zone, No-Go und Dock.
- `offen` Boundary als nachtraegliche Add-more-Option fuer die initiale Grundflaeche ist im Sinne des Originaldokuments nicht sauber getrennt umgesetzt.
- `offen` Channel ist nicht vorhanden. Das ist nur relevant, falls es fachlich weiterhin vorgesehen ist.
- `umgesetzt` Edit fuehrt in einen bewussten Bearbeitungsmodus.
- `umgesetzt` Ansehen, Hinzufuegen und Bearbeiten sind deutlich besser getrennt als zuvor.

### 11. Zonen

- `teilweise umgesetzt` Zonen kommen fachlich nach dem Kartenaufbau und diese Reihenfolge wird in der App nun deutlich strikter durchgesetzt.
- `umgesetzt` Zonen sind steuerbare Bereiche und in Missionen waehlbar.
- `teilweise umgesetzt` Zonenuebersicht, Anlegen und Bearbeiten sind vorhanden.
- `teilweise umgesetzt` Ein klarer separater Screen fuer Zoneneinstellungen fehlt weiterhin, aber eine direkte Konfiguration im Editor ist vorhanden.
- `umgesetzt` Als erste Einstellungen existieren Name, Prioritaet, Maehrichtung und Maehprofil.
- `umgesetzt` Prioritaet, Maehrichtung und Maehprofil sind als einfache Zoneneinstellungen umgesetzt.
- `umgesetzt` Zonen sind auf der Karte sichtbar und nicht ueberdominant.

### 12. Zeitplaene

- `umgesetzt` Zeitplaene kommen im Planen-Bereich nach Karte und Zonen.
- `umgesetzt` Uebersicht, Anlegen und Bearbeiten per Bottom Sheet sind vorhanden.
- `teilweise umgesetzt` Zielbereich ueber Mission beziehungsweise mehrere Zonen ist indirekt ueber Missionsauswahl abgebildet.
- `umgesetzt` Tage, Uhrzeit und Wiederholung sind umgesetzt.
- `teilweise umgesetzt` Das Ziel "gesamte Karte" ist nicht als eigener expliziter Zeitplanmodus sichtbar, sondern laeuft ueber Missionen.

### 13. Betriebs-Dashboard

- `umgesetzt` Karte, Roboterposition, Status, Akku und Verbindung sind sichtbar.
- `teilweise umgesetzt` Zone maehen ist moeglich ueber Missions-/Zonenauswahl.
- `offen` Komplette Karte maehen ist nicht als klar getrennte Standardaktion sichtbar.
- `offen` Pause und Fortsetzen fehlen als deutliche Primaeraktionen.
- `umgesetzt` Zone antippen hebt die Zone hervor.
- `teilweise umgesetzt` Danach kann Kontext ausgeloest werden, aber nicht als durchgaengig klarer Zone-Aktionsflow.

### 14. Zustaende der App

- `umgesetzt` Mehrere Hauptzustaende werden im Dashboard sichtbar unterschieden.
- `teilweise umgesetzt` Offline oder stale ist nur teilweise als eigener UX-Zustand ausgearbeitet.
- `umgesetzt` Das Dashboard reagiert sichtbar auf den Systemzustand.

### 15. Was wir bewusst nicht machen

- `umgesetzt` Die App vermeidet ueberladene Toolleisten im Standardzustand.
- `teilweise umgesetzt` Zu viele Haupttabs werden vermieden, aber es gibt weiterhin drei gleichrangige Hauptbereiche.
- `umgesetzt` Bearbeiten und Hinzufuegen sind weitgehend getrennt.
- `umgesetzt` Grosse dauerhafte Info-Sheets auf der Karte werden vermieden.
- `umgesetzt` Zu viele Zoneninfos im Standardzustand werden vermieden.

### 16. Finale App-Struktur

- `umgesetzt` Dashboard existiert als zentrale Alltagsansicht.
- `umgesetzt` Karten-Flow existiert.
- `teilweise umgesetzt` Zonen-Flow existiert nur teilweise in der Tiefe des Originaldokuments.
- `umgesetzt` Zeitplaene existieren als eigener Bereich.
- `umgesetzt` Kartenbearbeitung ueber Add more und Edit existiert.
- `umgesetzt` Einstellungen und Benachrichtigungen sind als sekundaere Bereiche ausgelagert.

### 17. Verdichtetes Fazit

- `umgesetzt` Die App ist jetzt deutlich kartenzentrierter, kompakter und ruhiger.
- `teilweise umgesetzt` Das volle Zielbild ist in den Kernpunkten deutlich naeher gerueckt; offen bleiben vor allem tiefe Kartenvalidierung, Statusdetails und einige Betriebsflows.
- `umgesetzt` Das Dokument ist jetzt an den aktuellen Ist-Zustand angepasst.

### Konkrete Folgearbeiten

- Statusangaben kuenftig nur noch nach echtem Codeabgleich aktualisieren.
- Dashboard-first entscheiden und konsequent umsetzen oder als Zielbild kennzeichnen statt als erledigt.
- Discovery und Dashboard fachlich zu einem sauberen Einstieg zusammensetzen.
- Navigationsmodell zwischen Produktziel und aktueller Bottom-Navigation bereinigen.
- Setup-Speichern auf den finalen Schritt begrenzen, wenn das weiter die Soll-Logik ist.
- Kartenvalidierung um fachliche Regeln erweitern.
- Zonen nur auf gueltiger Basiskarte erlauben, falls das Produktziel so bestehen bleibt.
- Zoneneinstellungen gezielt als eigenen kleinen Flow ausbauen.
- Pause/Fortsetzen und "gesamte Karte maehen" im Betriebs-Dashboard klarer machen.
