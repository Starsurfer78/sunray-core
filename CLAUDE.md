# sunray-core

Production runtime and operator stack for Alfred (Raspberry Pi 4B + STM32).
Hardware-coupled, timing-sensitive, and safety-relevant.

## Global Rules
1. **Safety & Correctness**: Prioritize over speed. Do not guess on hardware ops.
2. **Context First**: Always check `TASK.md` and `SYSTEM_OVERVIEW.md` before architecture changes.
3. **No Duplication**: The C++ backend (`core/`) is the ONLY source of truth for planning, missions, and safety. WebUI/Mobile are dumb renderers.
4. **Keep Context Fresh**: If you discover a new architectural invariant or complete a major milestone, update `TASK.md` or use the `manage_core_memory` tool immediately.

## Tech Stack & Directories
- **Backend (`core/`, `hal/`, `platform/`)**: C++17, CMake, Crow (HTTP/WS), nlohmann/json, SQLite.
- **WebUI (`webui-svelte/`)**: SvelteKit, TypeScript, Tailwind.
- **Mobile App (`mobile-app-v2/`)**: Flutter, Dart. *(Note: `mobile-app/` is deleted, v2 is the only app).*
- **Scripts (`scripts/`)**: Bash, OTA, deployment.

## Working Style
- **Surgical Changes**: Touch only what is necessary. Keep diffs small.
- **Think Before Coding**: If ambiguous, ask the user or propose a safe assumption and document it.
- **Validate**: Fixes must be validated (e.g. compiling `build_linux`, running `ctest`, or `flutter analyze`).

## Mobile App (Flutter) Specifics
When working in `mobile-app-v2/`:
1. `flutter analyze` must report 0 errors/warnings.
2. No `// TODO`, `// FIXME`, or placeholder widgets.
3. Fully implement UI specs. No "empty containers".
4. Run `flutter analyze` after every file edit. Fix errors immediately.

## Memory & Documentation Maintenance
- **`TASK.md`**: Contains Roadmap, Architectural Vision, and Tech Debt. Update when milestones are reached.
- **`SYSTEM_OVERVIEW.md`**: Core topology. Update if process boundaries or drivers change.
- **Agent Memory**: Proactively use `manage_core_memory` to store user preferences, project invariants, or persistent troubleshooting facts. Do NOT store ephemeral task progress.