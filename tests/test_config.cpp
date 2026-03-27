// test_config.cpp — Unit tests for Config (load, get, save, reload).
//
// Uses Catch2 v3. No hardware required — pure file I/O only.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "../core/Config.h"

using namespace sunray;
using Catch::Approx;
namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static fs::path tempPath(const std::string& name) {
    return fs::temp_directory_path() / ("sunray_test_" + name + ".json");
}

static void writeJson(const fs::path& p, const std::string& content) {
    std::ofstream f(p);
    REQUIRE(f.is_open());
    f << content;
}

static void removeSilent(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("Config: missing file yields built-in defaults", "[config]") {
    fs::path p = tempPath("no_file");
    removeSilent(p);  // ensure it doesn't exist

    Config cfg(p);

    CHECK(cfg.get<std::string>("driver",      "")    == "serial");
    CHECK(cfg.get<std::string>("driver_port", "")    == "/dev/ttyS0");
    CHECK(cfg.get<int>        ("driver_baud", 0)     == 115200);
    CHECK(cfg.get<double>     ("stanley_k",   0.0)   == Approx(0.5));
    CHECK(cfg.get<double>     ("battery_low_v",  0.0) == Approx(25.5));
    CHECK(cfg.get<double>     ("battery_full_v", 0.0) == Approx(30.0));
    CHECK(cfg.get<bool>       ("buzzer_enabled", false) == true);
    CHECK(cfg.get<std::string>("port_expander_addr", "") == "0x20");
}

TEST_CASE("Config: file values override built-in defaults", "[config]") {
    fs::path p = tempPath("override");
    writeJson(p, R"({"driver_baud": 9600, "stanley_k": 1.2})");

    Config cfg(p);

    CHECK(cfg.get<int>   ("driver_baud", 0)   == 9600);
    CHECK(cfg.get<double>("stanley_k",   0.0) == Approx(1.2));
    // Keys not in the file still return built-in defaults
    CHECK(cfg.get<std::string>("driver", "") == "serial");

    removeSilent(p);
}

TEST_CASE("Config: get uses caller fallback for unknown key", "[config]") {
    fs::path p = tempPath("unknown_key");
    writeJson(p, "{}");

    Config cfg(p);

    CHECK(cfg.get<int>        ("nonexistent_int",    42)     == 42);
    CHECK(cfg.get<std::string>("nonexistent_string", "hi")   == "hi");
    CHECK(cfg.get<bool>       ("nonexistent_bool",   true)   == true);
    CHECK(cfg.get<double>     ("nonexistent_double", 3.14)   == Approx(3.14));

    removeSilent(p);
}

TEST_CASE("Config: get returns caller fallback on type mismatch", "[config]") {
    fs::path p = tempPath("type_mismatch");
    writeJson(p, R"({"driver_baud": "not_a_number"})");

    Config cfg(p);

    // "not_a_number" cannot be converted to int → fallback
    CHECK(cfg.get<int>("driver_baud", 99) == 99);

    removeSilent(p);
}

TEST_CASE("Config: set + save + reload round-trips values", "[config]") {
    fs::path p = tempPath("save_reload");
    removeSilent(p);

    {
        Config cfg(p);
        cfg.set<int>        ("driver_baud",   57600);
        cfg.set<std::string>("driver_port",   "/dev/ttyAMA0");
        cfg.set<bool>       ("buzzer_enabled", false);
        cfg.set<double>     ("stanley_k",     0.8);
        cfg.save();
    }

    Config cfg2(p);
    CHECK(cfg2.get<int>        ("driver_baud",    0)    == 57600);
    CHECK(cfg2.get<std::string>("driver_port",    "")   == "/dev/ttyAMA0");
    CHECK(cfg2.get<bool>       ("buzzer_enabled", true) == false);
    CHECK(cfg2.get<double>     ("stanley_k",      0.0)  == Approx(0.8));

    removeSilent(p);
}

TEST_CASE("Config: reload discards unsaved in-memory changes", "[config]") {
    fs::path p = tempPath("reload");
    writeJson(p, R"({"driver_baud": 115200})");

    Config cfg(p);
    cfg.set<int>("driver_baud", 1234);
    CHECK(cfg.get<int>("driver_baud", 0) == 1234);  // in-memory change visible

    cfg.reload();
    CHECK(cfg.get<int>("driver_baud", 0) == 115200); // back to file value

    removeSilent(p);
}

TEST_CASE("Config: invalid JSON falls back to defaults", "[config]") {
    fs::path p = tempPath("invalid_json");
    writeJson(p, "{ this is not json }");

    Config cfg(p);
    // Must not throw; must return built-in default
    CHECK(cfg.get<int>("driver_baud", 0) == 115200);

    removeSilent(p);
}

TEST_CASE("Config: dump returns non-empty string containing known keys", "[config]") {
    fs::path p = tempPath("dump");
    removeSilent(p);

    Config cfg(p);
    std::string json = cfg.dump();

    CHECK(!json.empty());
    CHECK(json.find("driver")       != std::string::npos);
    CHECK(json.find("driver_baud")  != std::string::npos);
    CHECK(json.find("stanley_k")    != std::string::npos);
    CHECK(json.find("buzzer_enabled") != std::string::npos);
}

TEST_CASE("Config: save throws on unwritable path", "[config]") {
    // Use a path that cannot be created (directory that doesn't exist)
    Config cfg(fs::path("/nonexistent_dir/sunray_config.json"));
    // File absent → defaults loaded, no throw yet

    CHECK_THROWS_AS(cfg.save(), std::runtime_error);
}
