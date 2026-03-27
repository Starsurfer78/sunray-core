# webui — Vue 3 Frontend

**Status:** ✅ Complete
**Stack:** Vue 3 + Vite + Tailwind | Design: Dark Blue (locked to `webui/design/dashboard_reference.html`)

---

## MapEditor.vue
**Status:** ✅ Complete (C.2/C.3 + C.4b zones + C.4c Mähbahnen)
**Tools:** select, perimeter, exclusion, zone, dockPath, obstacle, delete
**Mähbahnen:** angle, stripWidth, overlap, edgeMowing, turnType (K-Turn/Zero-Turn), startSide → computeMowPath() → cyan/orange preview
**Map format:** perimeter/mow/dock/dockPath/exclusions/obstacles/zones/origin

## useMowPath.ts
**Status:** ✅ Complete (C.4c)
**Exports:** `MowPt`, `MowPathSettings`, `DEFAULT_SETTINGS`, `computeMowPath()`, `inwardOffsetPolygon()`
**Algorithm:** headland rings → strip scanlines (exclusion clipping, boustrophedon) → K-Turn/Zero-Turn transitions
**Tests:** `useMowPath.test.ts` — 6 Vitest tests

## Settings.vue
**Status:** ✅ Complete
**Sections:** System info | Robot config (6 keys via GET/PUT /api/config) | Diagnostics (motor test, IMU calib, log stream)

## useTelemetry.ts
**Status:** ✅ Complete
**Exports:** telemetry state, `logs: readonly string[]` (max 200), `clearLogs()`
**Source:** WebSocket `ws://host/ws/telemetry` — parses type:"log" messages for log stream
