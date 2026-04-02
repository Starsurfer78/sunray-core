# Bug Report

## Status

This report is the active landing zone for code findings. It separates proven defects from plausible risks so that documentation does not blur evidence levels.

## Critical

### Finding Template

- ID:
- Title:
- Status:
- Evidence:
- Reproduction:
- Risk:
- Minimal Fix:
- Verification:

### Current Critical Findings

- None confirmed at bootstrap time after the latest green test run

## High

- Incomplete hardware mapping for safety-critical lines
  - Status: Open
  - Evidence: documentation gap, not a code fault
  - Risk: wrong field assumptions during future changes
  - Minimal Fix: hardware evidence capture plus mapped signal docs

- Production service topology undocumented
  - Status: Open
  - Evidence: repo scan lacks authoritative service files
  - Risk: deployment and recovery mistakes
  - Minimal Fix: add deployment and service inventory from target device

## Medium

- MQTT production topic contract not fully centralized
  - Status: Open
  - Risk: drift between runtime, UI, and Home Assistant integration

- Frontend deployment path not documented as a controlled release procedure
  - Status: Open
  - Risk: partial updates and schema mismatch

## Low

- C++17 build emits designated-initializer warnings in Linux I2C code
  - Status: Known
  - Evidence: local build warning only
  - Risk: low immediate runtime impact, but noisy builds and portability concerns

## Plausible But Unproven

- Hidden MCU-side watchdog assumptions not mirrored in repo docs
- Dock recovery behavior may vary by hardware revision or charger hardware
- RTK degradation edge cases may depend on receiver firmware configuration not captured here

## Finding Template

### Summary

- Short defect statement

### Status

- Investigating
- Confirmed
- Fixed
- Deferred

### Reproduction

1. Preconditions
2. Steps
3. Observed behavior
4. Expected behavior

### Risk

- Safety
- Data loss
- Availability
- UX

### Minimal Fix

- Smallest acceptable safe change

### Verification

- Tests
- Logs
- On-device checks
