# MQTT Topics

## Zweck

Arbeitsbasis fuer eine vollstaendige Topic-Hierarchie unter `robot/lawnmower/#`.

## Aktueller Stand

Die genaue produktive Topic-Struktur ist im Repo noch nicht als zentrale Spezifikation dokumentiert. Diese Datei ist daher Startpunkt fuer eine robuste Definition.

## Proposed Hierarchy

| Topic | Richtung | Payload | QoS | Retain | Safety Check |
| --- | --- | --- | --- | --- | --- |
| `robot/lawnmower/status` | out | high-level state snapshot | 1 | yes | read-only |
| `robot/lawnmower/telemetry` | out | live telemetry JSON | 0 or 1 | no | freshness over persistence |
| `robot/lawnmower/fault` | out | active fault summary | 1 | yes | must clear deterministically |
| `robot/lawnmower/cmd/start` | in | command token plus request | 1 | no | auth and replay guard |
| `robot/lawnmower/cmd/dock` | in | command token plus request | 1 | no | auth and replay guard |
| `robot/lawnmower/cmd/stop` | in | command token plus request | 2 preferred | no | safety-critical |
| `robot/lawnmower/heartbeat` | out | timestamp plus health | 1 | no | timeout monitoring |
| `robot/lawnmower/discovery/...` | out | HA discovery payloads | 1 | yes | schema stability required |

## Payload Schemas

- Status: op, state_phase, battery, charge, GPS quality, uptime, error code
- Telemetry: high-frequency metrics and debug-friendly state
- Fault: code, reason, severity, time, recoverability
- Command: action, request id, auth token or validated session context

## Retain Policy

- Retain allowed for status and discovery
- No retain for motion commands
- Fault retain only if clear semantics are defined

## Examples

```json
{"op":"Dock","state_phase":"returning_home","battery_v":24.8,"gps_sol":4}
```

## Safety Checks

- Never retain unsafe commands
- Validate payload schema before execution
- Add timeout-based stale command rejection

## Known Unknowns

- Actual production topic names
- Required HA entity set
- Broker deployment model

## Update Rules

- Keep topic, QoS, and retain decision together in the same table
- Any command topic needs explicit auth and replay story
