# USER_EXPERIENCE_IMPROVEMENTS.md

Stand: 2026-03-27

Dieses Dokument beschreibt die wichtigsten Verbesserungsmöglichkeiten im Benutzererlebnis von `sunray-core`.
Der Fokus liegt nicht auf neuer Kernfunktionalität, sondern auf:

- besserer Nutzerführung
- verständlicherem Status
- mehr Vertrauen im Betrieb
- weniger Bedienhürden

---

## Kurzfazit

`sunray-core` ist technisch bereits erstaunlich weit, aber im Benutzererlebnis noch eher ein starkes System fuer Entwickler und engagierte Betreiber als ein wirklich reibungsarmes Produkt fuer normale Nutzer.

Die größten UX-Hebel liegen aktuell nicht in „mehr Funktionen“, sondern in:

- klarerer Führung
- besseren Statusaussagen
- verständlicherem Fehlermanagement
- geführten Abläufen für Mapping, Start und Recovery

---

## Die größten UX-Lücken heute

### 1. Zu viel Technik, zu wenig Aufgabe

Die WebUI zeigt bereits viele Funktionen, aber sie ist noch stärker entlang von Modulen aufgebaut als entlang echter Nutzeraufgaben.

Ein Nutzer denkt typischerweise nicht:

- „Ich möchte jetzt den Op-State sehen“
- „Ich möchte jetzt Rohtelemetrie prüfen“

sondern eher:

- „Ist der Roboter bereit?“
- „Warum startet er nicht?“
- „Wie lege ich jetzt eine neue Karte an?“
- „Was muss ich als Nächstes tun?“

Die UX sollte deshalb stärker von Aufgaben und Betriebsphasen ausgehen.

### 2. Status ist vorhanden, aber noch nicht immer verständlich

Die Telemetrie ist inzwischen gut, aber nicht alles ist im Dashboard so verdichtet, dass ein Nutzer ohne Technikverständnis sofort weiss:

- was gerade passiert
- warum es passiert
- was als Nächstes zu erwarten ist
- ob Handlungsbedarf besteht

### 3. Fehler und Recovery sind noch zu technisch

`Error`, `GpsWait`, `WaitRain`, `Dock`, `Undock` und ähnliche Zustände sind intern sinnvoll, aber aus Nutzersicht fehlen oft:

- klare Bedeutung
- sichtbare Ursache
- konkrete Handlungsempfehlung

### 4. Mapping und Setup sind funktional, aber noch nicht gefuehrt genug

Gerade für erste Inbetriebnahme und Kartenaufnahme ist noch zu viel implizites Wissen notwendig.
Der Nutzer sollte stärker durch die nötigen Schritte geführt werden.

---

## Prioritaet 1: Gefuehrter Erststart

### Ziel

Ein neuer Nutzer soll vom ersten Einschalten bis zur ersten sicheren Mähfahrt ohne Rätsel geführt werden.

### Empfohlener Ablauf

Ein geführter Setup-Flow sollte diese Schritte abbilden:

1. Verbindung zum Roboter prüfen
2. GPS-/RTK-Status prüfen
3. Akku und Sensorstatus prüfen
4. neue Karte anlegen oder vorhandene Karte laden
5. Begrenzung aufnehmen oder Karte importieren
6. Docking-Punkt / Docking-Pfad prüfen
7. Testfahrt ohne Mähmotor
8. Startfreigabe

### Nutzen

- deutlich weniger Einstiegshürden
- weniger Fehlbedienung
- bessere erste Nutzererfahrung
- weniger implizites Wissen notwendig

---

## Prioritaet 2: Preflight-Check vor dem Start

### Ziel

Vor jedem Start sollte für den Nutzer sofort sichtbar sein, ob das System wirklich startbereit ist.

### Empfohlene Darstellung

Ein klarer Preflight-Block im Dashboard mit Ampel-/Checklistendarstellung:

- GPS/RTK bereit
- Akku ausreichend
- Karte geladen
- Startpunkt plausibel
- Dock bekannt
- keine aktive Fehlersperre
- API/Verbindung OK
- Sensoren plausibel

### Wichtige UX-Regel

Nicht nur „rot/grün“, sondern immer:

- Zustand
- Ursache
- Konsequenz
- empfohlene nächste Aktion

Beispiel:

- `GPS noch nicht ausreichend`
  `Der Roboter wartet auf RTK-Fix oder stabilen Float.`
- `Karte fehlt`
  `Bitte zuerst eine Karte laden oder neu anlegen.`

---

## Prioritaet 3: Verstaendlichere Statusphasen im Dashboard

### Ziel

Der Nutzer sollte nicht primär interne Op-Namen lesen müssen.

### Heute intern

- `Idle`
- `Undock`
- `NavToStart`
- `Mow`
- `Dock`
- `Charge`
- `WaitRain`
- `GpsWait`
- `EscapeReverse`
- `Error`

### Empfohlene Nutzertexte

- `Bereit`
- `Verlässt die Ladestation`
- `Fährt zum Startpunkt`
- `Mäht`
- `Fährt zur Ladestation`
- `Lädt`
- `Wartet wegen Regen`
- `Wartet auf GPS-Erholung`
- `Umfährt Hindernis`
- `Fehler – Eingriff nötig`

### Zusatz

Diese nutzerfreundlichen Phasen sollten ergänzt werden um:

- `seit wann`
- `warum`
- `was als Nächstes passiert`

---

## Prioritaet 4: Besseres Fehlermanagement

### Ziel

Fehler sollen nicht nur sichtbar, sondern verständlich und handhabbar sein.

### Aktuelle Lücke

Es gibt intern bereits `event_reason`, `error_code`, `state_phase` und Telemetrie.
Die UX nutzt diese Informationen aber noch nicht überall voll aus.

