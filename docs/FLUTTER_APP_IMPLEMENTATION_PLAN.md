# Flutter App Implementation Plan

## Goal

Dieses Dokument definiert die Zielarchitektur und den Umsetzungsplan fuer eine neue mobile App in einem separaten Repo-Unterordner, voraussichtlich `mobile-app/`.

Die App soll:

- als echte Android-App gebaut werden
- spaeter auch iOS/Web/Desktop ermoeglichen
- Roboter automatisch im Netzwerk finden
- Karten, Missionen und Live-Betrieb sauber trennen
- die Bahnplanung immer serverseitig aus `sunray-core` nutzen
- OTA-Updatefaehigkeit fuer App und Roboter beruecksichtigen

## Product Principles

- Genau `4` Haupttabs: `Dashboard`, `Karte`, `Missionen`, `Service`
- Kein Wizard, keine doppelte Navigation, kein ueberladenes Chrome
- Karte ist fast immer die primaere Flaeche
- `Missionen` erstellt keine Geometrie
- `Karte` erstellt und bearbeitet Geometrie
- Vorschau zeigt nur den echten Backend-Plan
- Live-Betrieb springt immer uebers Dashboard an

## Target UX

### Dashboard

- Idle:
  - Statusbar
  - Mini-Karte
  - Robot-Status
  - naechste Mission
  - letzter Fehler
  - Schnellaktionen
- Waehrend Mission:
  - Live-Karte
  - Fortschritt
  - Flaeche
  - Restzeit
  - Akku
  - `Pause`, `Stop`, `Dock`

### Karte

Die Karte hat `3` klare Modi:

1. `Ansicht`
   - Layer Toggle
   - `+` fuer neue Objekte
   - Roboter zentrieren
   - Kompass
2. `Aufzeichnen`
   - grosse `+ Punkt speichern` Aktion
   - `Schliessen`
   - `Abbruch`
3. `Bearbeiten`
   - nur aktives Objekt editierbar
   - Punkt antippen = loeschen
   - Segment antippen = Punkt einfuegen
   - Midpoints sichtbar

### Missionen

- Karte bleibt sichtbar
- Formular kommt als Bottom Sheet
- Zonen werden auf der Karte ausgewaehlt
- Mission speichert:
  - Name
  - Zone/Zonen
  - Zeitplan
  - Bedingungen
  - Muster

### Service

- Verbindung
- Diagnose
- Logs
- Firmware-/Software-Updates
- Netzwerkdetails

## Proposed Project Location

Empfohlen:

```text
sunray-core/
  mobile-app/
```

Die App lebt bewusst getrennt von `webui-svelte`, nutzt aber dieselben Backend-Endpunkte und denselben Planner.

## Recommended Stack

- Framework: `Flutter`
- State: `Riverpod`
- Routing: `go_router`
- Map: `flutter_map`
- Discovery: `multicast_dns`
- Local cache: `drift`
- WebSocket: `web_socket_channel`
- HTTP API: `dio` oder `http`

## Build Output

Aus derselben Codebasis entstehen:

- Android APK via `flutter build apk`
- Android App Bundle via `flutter build appbundle`
- spaeter iOS, Web, Desktop

## Folder Structure

```text
mobile-app/
  pubspec.yaml
  lib/
    app/
      app.dart
      router.dart
      theme.dart
    core/
      env/
      error/
      utils/
    data/
      discovery/
        discovery_service.dart
        discovery_repository.dart
      robot/
        robot_api.dart
        robot_ws.dart
        robot_repository.dart
      maps/
        map_api.dart
        map_repository.dart
      missions/
        mission_api.dart
        mission_repository.dart
      updates/
        ota_repository.dart
      local/
        app_database.dart
        tables/
        daos/
    domain/
      robot/
        discovered_robot.dart
        saved_robot.dart
        robot_status.dart
      map/
        map_geometry.dart
        mow_zone.dart
        no_go_zone.dart
        coverage_plan.dart
      mission/
        mission.dart
        mission_schedule.dart
        mission_condition.dart
    features/
      discovery/
      dashboard/
      map_editor/
      missions/
      service/
    shared/
      widgets/
      providers/
      models/
  android/
  ios/
  web/
```

