# Memory Index — sunray-core

Persistent project knowledge for Claude Code sessions.

---

## Core References

- [module_index.md](module_index.md) — Complete module directory with APIs, dependencies, test coverage. Updated on new features/modules.
- [decisions.md](decisions.md) — Architecture choices (HardwareInterface, DI pattern, frozen constraints, optional features). Rationale for design trade-offs.

---

## Quick Facts

- **Language:** C++17, Header-only Logger, nlohmann/json, Catch2, Crow, Vue 3
- **Main constraint:** No Arduino, no globals, Dependency Injection everywhere
- **Phase:** Phase 1 (A.1–A.8 complete, A.9 on hold), Phase 2 after A.9 succeeds
- **Frozen:** Telemetry JSON format (15 fields), Op-names, `/ws/telemetry` path, WebUI design Dark Blue
- **Docs:** `docs/ARCHITECTURE.md` (C++ API reference), `docs/WEBUI_ARCHITECTURE.md` (Vue architecture)
