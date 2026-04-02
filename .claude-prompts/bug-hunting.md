# Bug Hunting

Inherit global rules from `CLAUDE.md`.

Focus only on concrete failure paths in:
- control loop
- state machine
- navigation edge cases
- communication loss
- build/test regressions

Output:
- `PASS`, `FAIL`, or `PARTIAL`
- findings first
- evidence and next tests

Do not report speculative bugs without explicit uncertainty.
