# sunray-core

Production runtime and operator stack for Alfred.
Primary target: Raspberry Pi 4B + STM32F103.
This repo is hardware-coupled, timing-sensitive, and safety-relevant.

## Priorities

1. Safety
2. Correctness
3. Determinism
4. Operability
5. Minimal diff

## Always Do This First

- Read TASK.md before starting implementation work.
- Identify whether the change touches safety-critical runtime behavior, planning, connectivity, or UI only.
- State hardware-related claims as FACT, INFERENCE, or UNKNOWN when evidence is incomplete.

## High-Risk Areas

- Motor control and motion commands
- Emergency stop and watchdog behavior
- UART, MCU reconnect, and command serialization
- Docking, charging, and battery-critical paths
- GPS recovery, RTK state handling, and mission start gating
- Autonomous mission planning and route validation

## Change Rules

- Keep diffs small in high-risk areas.
- Prefer root-cause fixes over UI-only masking.
- Do not introduce a second source of truth for missions, route progress, or safety state.
- Preserve existing fallback paths unless they are explicitly being removed or replaced.
- Update TASK.md when project status or backlog meaningfully changes.
- Update SAFETY_MAP.md when safety logic, stop conditions, or recovery behavior changes.

## Validation Rules

- Validate the narrowest relevant scope first.
- For safety or runtime changes, prefer build/tests plus a short risk note.
- If hardware verification is required but unavailable, say so explicitly instead of implying certainty.

## Repo Pointers

- TASK.md: current project status, completed work, deferred work
- SAFETY_MAP.md: safety-relevant flows and assumptions
- .memory/: repo knowledge, cost optimization, known issues, RTK context