## Core Domain Separation

### 1. Robot

Verantwortlich fuer:

- Verbindungsdaten
- Live-Status
- Batterie
- RTK/GPS
- Fehlerzustand

### 2. MapGeometry

Verantwortlich fuer:

- Perimeter
- No-Go-Zonen
- Dock
- Mähzonen

### 3. Mission

Verantwortlich fuer:

- Zonen-Auswahl
- Zeitplan
- Bedingungen
- Muster

### 4. CoveragePlan

Verantwortlich fuer:

- fertige serverseitige Fahrsegmente
- reine Anzeige/Ausfuehrung

Wichtig:

- Die App berechnet keine eigene Bahnlogik.
- Die App rendert nur das Ergebnis aus `sunray-core`.

## Main Screens

### DiscoveryScreen

Zweck:

- Roboter im Netzwerk finden
- Roboter hinzufuegen
- Fallback auf manuelle IP

### DashboardScreen

Zweck:

- Hauptstartseite
- Betriebsstatus
- Fortschritt
- Schnellaktionen
- Checkliste fuer erste Mission

### MapEditorScreen

Zweck:

- Objekte erstellen
- Objekte aufnehmen
- Objekte bearbeiten

### MissionsScreen

Zweck:

- Missionen verwalten
- Zonen auswaehlen
- Preview anfordern
- Mission speichern/starten

### ServiceScreen

Zweck:

- Diagnose
- OTA
- Logs
- Verbindungsstatus

## Routing

Es gibt exakt diese Hauptpfade:

```text
/discover
/dashboard
/map
/missions
/service
```

Empfohlene Guards:

- kein aktiver Roboter -> `/discover`
- Roboter verbunden -> `/dashboard`

## Riverpod Providers

```dart
final discoveredRobotsProvider;
final savedRobotsProvider;
final activeRobotProvider;
final connectionStateProvider;
final telemetryProvider;

final mapGeometryProvider;
final mapEditorModeProvider;
final mapEditorSelectionProvider;
final mapLayerVisibilityProvider;

final missionsProvider;
final selectedMissionProvider;
final plannerPreviewProvider;

final otaStateProvider;
```

## Discovery via mDNS

### Goal

Die App soll Roboter automatisch im WLAN finden, ohne manuelle IP-Eingabe.

### Pi Side

Empfohlene Avahi-Datei:

`/etc/avahi/services/sunray.service`

```xml
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name replace-wildcards="yes">Alfred (%h)</name>
  <service>
    <type>_sunray._tcp</type>
    <port>8080</port>
    <txt-record>version=1</txt-record>
    <txt-record>model=sunray-core</txt-record>
  </service>
</service-group>
```

### App Side

Package:

- `multicast_dns`

Aufgaben:

- PTR -> SRV -> A Lookup
- Name, IP, Port einsammeln
- Funde deduplizieren
- bekannte Roboter gegen neue IP abgleichen

## Local Persistence

Mit `drift` lokal speichern:

- bekannte Roboter
- letzter aktiver Roboter
- letzte IP/Port
- lastSeen
- lokale UI-Praeferenzen
- optionale Drafts

Nicht lokal als Wahrheit speichern:

- CoveragePlan
- Live-Telemetrie

## API Integration with sunray-core

Die App soll dieselben Backend-Flaechen verwenden wie die WebUI:

- `GET /api/map`
- `POST /api/map`
- `GET /api/missions`
- `POST /api/missions`
- `PUT /api/missions/:id`
- `POST /api/planner/preview`
- WebSocket fuer Live-Status/Commands

## Planner Preview Rule

Die wichtigste Architekturregel:

