# Alfred WebUI

Browser-Oberfläche für sunray-core auf Basis von **Svelte 5 + TypeScript + Vite**.
Läuft direkt auf dem Raspberry Pi — einfach `http://alfred.local:8765` im Browser öffnen.

## Seiten

| Seite | Inhalt |
|---|---|
| **Dashboard** | Live-Telemetrie, Roboterstatus, Steuerung, aktive Mission, Log-Leiste |
| **Karte** | Interaktiver Editor: Perimeter, Zonen, No-Go-Zonen, Dock-Pfad |
| **Missionen** | Mähzonen auswählen, Mährouten-Vorschau, Start/Stop |
| **Diagnose** | IMU, Motortest, Tick-Kalibrierung, Richtungsvalidierung, Sensorwerte |
| **Verlauf** | Ereignishistorie und Mähstatistik |
| **Einstellungen** | Konfiguration und OTA-Update |

## Karteneditor

- Perimeter, Mähzonen, No-Go-Zonen und Dock-Pfad direkt auf OpenStreetMap zeichnen
- Punkte per Klick setzen, verschieben, einfügen und löschen
- Sofortige Mährouten-Vorschau nach Zonenauswahl

## Diagnose

- **IMU-Panel** — Lage, Kalibrierungsstatus
- **Motortest** — Antriebs- und Mähmotoren einzeln testen
- **Tick-Kalibrierung** — Odometriekorrektur
- **Richtungsvalidierung** — Vorwärts/Rückwärts-Verifikation
- **Sensorpanel** — Perimeter-Signal, Stoßsensor, Regensensor, Ladekontakt

## Entwicklung

```bash
npm install
npm run dev      # Vite-Dev-Server auf :5173
```

Vite proxyt lokal automatisch:
- `/ws`  →  `ws://localhost:8765`
- `/api` →  `http://localhost:8765`

Für lokale Entwicklung einfach den sunray-core Simulator parallel starten:

```bash
./build/sunray-core --sim config.example.json
```

## Build & Deploy

```bash
npm run build    # → dist/
```

Der fertige Build liegt in `webui-svelte/dist/` und wird vom sunray-core C++-Server
direkt als statische Dateien ausgeliefert.

Das Installations-Skript baut die WebUI automatisch mit:

```bash
bash scripts/install_sunray.sh
```

## Architektur

```
src/
├── lib/
│   ├── api/          WebSocket- und REST-Client
│   ├── stores/       Globaler Zustand (Telemetrie, Verbindung, Karte)
│   ├── components/   Wiederverwendbare UI-Komponenten
│   └── pages/        Hauptseiten
└── App.svelte        Routing und Shell
```

## Typ-Check

```bash
npm run check
```
