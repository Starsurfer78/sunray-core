# AGENTS.md

Use [CLAUDE.md](/mnt/LappiDaten/Projekte/sunray-core/CLAUDE.md) as the canonical global rule set.

## Purpose

This file defines the preferred working pattern for agents in sunray-core so they load only the context needed for the current task and avoid mixing safety, planning, and UI assumptions.

## Default Workflow

1. Read TASK.md.
2. Choose exactly one primary ctx-profile.
3. Load only the files relevant to that profile first.
4. Expand scope only if the code proves the change crosses boundaries.
5. Validate the smallest meaningful scope before broader checks.
6. Update TASK.md or repo memory only when project knowledge actually changed.

## ctx-profile: navigation-planning

Use when the task involves mission compilation, preview generation, runtime route reuse, or validation.

Load first:
- core/navigation/
- tests covering route planning and validation
- TASK.md
- .memory/architecture-decisions.md

Watch for:
- second route truth
- silent route regeneration
- preview/runtime mismatch
- invalid transitions hidden by fallback paths

## ctx-profile: runtime-safety

Use when the task touches robot state flow, motion gating, stop behavior, watchdogs, docking, or recovery.

Load first:
- core/
- platform/
- hal/ for the affected hardware path
- SAFETY_MAP.md
- .memory/safety-findings.md

Watch for:
- control-loop thread ownership
- unsafe default behavior
- regression in stop, reset, or recovery semantics

## ctx-profile: gps-rtk

Use when the task involves GPS parsing, RTK quality, telemetry exposure, receiver config, or dashboard RTK diagnostics.

Load first:
- hal/GpsDriver/
- core/Robot.cpp
- core/WebSocketServer.*
- webui-svelte telemetry consumers if UI is affected
- .memory/rtk-context.md

Watch for:
- claimed metrics not actually backed by telemetry
- rover vs base-station confusion
- degraded RTK state being displayed as healthy

## ctx-profile: webui

Use when the task is primarily Svelte UI, dashboard layout, mission preview rendering, or operator feedback.

Load first:
- webui-svelte/src/
- related API types/stores
- TASK.md if mission or dashboard semantics are involved
- .memory/discovery-summary.md when available

Watch for:
- frontend reimplementing backend planning logic
- UI smoothing over backend faults instead of surfacing them
- desktop and tablet regressions in mission-critical views

## ctx-profile: mobile-app

Use when the task affects Flutter app flows, discovery, mission operations, OTA, or app-side operator controls.

Load first:
- mobile-app/
- relevant API contracts in backend/frontend
- .memory/mqtt-topics.md if connectivity is involved

Watch for:
- app/backend contract drift
- command semantics diverging from WebUI
- missing reconnect or update-state handling

## ctx-profile: connectivity-ota

Use when the task involves MQTT, Home Assistant, OTA, mDNS, restart/update flows, or service integration.

Load first:
- core/ and the affected API handlers/services
- scripts/ or deployment files involved
- .memory/mqtt-topics.md
- .memory/known-issues.md

Watch for:
- reconnect edge cases
- duplicated update logic between surfaces
- restart flows that report success too early

## Done Means

- requested scope is solved or explicitly blocked
- relevant checks passed, or the missing validation is stated clearly
- risks, assumptions, and unknowns are called out
- TASK.md, SAFETY_MAP.md, or .memory files are updated only when genuinely warranted

## Cost Discipline

For context budget and file routing, use .memory/cost-optimization.md.