- Keine Plannerlogik in Dart
- Keine lokale Stripe-/Offset-Berechnung
- Keine approximierte Vorschau

Stattdessen:

1. App sendet Mission + Map Snapshot an Backend
2. Backend erzeugt finalen `CoveragePlan`
3. App rendert genau diesen Plan

## Map Editor Implementation Rules

### View Mode

- kein Editieren
- nur Layer und Kamera

### Record Mode

- ein aktives Zielobjekt
- grosser Primarbutton
- neue Punkte ueber GPS/Robot oder Tap
- Polygon bleibt offen bis `Schliessen`

### Edit Mode

- nur aktives Objekt zeigt Handles
- nur aktives Objekt ist verschiebbar
- Segment-Tap fuegt Punkt ein
- Punkt-Tap loescht

## Dashboard Checklist Flow

Kein Wizard. Stattdessen ein Checklist-Block:

1. Roboter verbunden?
2. Perimeter vorhanden?
3. Dock gesetzt?
4. Mission vorhanden?

Jeder Punkt hat direkten Sprung auf den passenden Screen.

## OTA Requirements

Die App soll OTA-faehig sein in zwei getrennten Richtungen:

### A. App OTA

Ziel:

- neue Android-Versionen an Benutzer verteilen

Empfohlene Wege:

1. Entwicklungs-/Beta-Phase:
   - GitHub Releases oder self-hosted APK Download
2. spaeter:
   - Play Store Interne Tests / Closed Track

Wichtige Punkte:

- App-Version in `Service` anzeigen
- Update-Check gegen Release-Metadaten
- Download und Intent zum Installieren
- Hinweis auf unbekannte Quellen bei self-hosted APK

### B. Roboter OTA

Ziel:

- `sunray-core` auf dem Pi und optional STM-Firmware aus der App anstossen

Empfohlene Aufteilung:

1. Pi-Software-Update
   - App ruft vorhandenen OTA-Endpunkt von `sunray-core` auf
   - Update-Status und Neustart rueckmelden
2. STM-Firmware-Update
   - App nutzt dieselben Backend-Endpunkte wie die WebUI
   - Upload einer `.bin`
   - Flash nur in sicherem Zustand
   - Fortschritt und Maintenance-Window anzeigen

### OTA Safety Rules

- Updates nur bei `Idle` oder `Charge`
- Vor Start Sicherheitspruefung:
  - kein Mähvorgang aktiv
  - Akku ausreichend
  - Verbindung stabil
- Nach Update:
  - reconnect workflow
  - Version erneut pruefen

## Service Screen OTA Sections

Empfohlene Service-Sektionen:

1. `App`
   - Version
   - Update verfuegbar
   - APK installieren
2. `Pi / sunray-core`
   - aktuelle Version
   - auf Updates pruefen
   - Update starten
3. `STM`
   - erkannte Firmware-Version
   - `.bin` waehlen
   - hochladen
   - flashen

## Implementation Phases

### Phase 1 - Scaffold

- Flutter-Projekt unter `mobile-app/`
- `Riverpod`
- `go_router`
- Grundtheme
- 4 Tabs
- Discovery Startscreen

### Phase 2 - Connection and Discovery

- mDNS Discovery
- gespeicherte Roboter
- connect/reconnect
- manueller Host-Fallback

### Phase 3 - Dashboard

- Idle Dashboard
- Running Dashboard
- checklist block
- Schnellaktionen

### Phase 4 - Map Editor

- Karte Vollbild
- View / Record / Edit
- Create Menu
- Objekt-Selektion
- Segment-Einfuegen
- Edit-Lock

### Phase 5 - Missions

- Bottom Sheet UI
- Zonen-Auswahl
- Zeitplan
- Bedingungen
- Planner Preview aus Backend

### Phase 6 - Service

- Diagnose
- Logs
- OTA fuer App/Pi/STM

### Phase 7 - Polish

