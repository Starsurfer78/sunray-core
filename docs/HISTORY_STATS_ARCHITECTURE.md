# History And Statistics Architecture

Last updated: 2026-03-31

## Purpose

This document defines the first architectural baseline for persistent history,
statistics, and post-mortem reasoning in `sunray-core`.

The primary goal is:

- make it nachvollziehbar why the robot changed state or stopped
- persist those reasons centrally on the Pi
- expose the data later in WebUI history and statistics views

## Separation Of Concerns

Three data categories must stay distinct:

### 1. Text log

Human-oriented runtime output such as:

- startup messages
- warnings
- diagnostics
- technical errors

Examples today:

- `Robot init failed`
- `GPS recovered`
- `Start verweigert: keine aktive Karte geladen`

Text logs are useful for live debugging, but not ideal as the primary analysis source.

### 2. Structured events

Discrete machine-readable facts that explain what happened and why.

Examples:

- state transition `Idle -> Charge`
- start rejected because `no_map`
- `Error` entered because `lift_triggered`
- docking failed because `contact_timeout`
- battery low caused dock request

Structured events are the core source for later analysis and WebUI timelines.

### 3. Sessions / metrics

Aggregated records over a longer time window, such as one mowing run.

Examples:

- session start / end
- duration
- total distance
- battery start / end
- end reason

Sessions power summary statistics and efficiency views.

## Storage Decision

The current target backend is SQLite on the Raspberry Pi.

Reasons:

- no separate server process required
- robust enough for local embedded history
- easy to backup and inspect
- well suited for WebUI-backed history and statistics

## Current Groundwork

Initial groundwork added in this step:

- config keys:
  - `history_db_enabled`
  - `history_db_path`
- a central `HistoryDatabase` bootstrap component
- schema bootstrap with `schema_meta`
- schema version tracking currently at version `2`
- backup of existing DB file before schema-changing initialization when version/state is unknown or outdated
- graceful fallback when SQLite is not available at build time
- a central `EventRecorder` that routes structured events into the history backend
- first event hooks for:
  - operator commands
  - start rejections
  - state transitions, including `Error` entry with current `error_code`
  - battery low / critical thresholds
  - GPS lost / timeout / recovery
  - lift, motor fault, bumper and rain safety edges
  - inferred dock / undock failure reasons on error transitions
  - central translation from technical reasons to user-facing German messages for logs and WebUI notices

## Initial Schema Strategy

The current bootstrap schema is intentionally small but already useful.

It guarantees:

- database open/create
- `schema_meta` table
- `events` table
- `sessions` table
- basic index set for time-ordered history queries

Current schema version:

- `2`

Current migration status for sessions:

- the core owns the authoritative mowing session lifecycle
- active session IDs and start timestamps are exposed via telemetry
- the WebUI still keeps a local compatibility cache for existing History/Statistics screens
- new frontend session entries align to backend-generated session IDs instead of inventing their own

Current API surface:

- `GET /api/history/events?limit=100`
- `GET /api/history/sessions?limit=100`
- `GET /api/history/export`
- `GET /api/statistics/summary`

Current WebUI status:

- `History.vue` reads central sessions and events from the backend instead of `localStorage`
- `Statistics.vue` reads summary and session aggregates from the backend
- the old browser-local `useSessionTracker` only remains as a compatibility cache and is no longer the source for the main views

Current retention / support rules:

- retention is row-count based for now
- `history_db_max_events` limits persisted events
- `history_db_max_sessions` limits persisted sessions
- `history_db_export_enabled` gates support download of the SQLite file

## Runtime Behavior Expectations

When history DB is enabled:

- startup should attempt to initialize the database once
- missing parent directories should be created automatically
- database bootstrap failure should be visible in logs
- existing DB files must be reused, not deleted
- before a schema-changing bootstrap on an old or unknown DB, a timestamped backup should be created

When SQLite support is missing at build time:

- runtime should not crash
- history DB remains unavailable
- the logger should state clearly that SQLite support is not built in

## Safety Expectations For Existing Data

The intended rule is:

- never silently delete an existing history database
- never overwrite an existing database with an empty one
- backup first if the schema state is unknown or older than the current code expects

The backup format is currently:

- `<history_db_path>.bak-YYYYMMDD-HHMMSS`

## Next Steps

Planned immediate follow-up tasks:

1. review whether more op-specific failure reasons should be emitted directly instead of inferred in `Robot`
2. extend history endpoints with server-side filters once the first WebUI consumer is in place
3. decide whether path samples should move from compatibility cache into the backend as well
4. add migration notes for existing installations with older browser-only history
