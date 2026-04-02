# Future Features

## Safety

### Hardware Capability Handshake

- Nutzen: Runtime can adapt behavior per Alfred revision
- Betroffene Module: HAL, serial driver, config, telemetry
- Risiko: protocol extension complexity
- Abhängigkeiten: MCU support
- Komplexität: Medium
- Blocker: unknown current MCU protocol extensibility

### Independent Safety Status Page

- Nutzen: quick operator view of watchdog, charger, GPS, and stop-path health
- Betroffene Module: WebSocket server, frontend, telemetry
- Risiko: false confidence if status model is incomplete
- Abhängigkeiten: telemetry schema cleanup
- Komplexität: Medium
- Blocker: production UI contract not frozen

## Navigation

### Dock Corridor Validation Tooling

- Nutzen: catch bad dock geometry before deployment
- Betroffene Module: map editor flow, planner, docs
- Risiko: medium if heuristics are wrong
- Abhängigkeiten: dock meta standardization
- Komplexität: Medium
- Blocker: authoritative dock-map examples

### Adaptive Obstacle Persistence

- Nutzen: smarter distinction between transient bumps and persistent blockers
- Betroffene Module: `Map`, escape ops, telemetry
- Risiko: could over-avoid or under-avoid without field tuning
- Abhängigkeiten: obstacle evidence model
- Komplexität: Medium
- Blocker: real-world dataset

## Serviceability

### On-Device Health Snapshot Export

- Nutzen: easier support and incident triage
- Betroffene Module: backend diagnostics, packaging, docs
- Risiko: low
- Abhängigkeiten: service inventory and log strategy
- Komplexität: Low
- Blocker: define payload contents

## Remote And UX

### Home Assistant Discovery Pack

- Nutzen: predictable HA integration for mower, dock, battery, GPS, and fault states
- Betroffene Module: MQTT client, docs, HA prompt pack
- Risiko: stale retained entities if schema changes
- Abhängigkeiten: final MQTT topic contract
- Komplexität: Medium
- Blocker: official topic taxonomy

### Guided Recovery UI

- Nutzen: operator can see exactly why robot is in `GpsWait`, `Dock`, or `Error`
- Betroffene Module: WebSocket backend, frontend, event messaging
- Risiko: messaging can mislead if event reason precedence is incomplete
- Abhängigkeiten: better event taxonomy
- Komplexität: Medium
- Blocker: stable UX requirements

## Diagnostics

### Structured Incident Bundle

- Nutzen: captures config, op history, battery, GPS, and fault context in one export
- Betroffene Module: history DB, telemetry, docs
- Risiko: privacy or size concerns if over-broad
- Abhängigkeiten: service/runtime note capture
- Komplexität: Medium
- Blocker: define retention and export trigger

## Maintenance

### Safe Update Assistant

- Nutzen: stepwise update flow with preflight, deploy, verify, rollback gate
- Betroffene Modules: scripts, docs, maybe backend version endpoint
- Risiko: operational dependence on an unproven tool
- Abhängigkeiten: deployment process inventory
- Komplexität: Medium
- Blocker: real Alfred update procedure still undocumented
