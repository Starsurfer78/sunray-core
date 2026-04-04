# sunray-core

Pi-side runtime for Alfred (Raspberry Pi 4B + STM32F103) and Flutter mobile app.
Safety-critical robotics system.

## Rules

- Safety > Correctness > Determinism > minimal diff
- Read `TASK.md` before starting work
- `FACT / INFERENCE / UNKNOWN` for hardware claims
- High-risk areas: motor control, emergency stop, watchdogs, UART/MCU reconnect, docking, battery critical, GPS recovery
- Keep diffs small in high-risk areas, update `SAFETY_MAP.md` if safety logic changes
