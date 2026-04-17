# sunray-core

Production runtime and operator stack for Alfred.
Primary target: Raspberry Pi 4B + STM32F103.
This repo is hardware-coupled, timing-sensitive, and safety-relevant.

Behavioral guidelines to reduce common LLM coding mistakes.
These rules complement the project-specific instructions below.

Tradeoff: these guidelines bias toward caution over speed. For trivial tasks, use judgment.

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
- Preserve existing fallback paths unless they are explicitly being removed or replacing.
- Update TASK.md when project status or backlog meaningfully changes.
- Update SAFETY_MAP.md when safety logic, stop conditions, or recovery behavior changes.

## Validation Rules

- Validate the narrowest relevant scope first.
- For safety or runtime changes, prefer build/tests plus a short risk note.
- If hardware verification is required but unavailable, say so explicitly instead of implying certainty.

## Working Style

### 1. Think Before Coding

- Don't assume. Don't hide confusion. Surface tradeoffs.
- State assumptions explicitly before implementing.
- If uncertain, ask instead of silently guessing.
- If multiple interpretations exist, present them instead of choosing one without comment.
- If a simpler approach exists, say so.
- Push back when a requested approach is more complex or risky than necessary.
- If something is unclear, stop, name the confusion, and ask.

### 2. Simplicity First

- Prefer the minimum code that solves the problem.
- Do not add features beyond what was asked.
- Do not introduce abstractions for single-use code.
- Do not add flexibility or configurability that was not requested.
- Do not add error handling for impossible scenarios.
- If a change grows large and a much smaller version would work, rewrite it more simply.
- Ask: would a senior engineer call this overcomplicated? If yes, simplify.

### 3. Surgical Changes

- Touch only what is necessary for the requested change.
- Do not improve adjacent code, comments, or formatting unless the task requires it.
- Do not refactor unrelated code just because it could be cleaner.
- Match the surrounding style, even if you would structure it differently from scratch.
- If unrelated dead code is noticed, mention it instead of deleting it.
- Remove imports, variables, and functions that your own changes made unused.
- Do not remove pre-existing dead code unless explicitly asked.
- Every changed line should map directly to the user's request.

### 4. Goal-Driven Execution

- Define success in verifiable terms before implementing.
- Prefer checks that prove the requested behavior instead of vague "it should work" reasoning.
- Translate requests into concrete checks where possible.
- "Add validation" means writing or running checks for invalid inputs and making them pass.
- "Fix the bug" means reproducing the bug, fixing it, and verifying the fix.
- "Refactor X" means preserving behavior before and after the change.
- For multi-step work, state a brief plan with a verification target for each step.
- Strong success criteria reduce ambiguity and make independent progress safer.

### 5. Flutter / mobile-app-v2 – Fertigstellungsregeln

Diese Regeln gelten ausschliesslich für Arbeit in `mobile-app-v2/`.

**Eine Aufgabe gilt erst als fertig wenn alle folgenden Punkte erfüllt sind:**

1. `flutter analyze` meldet 0 Errors und 0 Warnings im betroffenen File.
2. Der Code enthält kein einziges `// TODO`, `// FIXME`, `// placeholder`,
   `// stub`, `// dummy` oder auskommentiertes Widget.
3. Alle Zustände, die laut `docs/MOBILE_APP_NAVIMOW_REDESIGN_GUIDE.md`
   für einen Screen definiert sind, werden tatsächlich gerendert –
   nicht durch ein einzelnes `Text('TODO')` simuliert.
4. Jedes Widget das in der Spec beschrieben ist existiert im Code als echtes
   Widget, nicht als Kommentar oder leerer Container.
5. Die Datei ist vollständig – kein abgeschnittener Code, keine fehlenden
   closing brackets, keine Imports die auf nicht-existierende Dateien zeigen.

**Für jeden Flutter-Screen gilt ausserdem:**

- Bevor du eine Dart-Datei schreibst: lies die aktuelle Version vollständig.
- Schreibe die Datei in einem einzigen Schreibvorgang komplett neu.
  Kein iteratives Schreiben ("erster Teil... zweiter Teil...").
- Nach dem Schreiben: führe `flutter analyze path/to/file.dart` aus
  und zeige die Ausgabe. Wenn Errors vorhanden sind: fix sie sofort,
  ohne auf Anweisung zu warten.
- Zeige nach dem Schreiben den finalen Zeilencount der Datei.

**Was als Aufgabe nicht akzeptiert wird:**

- "Grundgerüst" mit echtem Code später
- "Platzhalter für jetzt, wird später ersetzt"
- Stack-Layout ohne echten Inhalt in den Positioned-Widgets
- Enum-Zustände definieren ohne sie im Build-Methode wirklich zu verwenden

## Repo Pointers

- TASK.md: current project status, completed work, deferred work
- SAFETY_MAP.md: safety-relevant flows and assumptions
- .memory/: repo knowledge, cost optimization, known issues, RTK context
