# WORKING RULES

## General
- Keep changes minimal
- Do not break build
- Avoid unnecessary complexity

## Code Style
- Follow existing style
- Do not reformat unrelated code

## Git
- One logical change per commit
- Clear commit messages

## Safety Rules
- Do NOT delete files without reason
- Do NOT change build system unless necessary

## When unsure
- Ask or leave a TODO
- Do not guess

## Testing
- Always run build
- Run tests if available
- For navigation, recovery, docking, or resume changes: add or update scenario tests in the same change

## Docs
- Update TASKS.md and TODO.md when a planned architecture-risk reduction commit is completed
- Before refactoring `Robot::run()`, keep `docs/ROBOT_RUN_BASELINE.md` and `docs/TELEMETRY_CONTRACT.md` in sync with the observed pre-refactor behavior
