# Sunray WebUI

`webui-svelte` ist die Browser-Oberflaeche fuer `sunray-core`, gebaut mit Svelte 5, TypeScript und Vite.
Sie wird im Produktbetrieb vom C++-Server direkt ausgeliefert und ist typischerweise unter `http://alfred.local:8765` erreichbar.

## Kernfunktionen

- Dashboard mit Live-Telemetrie, Status, Missionsfortschritt und manueller Steuerung
- Kartenansicht und Editor fuer Perimeter, No-Go, Dock und Zonen
- Missionsseite mit echter Planner-Vorschau aus dem Backend
- Diagnose- und Service-Funktionen fuer Sensorik, OTA und Runtime-Zustand
- Verlauf mit Historie und Statistik
- responsives Layout fuer Desktop und Tablet

## Wichtige Architekturregel

Die WebUI enthaelt keine eigene Missionsplanung.
Vorschau, Runtime und Fortschritt basieren auf derselben vom Backend gelieferten kompilierten Route.

## Entwicklung

```bash
cd webui-svelte
npm install
npm run dev
```

Der Vite-Dev-Server laeuft standardmaessig auf `:5173` und proxyt lokal:

- `/api` nach `http://localhost:8765`
- `/ws` nach `ws://localhost:8765`

Fuer lokale Entwicklung kann parallel der Simulator laufen:

```bash
./build/sunray-core --sim config.example.json
```

## Build

```bash
cd webui-svelte
npm run build
```

Der Produktions-Build landet in `webui-svelte/dist/` und wird von `sunray-core` als statische WebUI ausgeliefert.

## Qualitaetssicherung

```bash
cd webui-svelte
npm run check
```

## Projektstruktur

```text
src/
├── lib/api/           REST- und WebSocket-Anbindung
├── lib/components/    Dashboard-, Map- und Diagnose-Komponenten
├── lib/stores/        globaler UI- und Telemetrie-Zustand
├── lib/utils/         kleine Frontend-Hilfen
└── routes bzw. Shell  Seitenstruktur und App-Rahmen
```

## Betriebskontext

- die WebUI ist die vollwertige Browser-Oberflaeche fuer den Roboter
- sie laeuft lokal im Heimnetz ohne Cloud-Zwang
- fuer mobile Feldtests ist `mobile-app-v2` die parallele native Bedienoberflaeche
