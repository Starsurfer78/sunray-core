# Improvement Backlog

## Prioritization Model

| Priority | Meaning |
| --- | --- |
| P1 | Safety or deployment-risk reduction |
| P2 | Reliability and diagnosability |
| P3 | Maintainability and operator UX |

## Diagnostics

| Item | Benefit | Risk | Effort | Priority |
| --- | --- | --- | --- | --- |
| Add explicit watchdog telemetry fields | Faster root cause analysis | Low | Medium | P1 |
| Record op transition reason history in a compact ring buffer | Better debugging of safety flow | Low | Medium | P2 |
| Add hardware capability self-report on startup | Clarifies deployed board traits | Low | Medium | P2 |

## Architecture

| Item | Benefit | Risk | Effort | Priority |
| --- | --- | --- | --- | --- |
| Separate policy logic from `Robot.cpp` into smaller safety modules | Easier review of fault paths | Medium | High | P2 |
| Formalize command and telemetry contract docs | Reduces UI and MQTT drift | Low | Medium | P2 |
| Vendor or pin backend third-party dependency versions | Build reproducibility | Low | Medium | P2 |

## Reliability

| Item | Benefit | Risk | Effort | Priority |
| --- | --- | --- | --- | --- |
| Add tests for serial disconnect and stale MCU data | Better comms fault coverage | Medium | Medium | P1 |
| Add dock contact flap scenario coverage on hardware | Reduces charger-state uncertainty | Medium | Medium | P1 |
| Document and validate GPS failover tuning on real Alfred units | Better degraded-mode confidence | Medium | Medium | P1 |

## Testability

| Item | Benefit | Risk | Effort | Priority |
| --- | --- | --- | --- | --- |
| Add hardware-free tests for auth and command replay edge cases | Better remote safety coverage | Low | Medium | P2 |
| Add golden-config tests to prevent default drift | Catch silent config regressions | Low | Low | P2 |
| Add smoke script that builds backend plus frontend together | Faster release confidence | Low | Low | P3 |

## Deployment Safety

| Item | Benefit | Risk | Effort | Priority |
| --- | --- | --- | --- | --- |
| Capture actual `systemd` service layout from Alfred | Prevent deployment mistakes | Low | Low | P1 |
| Define rollback checklist for field updates | Safer maintenance | Low | Low | P1 |
| Store on-device version metadata for backend and frontend | Easier support | Low | Medium | P2 |
