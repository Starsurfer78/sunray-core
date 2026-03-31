# webui-svelte

Neustart der Alfred-WebUI auf Basis von `Svelte + Vite + TypeScript`.

Dies ist die aktive Frontend-Basis des Projekts.

Die fruehere Vue-WebUI wurde als Referenz nach
`ALTE_DATEIEN/webui-vue-reference/` verschoben.

## Ziel

Das neue Frontend soll klein, robust und websocket-faehig sein. Der MVP deckt
zuerst nur diese Bereiche ab:

- Verbindung und Live-Telemetrie
- Dashboard
- Basis-Steuerung
- Diagnose
- Motor-Kalibrierung
- einfache Karte mit Gitter

## Entwicklung

```bash
npm install
npm run dev
```

Vite proxyt lokal:

- `/ws` -> `ws://localhost:8765`
- `/api` -> `http://localhost:8765`

## Build

```bash
npm run build
```

Der Build landet in:

`webui-svelte/dist`

## Deploy

Das Frontend sollte als fertige statische `dist/`-App deployed werden. Nicht
auf dem Pi entwickeln, wenn es sich vermeiden laesst.

## Architektur

- `src/lib/api/` fuer WS- und REST-Zugriff
- `src/lib/stores/` fuer globalen Zustand
- `src/lib/components/` fuer wiederverwendbare UI-Bloecke
- `src/lib/pages/` fuer Seiten
