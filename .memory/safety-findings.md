# Safety Findings

## Zweck

Sammelstelle fuer safety-relevante Beobachtungen, Schwachstellen und Folgeaktionen.

## Confirmed Safeguards

- Motor-stop on key safety events is explicitly modeled
- Dock watchdog has automated test coverage
- Battery critical path can stop runtime and drop power-hold
- Perimeter violation is handled before normal progression
- Error state and obstacle recovery flows are scenario-tested

## Suspected Weaknesses

- Hardware-level motor enable path not fully evidenced
- External watchdog chain, if present, is undocumented here
- Production service supervision is not yet described with proof

## Open Validation Points

- Confirm physical e-stop / relay topology
- Confirm charger contact edge cases on hardware
- Confirm GPS degradation behavior near dock and under canopy
- Confirm serial stale-data handling under cable or MCU faults

## Incidents / Near Misses

- Historical repo state contained drift between runtime defaults and some tests
- Documentation state previously mixed active and legacy material

## Follow-Up Actions

- Capture real Alfred hardware evidence
- Add deployment/service inventory from production target
- Define MQTT safety contract and HA entity map

## Update Rules

- Jedes Finding mit Quelle oder Beobachtung verknuepfen
- suspected weakness nach Verifikation entweder hoch- oder herunterstufen
