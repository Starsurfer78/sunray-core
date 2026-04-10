# Sunray WebUI - Desktop/Tablet UX Plan

## Zielbild

Die WebUI wird auf Desktop und Tablet als klare Arbeitsoberflaeche fuer drei Hauptaufgaben optimiert:

1. Dashboard: Roboterstatus sofort erfassen und schnell handeln.
2. Karten: Kartenaufnahme, Validierung und Kartenverwaltung getrennt und gefuehrt bearbeiten.
3. Missionen: Zonenreihenfolge, Zonenparameter und Startvorschau ohne Formularueberladung steuern.

Mobile Smartphone-Optimierung ist nicht prioritaer, da dafuer die App gedacht ist. Tablet soll jedoch sauber bedienbar bleiben.

## Rahmenbedingungen

1. Die Status-Sidebar des Dashboards bleibt als schneller Ueberblick erhalten.
2. Schrift soll insgesamt nicht groesser werden; in einigen Bereichen eher ruhiger und kompakter.
3. Karten und Missionen muessen deutlich entladen werden.
4. Analyse und Diagnose muessen klarer strukturiert werden.
5. Perimeter und No-Go-Zonen werden weiterhin manuell angelegt: Roboter an Position bringen und Punkt per Button speichern.
6. Die RTK-Qualitaet muss beim Aufnehmen permanent sichtbar sein.
7. Sicheres Speichern ist bei RTK Fix direkt freigegeben; RTK Float nur mit Warnung und sichtbarer Genauigkeit; ohne brauchbaren RTK-Status wird Speichern blockiert.
8. Von den aktuellen Funktionen darf nichts verloren gehen; sie werden nur neu gruppiert.

## Informationsarchitektur

### 1. Dashboard

Beibehalten:

1. Permanente Status-Rail mit Kernwerten des Roboters.
2. Karte als Hauptflaeche.
3. Schnelle Aktionen fuer Start, Stop, Dock, Joystick.

Verbessern:

1. Status in drei Ebenen trennen:
   - Sofort relevant: Betriebszustand, GPS, Akku, Fehler, Startfreigabe.
   - Laufend relevant: Position, aktive Mission, Sensorflags.
   - Detail/Debug: nur aufklappbar.
2. Bottom-Log kompakter und besser filterbar machen.
3. Globales Info-Fenster nicht mehr als frei schwebendes Standard-Element behandeln.

### 2. Karten

Die Kartenansicht wird in drei klar getrennte Bereiche aufgeteilt:

1. Gefuehrter Aufnahme-Flow
   - RTK pruefen
   - Perimeter aufnehmen
   - No-Go-Zonen aufnehmen
   - Dock setzen
   - Validieren
   - Speichern/Aktivieren
2. Karten-Arbeitsflaeche
   - grosse Karte in der Mitte
   - nur kontextbezogene Werkzeuge sichtbar
3. Kartenbibliothek
   - Laden
   - Aktivieren
   - Duplizieren
   - Loeschen
   - Import/Export

Zusaetzliche Anforderung fuer die Aufnahme:

1. Die Aufnahme ist an die echte Roboterposition gekoppelt, nicht an frei gesetzte Kartenklicks.
2. Die Hauptaktion im Workflow lautet fachlich "Aktuelle Position speichern".
3. Vor dem Speichern sind RTK-Typ, Genauigkeit und aktuelle Roboterkoordinaten sichtbar.
4. Speicherlogik:
   - RTK Fix: normal freigegeben
   - RTK Float: nur mit klarer Warnung und sichtbarer Genauigkeit
   - kein RTK / unbrauchbar: gesperrt
5. Undo/Redo, Draft-Wiederherstellung, Import/Export, Bibliothek, Aktivieren und Validierung bleiben erhalten.

Wichtige Aenderung:

Die eigentliche Aufnahme darf nicht mit Bibliotheksverwaltung, Import/Export und Expertenoptionen im selben visuellen Block konkurrieren.

Empfohlene Unterteilung im Karten-Workflow:

1. Aufnahmebereich
   - aktueller Schritt
   - RTK Status
   - Genauigkeit
   - aktuelle Position
   - Button "Aktuelle Position speichern"
   - Button "Schritt abschliessen"
2. Validierungsbereich
3. Kartenbibliothek

### 3. Missionen

Die Missionsseite wird in drei Spalten oder Funktionsblaecke gegliedert:

1. Missionsliste
2. Missionskonfiguration
   - Zonenreihenfolge
   - Parameter der gewaehlten Zone
   - Zeitplan
3. Live-Vorschau
   - Routen-Preview
   - Validierungsstatus
   - Startfreigabe

Wichtige Aenderung:

Missionserstellung soll sich wie ein Builder anfuehlen, nicht wie ein Admin-Formular.

### 4. Analyse & Diagnose

Diese Ansicht wird in klar benannte Sektionen zerlegt:

1. Systemstatus
   - Versionen
   - Runtime Health
   - letztes relevantes Ereignis
2. Service
   - OTA
   - STM Flash
   - Neustartnahe Aktionen
3. Hardware-Diagnose
   - Sensoren
   - IMU
   - Motoren
   - Tick-Kalibrierung
4. Expertenprotokoll
   - Rohdaten
   - Diagnoseausgabe
   - technische Details

Wichtige Aenderung:

