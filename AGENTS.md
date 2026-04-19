# AGENTS.md

Use `CLAUDE.md` as the global rule set.
This file defines agent profiles to ensure context is loaded efficiently and domain boundaries are respected.

## Default Workflow
1. Read `TASK.md` and `SYSTEM_OVERVIEW.md` for context.
2. Choose **one** primary `ctx-profile`.
3. Load only relevant files. Expand scope only if necessary.
4. Validate changes locally.
5. Update `TASK.md` and use `manage_core_memory` when architecture or milestones change.

---

## 1. ctx-profile: C++ Backend (Navigation & Safety)
**Use when:** Modifying mission planning (A*), runtime ops (`Undock`, `NavToStart`), GPS/RTK logic, hardware HAL, or safety watchdogs.
**Load first:**
- `core/navigation/` or `core/op/`
- `hal/` or `platform/`
- `core/Robot.cpp` and `core/WebSocketServer.cpp`
**Watch for:**
- Never create a "second route" truth (WebUI is just a renderer).
- Ensure safe state transitions. Avoid blocking the control-loop.
- Validate with `ctest` or compiling `build_linux`.

## 2. ctx-profile: WebUI (Svelte)
**Use when:** Adjusting dashboard layout, telemetry rendering, map visualization, or SvelteKit configs.
**Load first:**
- `webui-svelte/src/lib/`
- `core/WebSocketServer.cpp` (if API contract changes)
**Watch for:**
- WebUI must NOT reimplement backend planning.
- Do not smooth over backend faults (surface them instead).
- Ensure responsive design (desktop vs. tablet).

## 3. ctx-profile: Mobile App (Flutter)
**Use when:** Working on `mobile-app-v2/` (Discovery, Map Editor, App-OTA, UI).
**Load first:**
- `mobile-app-v2/lib/`
- Relevant API contracts (`core/WebSocketServer.cpp` or `rest.ts`)
**Watch for:**
- App/Backend contract drift.
- Must run `flutter analyze` and leave zero warnings/errors.
- No dummy/TODO widgets. Complete the implementation.

## 4. ctx-profile: DevOps & Connectivity
**Use when:** Touching MQTT, OTA scripts, deployment flows, or Home Assistant integration.
**Load first:**
- `scripts/` (e.g. `ota_update.sh`, `install_sunray.sh`)
- `core/MqttClient.cpp`
**Watch for:**
- Reconnect edge cases (MQTT 30min disconnects).
- Verify bash script syntax (`#!/usr/bin/env bash`).
- Ensure graceful restarts.

---
## Completion Criteria
- Requested scope is fully solved and validated.
- Risks/assumptions are explicitly stated.
- If system knowledge evolved, `TASK.md` is updated.