- Animationen
- Offline-Verhalten
- Fehleranzeigen
- Responsive Anpassungen

## Critical Risks

1. Zu viel Kartenlogik direkt im Widget
2. lokale Vorschau weicht vom Backend ab
3. Discovery scheitert in isolierten Netzwerken
4. OTA ohne klare Sicherheitsgrenzen
5. zu viele Panels auf kleinen Displays

## First Concrete Build Order

Empfohlen fuer die echte Umsetzung:

1. `mobile-app/` Scaffold anlegen
2. Discovery + Connect bauen
3. Dashboard bauen
4. Map Editor Canvas bauen
5. Missions Bottom Sheet bauen
6. OTA-/Service Screen bauen

## Definition of Done for V1

V1 ist erreicht, wenn:

- die App Alfred im WLAN automatisch findet
- ein Roboter gespeichert und erneut verbunden werden kann
- Dashboard Idle und Running funktioniert
- Perimeter, No-Go, Zonen und Dock in der Karte bearbeitet werden koennen
- Missionen aus Zonen erstellt werden koennen
- Preview vom Backend kommt
- Pi- und STM-Update ueber die App anstossbar sind
- eine Android APK gebaut werden kann

---

## Visual Design

### Leitprinzipien

- Dark-First, niemals helles Theme als Default
- Karte ist immer das primaere UI-Element
- Wenige Farben, klare Hierarchie
- Keine Karten-Styles die mit dem Karten-Inhalt konkurrieren (kein buntes Tile-Overlay)
- Kompakte Typografie fuer Statusinformationen, grosse Targets fuer Aktionen

### Farbpalette

```
Background deep:   #060c17
Background card:   #0f1829
Background input:  #08101d
Border default:    #1e3a5f
Border subtle:     #1a2a40

Accent blue:       #2563eb
Accent blue light: #60a5fa
Accent cyan:       #22d3ee

Text primary:      #e2e8f0
Text secondary:    #94a3b8
Text muted:        #475569

Status OK:         #4ade80
Status warn:       #fbbf24
Status error:      #f87171
Status running:    #60a5fa
```

### Bottom Navigation

4 feste Tabs, immer sichtbar:

```
[ Dashboard ]  [ Karte ]  [ Missionen ]  [ Service ]
```

- Icons + Label, keine reinen Icons
- aktiver Tab: Accent Blue Underline
- keine Badges ausser Fehlerindikator im Service-Tab

### Typografie

- Body/Label: System-Font, `sp 13-14`
- Status gross: `sp 18-22`, `FontWeight.w500`
- Karteninhalte: `sp 10-11`, Monospace fuer Koordinaten
- Keine Serif-Fonts

### Elevation / Cards

- Cards: `border: 1px solid #1e3a5f`, kein Schatten
- Kein Material-Elevation-System, stattdessen Border + Background-Schichtung
- Bottom Sheets: `background #0f1829`, Griff-Bar oben, kein Dimming der Karte wenn moeglich

---

## Discovery UX Detail

### Automatischer Fund ("Hinzufuegen"-Banner)

Der haeufigste Flow ist:

1. App startet, kein bekannter Roboter
2. mDNS findet `_sunray._tcp` im Netz
3. Banner erscheint sofort im Discovery Screen: **"Alfred gefunden — Hinzufuegen?"**
4. Ein Tap -> gespeichert, direkt zu Dashboard

Wenn bereits ein bekannter Roboter unter neuer IP erscheint:

- Kein neuer Eintrag, bestehender Roboter wird aktualisiert
- Stille Reconnection, kein Banner

### Manueller Fallback

- In Discovery Screen: `Manuell verbinden` -> IP + Port eingeben
- Wird als normaler Roboter gespeichert
- Erscheint auch in der Liste wenn kein mDNS verfuegbar

### Discovery Screen Layout