Allgemeine Einstellungen und Hardware-Diagnose nicht mehr in derselben visuellen Dichte praesentieren.

## Design-Regeln

### Typografie

1. Keine weitere Vergroesserung der UI-Schrift als Standard.
2. Basisgroessen:
   - Primäre UI-Schrift: 13px bis 14px
   - Sekundaere Labels: 11px bis 12px
   - Kritische Kennzahlen: 18px bis 22px
3. Monospace nur fuer echte technische Werte.

### Dichte

1. Dashboard darf kompakt bleiben.
2. Karten und Missionen bekommen mehr Luft und weniger gleichzeitige Elemente.
3. Diagnose darf dichter sein als Missionen, aber mit klaren Gruppen.

### Komponentenstil

1. Eine gemeinsame Kartenkomponente fuer Panels, Tabellen, Formboxen und Statuskarten.
2. Konsistente Zustandsfarben:
   - Blau: Information / aktiv
   - Gruen: OK / freigegeben
   - Amber: Hinweis / Vorsicht
   - Rot: Fehler / Safety
3. Keine frei schwebenden Fenster als Standardmuster in Desktop/Tablet-Workflows.

## Umsetzung in Phasen

### Phase 1 - Struktur und Tokens

Dateien mit hoher Prioritaet:

1. src/App.svelte
2. src/app.css
3. src/lib/components/PageLayout.svelte

Aufgaben:

1. Topbar vereinfachen.
2. Typografie und Groessen harmonisieren.
3. Layout-Bausteine fuer Arbeitsflaeche, Rail und Inspektor vereinheitlichen.

### Phase 2 - Karten neu strukturieren

Dateien mit hoher Prioritaet:

1. src/lib/pages/Map.svelte
2. src/lib/components/Map/MapCanvas.svelte
3. neue Komponenten fuer Workflow, Bibliothek und Validierung

Aufgaben:

1. Kartenaufnahme-Flow sichtbar machen.
2. Kartenbibliothek separat darstellen.
3. Werkzeuge kontextbezogen statt dauerhaft zeigen.

### Phase 3 - Missionen entschlacken

Dateien mit hoher Prioritaet:

1. src/lib/pages/Mission.svelte
2. src/lib/components/Mission/PathPreview.svelte
3. neue Komponenten fuer Missionsliste und Zonenreihenfolge

Aufgaben:

1. Missionsliste links.
2. Konfiguration in der Mitte.
3. Vorschau rechts oder unterhalb.

### Phase 4 - Analyse & Diagnose ordnen

Dateien mit hoher Prioritaet:

1. src/lib/pages/Settings.svelte
2. src/lib/components/Diagnostics/*

Aufgaben:

1. Service, Diagnose und Rohdaten visuell trennen.
2. Wichtige Statuswerte zuerst, Expertendetails nachgeordnet.

## Konkrete Komponenten-Ziele

### Dashboard

Behalten:

1. Status-Rail.
2. Kartenfokus.
3. Schnellaktionen.

Anpassen:

1. Bottom-Panel hoehenflexibel statt starrer Debug-Box.
2. Info-Fenster als einblendbarer Inspector statt frei verschiebbares Standard-Widget.

### Karten

Neue Komponenten:

1. MapWorkflowPanel
2. MapLibraryPanel
3. MapValidationPanel
4. MapContextToolbar
5. MapCaptureStatusCard
6. MapSaveGateCard

Pflichtinhalte fuer den Aufnahmebereich:

1. RTK-Zustand Fix / Float / kein Fix
2. Genauigkeit in Metern
3. aktuelle Roboterkoordinaten
4. Primärbutton "Aktuelle Position speichern"
5. sichtbarer Speicherstatus: freigegeben, gewarnt oder gesperrt

### Missionen

Neue Komponenten:

1. MissionListPanel
2. MissionSequencePanel
3. MissionSchedulePanel
4. MissionPreviewStatusCard

### Analyse & Diagnose

Neue Komponenten:

1. SystemOverviewPanel
2. ServiceActionsPanel
3. HardwareDiagnosticsPanel
4. ExpertLogPanel

## Tablet-Regeln

1. Keine Smartphone-Sonderlogik als Leitmodell.
2. Dashboard-Rail darf schmaler werden, aber nicht verschwinden.
3. Karten- und Missionsansicht dürfen von 3-Spalten auf 2-Spalten wechseln.
4. Detailinspektoren duerfen unter die Hauptflaeche rutschen.

## Erfolgskriterien

1. Dashboard bleibt auf einen Blick erfassbar.
2. Kartenansicht fuehlt sich wie ein gefuehrter Arbeitsprozess an.
3. Missionsansicht trennt Reihenfolge, Parameter und Vorschau sichtbar.
4. Diagnose ist schneller scanbar und weniger ueberladen.
5. Schrift bleibt kompakt, aber lesbar.
6. Der manuelle Aufnahmeprozess fuer Perimeter und No-Go ist klar und sicher fuehrbar.
7. Bestehende Funktionen bleiben erhalten und gehen beim Umbau nicht verloren.

## Empfohlene Reihenfolge

1. App-Struktur und visuelle Tokens vereinheitlichen.
2. Kartenansicht umbauen.
3. Missionsansicht umbauen.
4. Analyse & Diagnose neu ordnen.
5. Dashboard nur punktuell verfeinern, nicht komplett neu erfinden.