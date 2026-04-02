# webui-svelte

Neustart der Alfred-WebUI auf Basis von `Svelte + Vite + TypeScript`.

Dies ist die aktive Frontend-Basis des Projekts.

## Ziel

Das Frontend soll robust, feldtauglich und websocket-faehig sein. Aktuell
deckt es unter anderem diese Bereiche ab:

- Verbindung und Live-Telemetrie
- Dashboard
- Missionssteuerung
- Diagnose
- Karteneditor
- Verlauf und Statistik
- operatornahe Fehler- und Statushinweise

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

Das Frontend wird als fertige statische `dist/`-App vom `sunray-core`-Backend
ausgeliefert.

Fuer Alfred ist der normale Weg:

```bash
cd ~/sunray-core/webui-svelte
npm install
npm run build
```

Alternativ baut `bash scripts/install_sunray.sh` die WebUI automatisch mit.

Nicht auf dem Pi entwickeln, wenn es sich vermeiden laesst.

## Architektur

- `src/lib/api/` fuer WS- und REST-Zugriff
- `src/lib/stores/` fuer globalen Zustand
- `src/lib/components/` fuer wiederverwendbare UI-Bloecke
- `src/lib/pages/` fuer Seiten

Aktuelle Hauptseiten:

- `Dashboard.svelte`
- `Map.svelte`
- `Mission.svelte`
- `Diagnostics.svelte`
- `History.svelte`