```
┌────────────────────────────────┐
│  Mein Roboter                  │  <- Seitenheader
│                                │
│  ╔══════════════════════════╗  │
│  ║  Alfred                  ║  │  <- gefundener Roboter
│  ║  192.168.1.42 · RTK-Fix  ║  │
│  ║  [  Verbinden  ]         ║  │
│  ╚══════════════════════════╝  │
│                                │
│  Suche laeuft ...              │  <- Spinner
│                                │
│  ─── Manuell verbinden ─────   │
│  IP: [___________] Port [____] │
│  [  Verbinden  ]               │
└────────────────────────────────┘
```

---

## Perimeter Recording UX Detail

### Hintergrund

Segway Navimow, Mammotion Luba und aehnliche Systeme nutzen:

- Bluetooth fuer erstes Pairing
- WLAN fuer normalen Betrieb
- Kontinuierliches Fahren entlang der Grenze (Aufzeichnungsfahrt)

Dieses System nutzt:

- WLAN direkt (kein Bluetooth)
- **Punkt-fuer-Punkt-Aufnahme**: Roboter wird an die Grenze gestellt, Punkt wird manuell gespeichert
- Ecken und Kurven werden bewusst durch Setzen mehrerer Punkte abgebildet
- Vorteil: prazise Ecken ohne Rueckwirkung auf Fahrt, funktioniert ohne Fahren

### Record Mode UX

```
┌─────────────────────────────────────┐
│  [Schliessen]  Perimeter aufnehmen  │  <- Modus-Header
│                                     │
│  ┌─────────────────────────────┐    │
│  │                             │    │
│  │       flutter_map           │    │
│  │       Roboter-Dot blau      │    │
│  │       Bisheriges Polygon    │    │
│  │                             │    │
│  └─────────────────────────────┘    │
│                                     │
│  ╔═════════════════════════════╗    │
│  ║   +  Punkt speichern        ║    │  <- Primaer-Aktion, gross
│  ╚═════════════════════════════╝    │
│                                     │
│  [Schliessen]         [Abbrechen]   │
└─────────────────────────────────────┘
```

- `+ Punkt speichern` holt aktuelle Roboter-RTK-Position aus Telemetrie
- Roboter-Dot und aktuelles Polygon werden live aktualisiert
- `Schliessen` schliesst das Polygon (letzter Punkt = erster Punkt)
- `Abbrechen` verwirft ohne Speichern
- Mindestpunktanzahl fuer `Schliessen`: `3`

### Punkt-Feedback

- Jeder neue Punkt: kurze Vibration + visueller Punkt-Ping auf der Karte
- Koordinaten des letzten Punktes klein unter dem Primaerbutton anzeigen
- Punktanzahl-Zaehler sichtbar

---

## Map Editor - Kartenobjekt-Typen

Folgende Objekte werden im Editor erstellt und verwaltet:

| Typ | Form | Erlaubt | Besonderheit |
|---|---|---|---|
| Perimeter | Polygon geschlossen | 1 | darf nicht mit No-Go ueberlappen |
| No-Go-Zone | Polygon geschlossen | n | muss innerhalb Perimeter liegen |
| Mähzone | Polygon geschlossen | n | wird gegen befahrbare Flaeche geclippt |
| Dock | Punkt oder Linie | 1 | Anfahrtspfad optional |
| Korridor | Polygon offen | 1 | optionaler Dock-Korridor |

### Create Menu

Beim Tap auf `+` in View Mode erscheint ein kompaktes Action Sheet:

```
[ Perimeter aufnehmen ]
[ No-Go-Zone aufnehmen ]
[ Mähzone aufnehmen ]
[ Dock setzen ]
```

---

## Dashboard Screen Layout

### Idle

