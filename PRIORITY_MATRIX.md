# Priority Matrix

## Status

This matrix is the execution control surface for Phase 3.
New findings are first recorded in `BUG_REPORT.md`, `IMPROVEMENT_BACKLOG.md`, or `FUTURE_FEATURES.md`.
Execution then happens only from this matrix, one `P1` item at a time.

## Scoring Model

| Score band | Meaning |
| --- | --- |
| 90-100 | Immediate execution candidate |
| 75-89 | Important next wave after current `P1` lane |
| 60-74 | Valuable, but not on the critical path |
| 40-59 | Later-phase feature or structural work |

## Active Lane

| Lane | Item | Status | Notes |
| --- | --- | --- | --- |
| `P1` | none | Clear | `IB-017`, `IB-018`, and `IB-020` are done; no queued `P1` items remain |

## Fix And Improvement Priorities

| ID | Source | Title | Score | Phase | Risiko | Confidence | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `IB-001` | `BUG-CRIT-001`, `BUG-HIGH-001` | Serialize external commands onto the control-loop thread | 98 | `P1` | High | High | Done |
| `IB-002` | `BUG-HIGH-002` | Restore stop-button authority during active diagnostics | 96 | `P1` | High | High | Done |
| `IB-003` | `BUG-MED-002` | Add an explicit Pi-side communication-loss safety transition | 93 | `P1` | High | Medium | Done |
| `IB-004` | `BUG-HIGH-003`, `BUG-PLAUS-001` | Harden MQTT reconnect and command-subscription recovery | 89 | `P1` | Medium | Medium | Done |
| `IB-005` | `BUG-MED-003`, `BUG-MED-004` | Add charger-contact debounce and dock-state observability | 87 | `P1` | Medium | Medium | Done |
| `IB-006` | bug regression coverage | Add fault-injection and soak coverage for UART, MQTT, and dock-contact edge cases | 82 | `P1` | Low | High | Done |
| `IB-012` | `BUG-HIGH-004` | Require RTK Fix before resuming dock-critical approach | 91 | `P1` | Medium | High | Done |
| `IB-013` | `BUG-HIGH-005` | Detect non-progressive dock retries and escalate earlier | 84 | `P1` | Medium | Medium | Done |
| `IB-014` | `BUG-HIGH-006` | Surface failed WebSocket command sends to the operator UI | 90 | `P1` | Low | High | Done |
| `IB-015` | `BUG-HIGH-007` | Keep the active mission card visible through transient recovery ops | 86 | `P1` | Low | High | Done |
| `IB-016` | `BUG-HIGH-008` | Tune GPS-loss thresholds for safer EKF coast through short shadow zones | 92 | `P1` | Low | Medium | Done |
| `IB-017` | `BUG-HIGH-008` | Add bounded mowing GPS-coast window on degraded EKF fusion | 88 | `P1` | Medium | Medium | Done |
| `IB-018` | `BUG-HIGH-009` | Add encoder-based stuck detection and recovery dispatch | 95 | `P1` | Medium | High | Done |
| `IB-020` | statistics / observability evidence | Persist field-usable incident counters in the history summary | 66 | `P2` | Low | High | Done |
| `IB-019` | `BUG-MED-006` | Add lateral-offset dock retry geometry for repeated contact misses | 81 | `P2` | Medium | Medium | Done |
| `IB-008` | deployment evidence | Make deployment and rollback procedure explicit and verifiable | 72 | `P2` | Low | Medium | Done |
| `IB-009` | config drift evidence | Clarify and stabilize configuration ownership for Alfred runtime defaults | 69 | `P2` | Medium | Medium | Done |
| `IB-007` | health telemetry evidence | Centralize runtime health telemetry for watchdog, comms, and recovery state | 64 | `P2` | Low | Medium | Done |
| `IB-010` | hardware assumption evidence | Add hardware-capability and safety-assumption self-report at startup | 55 | `P3` | Low | Medium | Queued |
| `IB-011` | high-risk `Robot.cpp` concentration | Break out and document high-risk safety policy seams inside `Robot` | 48 | `P3` | Medium | High | Queued |

## Feature Priorities

| ID | Title | Score | Phase | Risiko | Confidence | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `FF-001` | Runtime Safety Status Surface | 71 | `F1` | Low | High | Queued |
| `FF-003` | On-Device Incident Bundle Export | 68 | `F1` | Low | High | Queued |
| `FF-004` | Hardware Self-Test and Bring-Up Report | 67 | `F1` | Medium | High | Queued |
| `FF-002` | Guided Fault Recovery Workflow | 63 | `F2` | Medium | High | Queued |
| `FF-005` | Dock Geometry Validation Assistant | 60 | `F2` | Medium | High | Queued |
| `FF-006` | RTK Degradation Analyzer | 58 | `F2` | Medium | Medium | Queued |
| `FF-007` | Mission Resume With Safety Preconditions | 57 | `F2` | High | Medium | Queued |
| `FF-010` | Long-Run Comms Health Analytics | 56 | `F2` | Low | Medium | Queued |
| `FF-008` | Home Assistant Operational Workflow Pack | 52 | `F2` | Low | Medium | Queued |
| `FF-011` | Operator Calibration Workspace | 51 | `F2` | Medium | Medium | Queued |
| `FF-009` | Safe Update Assistant for Alfred | 49 | `F3` | Medium | Medium | Queued |
| `FF-012` | Degraded-Mode Maintenance API | 45 | `F3` | High | Medium | Queued |

## Notes

- `Score` is a decision aid, not a severity replacement.
- `Risiko` reflects change risk, not only runtime danger.
- `Confidence` reflects how strongly the repository evidence supports the item definition and landing zone.
