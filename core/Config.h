#pragma once
// Config.h — Runtime configuration via JSON file (replaces config.h macros).
//
// Design:
//   - Loads config.json at construction; starts with built-in defaults if
//     the file is absent or contains invalid JSON.
//   - File values override built-in defaults on a per-key basis.
//   - get<T>(key, fallback): returns the value for key, or fallback if the key
//     is missing or has an incompatible type. Type conversion is handled by
//     nlohmann::json — no manual casting required.
//   - set<T>(key, value): writes into the in-memory document (not to disk).
//   - save(): writes the full in-memory document back to the file.
//   - dump(): returns the current document as a pretty-printed JSON string.
//   - reload(): re-reads the file, discarding any unsaved in-memory changes.
//
// Intended use (Dependency Injection — no Singleton):
//   auto config = std::make_shared<Config>("/etc/sunray/config.json");
//   auto hw     = std::make_unique<SerialRobotDriver>(config);
//   auto robot  = Robot(std::move(hw), config);

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace sunray {

class Config {
public:
    /// Load configuration from path.
    /// If the file does not exist or contains invalid JSON, the in-memory
    /// document is initialised with built-in defaults only (no exception thrown).
    explicit Config(std::filesystem::path path);

    // ── Read ──────────────────────────────────────────────────────────────────

    /// Returns the value for key cast to T.
    /// Lookup order: (1) in-memory document, (2) caller-supplied fallback.
    /// If the key exists but cannot be converted to T, fallback is returned.
    ///
    /// Supported T: bool, int, unsigned, long, float, double, std::string,
    ///              and any other type that nlohmann::json can convert.
    template<typename T>
    T get(const std::string& key, const T& fallback) const;

    // ── Write ─────────────────────────────────────────────────────────────────

    /// Write key=value into the in-memory document.
    /// Call save() afterwards to persist.
    template<typename T>
    void set(const std::string& key, const T& value);

    // ── Persistence ───────────────────────────────────────────────────────────

    /// Write the current in-memory document to disk (pretty-printed, 4 spaces).
    /// Throws std::runtime_error if the file cannot be opened for writing.
    void save() const;

    /// Discard in-memory changes and reload from disk.
    /// If the file is gone or broken, resets to built-in defaults.
    void reload();

    // ── Inspection ────────────────────────────────────────────────────────────

    /// Returns the current in-memory document as a pretty-printed JSON string.
    std::string dump() const;

    /// The path that was passed to the constructor.
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    nlohmann::json        data_;   ///< in-memory document (defaults merged + file)

    /// Map legacy alias keys to canonical keys used internally.
    static std::string canonicalKey(const std::string& key);

    /// Built-in default values for all known configuration keys.
    /// Any key absent from the JSON file falls back to these.
    static nlohmann::json defaults();

    /// Load from disk and merge on top of defaults.
    void load();
};

// ── Template implementations ──────────────────────────────────────────────────

template<typename T>
T Config::get(const std::string& key, const T& fallback) const {
    const std::string k = canonicalKey(key);
    if (data_.contains(k)) {
        try {
            return data_.at(k).get<T>();
        } catch (...) {
            // Type mismatch — fall through to caller's fallback
        }
    }
    return fallback;
}

template<typename T>
void Config::set(const std::string& key, const T& value) {
    data_[canonicalKey(key)] = value;
}

} // namespace sunray
