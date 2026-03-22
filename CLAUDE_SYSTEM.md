# CLAUDE_SYSTEM.md — sunray-core Agent Rules

## Role

You are implementing a clean C++17 rewrite of a robot mower firmware core.
Every decision must be intentional, testable, and documented.

---

## Aktives Repo

Arbeitsverzeichnis: E:\TRAE\sunray-core\
Memory: E:\TRAE\sunray-core\.memory\

E:\TRAE\Sunray\ = Read-only Referenz.
Niemals dort committen oder Memory-Dateien schreiben.
Wenn Code aus Sunray gebraucht wird: lesen, nach sunray-core portieren, fertig.

## Mandatory Session Start

Before any work:

1. Read `.memory/module_index.md`
2. Read `.memory/decisions.md`
3. Read `TODO.md` — find the next open `[ ]` item in Section A

Never start work without knowing the current state.

---

## Task Execution Rules

### Before writing code

- Read ALL files you will modify or that your new code depends on
- If a file is >200 lines: read interface + relevant functions only
- State explicitly: "I will modify X, Y, Z"

### While writing code

- One TODO item per task — do not jump ahead
- C++17 only — no Arduino, no platform-specific code in core/
- Every new class gets a header guard + `#pragma once`
- Every public method gets a brief comment
- Use `std::unique_ptr` for ownership, `std::shared_ptr` for shared access

### After writing code

- Verify: does it compile? (check includes, types, method signatures)
- Write or update the test in `tests/test_<module>.cpp`
- Mark `TODO.md` as `[x]`
- Update `.memory/module_index.md`
- Commit with format: `feat(module): description`

---

## Subagent Rules

**Trigger subagent when:**

- File is >300 lines AND you need to understand it deeply
- Task requires reading >3 files simultaneously
- Porting a large module (Map.cpp, StateEstimator.cpp)

**Subagent gets:**

- Specific question only
- Max 3 files
- No global context — focused task

**Do NOT use subagent for:**

- Simple file reads
- Writing new small modules (<100 lines)
- Config or platform layer work

---

## Architectural Constraints (non-negotiable)

1. `HardwareInterface` is the ONLY boundary between core and hardware
2. No global objects in core/ — everything via constructor injection
3. `Config` is always passed by `std::shared_ptr<Config>` — never global
4. `nearObstacle` defaults to `false` — Alfred has no sonar
5. BUG-07 (PWM/encoder swap) lives in `SerialRobotDriver` only
6. AT-frame protocol is unchanged in Phase 1

---

## Decision Protocol

When you encounter a design choice:

1. State the options briefly
2. Pick the one that is simpler AND consistent with existing decisions
3. Write it to `.memory/decisions.md` format:

```
## [date] Decision: <title>
**Choice:** <what was chosen>
**Reason:** <one sentence why>
**Rejected:** <alternative and why not>
```

---

## Error Handling

- If a file doesn't exist yet: create it, don't assume
- If a compile error is likely: state it and fix it before moving on
- If a TODO item is blocked: note the blocker in TODO.md and skip to next
- Never leave broken code — if unsure, write a stub with TODO comment

---

## Phase Discipline

- **Phase 1:** Only Section A of TODO.md
- **Phase 2:** Only after `[x] Alfred Build-Test` in TODO.md Section A
- **Mission Service (Section C):** Separate project — do not touch

---

## Commit Message Format

```
feat(config): add Config class with nlohmann/json
feat(hal): add HardwareInterface abstract base
feat(serial): implement SerialRobotDriver AT+M/S/V
test(config): add 8 unit tests for Config
fix(odometry): correct unsigned overflow in tick delta
```
