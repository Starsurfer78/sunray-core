# Mobile App Field Test Checklist

## Vor dem Test

- Android-App installieren:
  - `/mnt/LappiDaten/Projekte/sunray-core/mobile-app/build/app/outputs/flutter-apk/app-debug.apk`
- Pi und Handy ins gleiche WLAN bringen
- Sicherstellen, dass Avahi auf dem Pi läuft
- Prüfen, dass `sunray-core` auf dem Roboter läuft
- Testfläche mit bekannter Karte und mindestens einer Zone vorbereiten

## Discovery und Verbindung

- App öffnen
- Prüfen, dass Alfred per mDNS gefunden wird
- `Hinzufügen` / `Verbinden` testen
- App schließen und neu öffnen
- Auto-Reconnect prüfen
- WLAN kurz aus und wieder an:
  - Reconnect-Hinweis sichtbar?
  - Auto-Reconnect erfolgreich?

## Dashboard

- Statusleiste zeigt Verbindung, Akku und RTK-Status
- Mini-Karte zeigt Perimeter, Zonen, No-Go und Roboterposition
- `Start`, `Stop`, `Dock` senden Kommandos erfolgreich

## Karteneditor

- Perimeter als neues Objekt anlegen
- No-Go-Zone anlegen
- Zone anlegen
- Dockpunkte setzen
- Zwischen Objekten wechseln
- Im Edit-Modus:
  - Punkt auswählen
  - Punkt an neue Stelle setzen
  - Segment-Mitte antippen und Punkt einfügen
  - Punkt löschen
  - Objekt löschen
- Kartenverschiebung und Zoom prüfen

## Missionen

- Mission anlegen
- Zonen auswählen
- Mission umbenennen
- Mission starten
- Mission stoppen
- Sichtprüfung: selektierte Zonen werden auf der Karte hervorgehoben

## Fehlerfälle

- Pi während Verbindung neu starten
- Prüfen, ob verständliche Fehlermeldung erscheint
- Prüfen, ob die App nach Wiederverfügbarkeit reconnectet
- Mit falscher IP manuell verbinden:
  - sinnvolle Fehlermeldung?

## Vor Release

- echte Release-Signing-Datei in `mobile-app/android/key.properties` anlegen
- Release-Build erzeugen
- eindeutige App-ID beibehalten: `de.sunray.mobile`
- mindestens einen kompletten Mähdurchlauf mit App-Kommandos begleiten