### Empfohlene Verbesserung

Jeder relevante Fehlerzustand sollte im UI drei Dinge zeigen:

1. **Was ist passiert?**
2. **Wie kritisch ist das?**
3. **Was soll der Nutzer jetzt tun?**

### Beispiel

Statt:

- `Error`

lieber:

- `Fehler: GPS konnte nicht rechtzeitig wiederhergestellt werden`
- `Der Roboter hat die Mission abgebrochen und ist gestoppt.`
- `Empfohlen: GPS-Qualität prüfen, freie Sicht zum Himmel herstellen, dann manuell neu starten.`

### Wichtige Fehlergruppen

- GPS/GNSS
- Akku
- Docking/Kontakt
- Lift / Sicherheit
- Motorfehler
- Karten-/Resume-Sperren

---

## Prioritaet 5: Gefuehrter Mapping-Workflow

### Ziel

Mapping soll sich wie ein klarer Arbeitsablauf anfühlen, nicht wie eine Sammlung einzelner Editorfunktionen.

### Sinnvoller Ablauf

1. Neue Karte anlegen
2. GPS-Qualität prüfen
3. Grenze schrittweise aufnehmen
4. Punkte direkt auf der Karte sehen
5. Qualität der aufgenommenen Punkte anzeigen
6. Karte prüfen
7. Docking-Pfad setzen
8. Testfahrt / Plausibilisierung
9. Speichern

### Wichtige UX-Verbesserungen

- Fortschrittsanzeige während Aufnahme
- deutliches Feedback pro aufgenommenem Punkt
- klare Hinweise bei `Fix`, `Float`, `Invalid`
- Validierung:
  - Perimeter geschlossen?
  - ausreichend Punkte?
  - Docking sinnvoll?
  - Startbar?

### Nachbearbeitung

Auch nach der Aufnahme sollte die Bearbeitung leicht bleiben:

- Punkt löschen
- Punkt auf Kante einfügen
- Marker und Metadaten sichtbar halten

---

## Prioritaet 6: Ereignis- und Missionshistorie

### Ziel

Nutzer wollen oft weniger Rohdaten und mehr eine verständliche Geschichte des letzten Einsatzes.

### Sinnvolle Fragen

- Warum hat der Roboter heute gestoppt?
- Wann ist er in den Dock gefahren?
- Gab es GPS-Probleme?
- Hat ein Hindernis die Mission unterbrochen?
- Wie lange wurde tatsächlich gemäht?

### Empfohlene Darstellung

Eine verständliche Session-Historie mit Ereignissen wie:

- Mission gestartet
- Startpunkt erreicht
- Mähbetrieb aktiv
- Hindernis erkannt
- GPS verloren
- Regen erkannt
- Docking gestartet
- Laden begonnen
- Mission beendet

### Nutzen

- höheres Vertrauen
- bessere Fehlersuche
- weniger „Black Box“-Gefühl

---

## Prioritaet 7: Weniger Entwicklerdenken in der Primäransicht

### Ziel

Das Dashboard sollte primär eine Betriebsoberfläche sein und erst sekundär eine Diagnosefläche.

### Empfehlung

Klare Trennung in zwei Ebenen:

#### Ebene 1: Nutzerbetrieb

- Start / Stop / Dock
- Bereitschaft
- Karte
- Status
- nächste empfohlene Aktion

#### Ebene 2: Diagnose

- Rohtelemetrie
- Debug-Felder
- technische Zustände
- erweiterte Sensorinformationen

So bleibt die technische Tiefe erhalten, ohne die Hauptbedienung zu überladen.

---

## Prioritaet 8: Mehr Vertrauen durch Sichtbarkeit

### Ziel

Der Nutzer soll das Gefühl haben, dass das System nachvollziehbar und kontrollierbar arbeitet.

### Besonders hilfreich

- sichtbarer Start-Check
- sichtbare Gründe für Nicht-Start
- sichtbare Gründe für Abbruch
- sichtbare Missionsphase
- sichtbare GPS-/RTK-Qualität
- sichtbare Docking-/Ladephase
- sichtbare letzte wichtige Ereignisse

Gerade bei autonomen Robotern ist Vertrauen ein zentrales UX-Thema.

---

## Konkrete naechste UX-Bausteine

Wenn nur wenige Dinge priorisiert werden sollen, wären diese am wertvollsten:

### UX-Block A — Startbereitschaft

- Preflight-Panel im Dashboard
- Ampel / Checklistendarstellung
- Start-Blocker mit Handlungsempfehlungen

### UX-Block B — Bessere Statussprache

- Nutzerfreundliche Phasen statt nur Op-Namen
- Status + Ursache + nächste Aktion

### UX-Block C — Fehlerführung

- einheitliche Fehlerkarten
- verständliche Recovery-Hinweise
- klarere Differenzierung zwischen Warnung, Pause und Fehler

### UX-Block D — Mapping-Assistent

- geführte Kartenerstellung
- RTK-Qualität prominent
- Validierung vor Freigabe

### UX-Block E — Ereignishistorie

- einfache Timeline
- Missionszusammenfassung
- Abbruchgründe

---

## Zusammenfassung

Das größte UX-Potenzial von `sunray-core` liegt derzeit nicht im Ausbau der tiefen Technik, sondern in ihrer besseren Vermittlung.

Technisch ist bereits viel Substanz vorhanden.
Die nächste Reifestufe entsteht dadurch, dass das System für den Nutzer:

- klarer
- geführter
- vertrauenswürdiger
- handlungsorientierter

wird.

Die Richtung ist deshalb:

**weniger Modul-Denken, mehr Nutzerfluss**

und

**weniger Rohzustand, mehr verständliche Betriebslogik**
