# core/Config

**Status:** ✅ Complete
**Files:** `core/Config.h`, `core/Config.cpp`
**Purpose:** JSON-based runtime configuration — replaces config.h

**Public API:** `get<T>(key, fallback)`, `set<T>(key, value)`, `save()`, `reload()`, `dump()`, `path()`

**Notes:**
- Silent fallback on corrupt JSON.
- Forward-compatible (unknown keys accepted).
- Load order: defaults → file overrides per-key.
- Always injected as `std::shared_ptr<Config>` — never global.
- ~90 keys documented in `config.example.json`.

**Tests:** `tests/test_config.cpp` — 8 tests: missing file, overrides, unknown key, type mismatch, save+reload, reload discards, invalid JSON, dump
