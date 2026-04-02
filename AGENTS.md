# AGENTS.md

Use [CLAUDE.md](/mnt/LappiDaten/Projekte/sunray-core/CLAUDE.md) as the single canonical rule set.

## Repo Identity

`sunray-core` is Alfred's production Pi-side runtime:
- Raspberry Pi 4B
- STM32F103
- C++17
- RTK GPS
- MQTT / Home Assistant

The repo is hardware-coupled, timing-sensitive, and safety-relevant.

## Agent Rule

- Do not restate global rules from `CLAUDE.md`.
- Read `TASK.md` first.
- Select one `ctx-profile`.
- Load only the routed files for that profile.
- Keep diffs small.
- Verify before closing.
- Update `TASK.md`, relevant `.memory/*`, and affected maps when understanding changes.

## Done

Done means:
- requested scope solved
- checks passed or clearly blocked
- risks/unknowns stated
- docs/memory updated where needed

For budgets and routing, follow `CLAUDE.md` and `.memory/cost-optimization.md`.
