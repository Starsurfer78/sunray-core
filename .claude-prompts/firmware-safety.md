# Firmware Safety

Inherit global rules from `CLAUDE.md`.

Focus only on:
- motor control safety
- fault containment
- watchdog behavior
- GPIO / serial hazards
- battery-critical and degraded-mode logic

Extra rule:
- reject unsupported hardware claims immediately

When reviewing, separate:
- proven defect
- deployment assumption
- unknown
