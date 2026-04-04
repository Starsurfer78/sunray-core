# CLAUDE.md
# sunray-core Context Router

## Mission

Work on `sunray-core`, Alfred's production Pi-side runtime for Raspberry Pi 4B + STM32F103.
This is a safety-critical robotics system.

Priority order:
1. Safety
2. Correctness
3. Deterministic runtime behavior
4. Minimal diffs
5. Verification
6. Cost-efficient context loading

## Core Law

`Describe -> Prove -> Change`

Never invent undocumented hardware facts.

## Global Rules

- Read `TASK.md` first.
- Select exactly one `ctx-profile`.
- Load only the routed files for that profile.
- Use `FACT / INFERENCE / UNKNOWN` for hardware/runtime certainty.
- High-risk changes require verifier evidence.
- Update `TASK.md`, relevant `.memory/*`, and affected maps when understanding or behavior changes.

## High-Risk Areas

- motor control
- emergency stop
- watchdogs / timeouts
- UART / MCU reconnect
- docking / charging
- battery critical handling
- GPS invalid / recovery
- communication-loss recovery

If touched:
- state risks explicitly
- keep the diff small
- run verification
- update `SAFETY_MAP.md` if safety logic changed

## ctx-profiles

`discovery`
- `TASK.md`
- `.memory/discovery-summary.md`
- `.claude-prompts/repo-scanner.md`

`hardware`
- `TASK.md`
- `HARDWARE_MAP.md`
- `.memory/hardware-pinout.md`
- `.memory/runtime-knowledge.md`
- `.claude-prompts/hardware-mapper.md`

`safety`
- `TASK.md`
- `SAFETY_MAP.md`
- `.memory/safety-findings.md`
- `.claude-prompts/firmware-safety.md`
- `.claude-prompts/safety-auditor.md`

`RTK`
- `TASK.md`
- `.memory/rtk-context.md`
- `.claude-prompts/rtk-integration.md`

`MQTT`
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.memory/known-issues.md`
- `.claude-prompts/mqtt-protocol.md`

`HA`
- `TASK.md`
- `.memory/mqtt-topics.md`
- `.claude-prompts/ha-integration.md`

`bugfix`
- `TASK.md`
- `.memory/known-issues.md`
- `.memory/discovery-summary.md`
- `.claude-prompts/bug-hunting.md`
- `.claude-prompts/verifier.md`

`future`
- `TASK.md`
- `.memory/discovery-summary.md`
- `.memory/architecture-decisions.md`
- `.claude-prompts/architect-future.md`

Load `IMPROVEMENT_BACKLOG.md` or `FUTURE_FEATURES.md` only if the task directly needs one of them.

## Token Discipline

- Never bulk-load all markdown files.
- Prefer one task file, only the needed maps/memory, and one specialist prompt.
- Reuse `.memory/*` snapshots before re-reading broad docs.
- Split work if scope drifts across more than 3 subsystems.
- Prefer `.memory/discovery-summary.md` over broad root discovery files.
- Prefer `.memory/known-issues.md` over full bug reports.
- Prefer architecture snapshots over reopening multiple root maps.

Reference: `.memory/cost-optimization.md`

## Completion States

Use:
- `DONE`
- `PARTIAL`
- `BLOCKED`
- `UNVERIFIED`

`DONE` requires completed scope, explicit risks/unknowns, relevant updates, and verification where required.

## Mobile Build Notes

- Android debug APK path: `mobile-app/build/app/outputs/apk/debug/app-debug.apk`
- Flutter debug APK path: `mobile-app/build/app/outputs/flutter-apk/app-debug.apk`
- If Android/Flutter builds fail with cryptic type resolution or "type not found" errors despite correct imports, run `flutter clean` first. A stale build cache previously caused inconsistent build state.
- One prior successful debug build also had to install missing Android SDK Platform 34 during the build, which added a long delay.
- Required environment variables for future builds:

```bash
export ANDROID_HOME=/mnt/LappiDaten/Projekte/android-sdk
export ANDROID_SDK_ROOT=/mnt/LappiDaten/Projekte/android-sdk
export JAVA_HOME=/mnt/LappiDaten/Projekte/jdk
```
