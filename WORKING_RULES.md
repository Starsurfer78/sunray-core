# WORKING RULES

## General

- Keep changes minimal.
- Do not break build or runtime behavior without intent.
- Avoid unnecessary complexity.

## Code Style

- Follow existing style.
- Do not reformat unrelated code.

## Git

- One logical change per commit.
- Clear commit messages.

## Safety Rules

- Do not delete files without a concrete reason.
- Do not change build system or deployment paths unless necessary.
- For navigation, docking, recovery, or resume changes: update scenario tests in the same change.

## Testing

- Run a meaningful verification step for every change.
- Prefer fresh build directories over checked-in build artifacts.
- For frontend changes, run `npm run check` when possible.

## Docs

- `TODO.md` is the only open backlog.
- Keep `README.md`, `PROJECT_OVERVIEW.md`, and `PROJECT_MAP.md` aligned with repo reality.
- Keep durable fachliche Doku in `docs/`; do not create parallel temporary task lists unless explicitly needed.
- Before refactoring `Robot::run()`, keep [docs/ROBOT_RUN_BASELINE.md](/mnt/LappiDaten/Projekte/sunray-core/docs/ROBOT_RUN_BASELINE.md) and [docs/TELEMETRY_CONTRACT.md](/mnt/LappiDaten/Projekte/sunray-core/docs/TELEMETRY_CONTRACT.md) in sync with observed behavior.

## When Unsure

- Ask or leave a focused TODO.
- Do not guess about hardware behavior.