```
┌────────────────────────────────┐
│  Alfred · RTK-Fix · 95%        │  <- Status-Strip
├────────────────────────────────┤
│                                │
│   flutter_map Mini-Karte       │  <- ~40% der Hoehe
│   Roboter-Position sichtbar    │
│                                │
├────────────────────────────────┤
│  Checkliste                    │
│  ✓ Verbunden                   │
│  ✓ Perimeter vorhanden         │
│  ✓ Dock gesetzt                │
│  ✗ Keine Mission → [Erstellen] │
├────────────────────────────────┤
│  Naechste Mission              │
│  Rasenmaeher · Mo 07:00        │
│  [  Mission starten  ]         │
└────────────────────────────────┘
```

### Running

```
┌────────────────────────────────┐
│  Alfred · Mäht · Zone 2/3      │  <- Status-Strip, blau pulsierend
├────────────────────────────────┤
│                                │
│   flutter_map Live-Karte       │  <- ~55% der Hoehe
│   Fahrspur wird eingezeichnet  │
│   Roboter-Dot mit Heading      │
│                                │
├────────────────────────────────┤
│  ████████████░░░░  66%         │  <- Fortschrittsbalken
│  Zone: Hauptrasen              │
│  Flaeche: 340 m² / 510 m²     │
│  Akku: 72%                     │
├────────────────────────────────┤
│  [ Pause ]  [ Stop ]  [ Dock ] │
└────────────────────────────────┘
```

---

## Missions Screen Layout

Die Karte bleibt sichtbar. Mission-Details kommen als Bottom Sheet.

```
┌────────────────────────────────┐
│   flutter_map Vollbild         │
│   Zonen hervorgehoben          │
│                                │
│   [+ Mission]                  │  <- FAB oben rechts
└────────────────────────────────┘
     ↑
┌────────────────────────────────┐  <- Bottom Sheet
│  ▬▬▬  (Griff)                  │
│  Mission: Rasenmaeher          │
│  Zonen: [Vorgarten] [Garten]   │
│  Zeitplan: Mo Mi Fr 07:00      │
│  Muster: Streifen 45°          │
│                                │
│  [Vorschau]    [Speichern]     │
│  [Starten]                     │
└────────────────────────────────┘
```

Zone-Auswahl: Tap auf Zone in der Karte togglet sie in die Mission.

---

## Competitor-Informed Design Decisions

### Was Luba 2 / Mammotion gut macht

- Discovery + First Setup ueber BLE, dann WLAN-Handover
- Karte ist immer die Hauptflaeche
- Modus-wechsel klar durch Bottom Bar mit Icons
- Perimeter-Aufnahme als geführter Wizard-Flow mit klarem Schritt-fuer-Schritt

### Was dieses System anders macht (und warum)

| Entscheidung | Begruendung |
|---|---|
| Kein Bluetooth, nur WLAN | Pi hat kein BT-Pairing-System, WLAN direkt einfacher |
| Punkt-fuer-Punkt statt Fahraufnahme | Prazise Ecken, kein Schieben des Roboters noetig |
| Kein Wizard fuer Ersteinrichtung | Checklist-Block ist flexibler, kein Zwang zur Reihenfolge |
| Backend-only Preview | Verhindert exakt die Preview/Roboter-Divergenz-Klasse von Fehlern |
| 4 feste Tabs | Keine verschachtelten Navigations-Ebenen, alles sofort erreichbar |

---

## Map Layer System

Die Karte zeigt verschiedene Layer, die einzeln togglebar sind:

| Layer | Standard | Beschreibung |
|---|---|---|
| Satellite / OSM | an | Hintergrundkarte |
| Perimeter | an | Aeussere Mähgrenze |
| No-Go-Zonen | an | Gesperrte Bereiche |
| Mähzonen | an | Definierte Zonen |
| Dock + Korridor | an | Dockposition |
| Roboter | an | Live-Position + Heading |
| Coverage Preview | an wenn aktiv | Fahrstrecken aus Planner |
| Bereits gemaeht | aus | Tracking-Spur (spaeter) |

Layer-Toggle: kompaktes Icon-Panel am Kartenrand, kein separater Screen.

