// Config.cpp — Implementation of Config (see Config.h for design notes).

#include "Config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sunray {

// ── Built-in defaults ─────────────────────────────────────────────────────────
//
// Values derived from alfred/config_alfred.h and the Sunray firmware defaults.
// All keys use snake_case. Add new keys here; no other file needs changing.

nlohmann::json Config::defaults() {
    return nlohmann::json{
        // ── Driver / hardware ────────────────────────────────────────────────
        {"driver",              "serial"},      // "serial" | "sim" (future)
        {"driver_port",         "/dev/ttyS0"},  // UART device for STM32
        {"driver_baud",         115200},        // must match rm18.ino
        {"i2c_bus",             "/dev/i2c-1"},  // Linux I2C bus for PortExpander + ADC

        // ── Navigation ───────────────────────────────────────────────────────
        {"stanley_k",           0.5},           // Stanley gain (cross-track error)
        {"gps_no_motion_threshold_m", 0.05},    // below this → robot considered stationary

        // ── Energy ───────────────────────────────────────────────────────────
        {"battery_low_v",       22.0},          // return-to-dock threshold (V)
        {"battery_full_v",      29.4},          // stop-charging threshold (V)

        // ── Peripherals ──────────────────────────────────────────────────────
        {"buzzer_enabled",      true},
        {"port_expander_addr",  "0x20"},        // I2C address of MCP23017

        // ── Undocking (UndockOp) ─────────────────────────────────────────────
        {"undock_speed_ms",           0.1},     // m/s reverse speed
        {"undock_distance_m",         0.5},     // min distance from dock point
        {"undock_charger_timeout_ms", 3000},    // wait for charger disconnect
        {"undock_position_timeout_ms",10000},   // overall undock timeout
        {"undock_heading_tolerance_rad", 0.175},// ±10° IMU heading check
        {"undock_encoder_check_ms",   1000},    // encoder cross-check window
        {"undock_encoder_min_ticks",  5},       // min ticks to confirm movement

        // ── Stuck recovery (StuckRecoveryOp) ─────────────────────────────────
        {"stuck_detect_timeout_ms",   5000},
        {"stuck_recovery_max_attempts", 3},

        // ── Docking ──────────────────────────────────────────────────────────
        {"dock_retry_max_attempts",   3},

        // ── Obstacle handling (MowOp) ─────────────────────────────────────────
        {"obstacle_loop_max_count",   5},
        {"obstacle_loop_window_ms",   30000},
    };
}

// ── Constructor / load ────────────────────────────────────────────────────────

Config::Config(std::filesystem::path path)
    : path_(std::move(path))
{
    load();
}

void Config::load() {
    // Start from built-in defaults so every known key is always present.
    data_ = defaults();

    if (!std::filesystem::exists(path_)) {
        return;  // no file → pure defaults, not an error
    }

    std::ifstream f(path_);
    if (!f.is_open()) {
        return;  // can't open → pure defaults
    }

    try {
        nlohmann::json fromFile = nlohmann::json::parse(f);

        // Merge: file values override defaults key-by-key.
        // Unknown keys from the file are accepted (forward compatibility).
        for (auto& [key, val] : fromFile.items()) {
            data_[key] = val;
        }
    } catch (const nlohmann::json::exception&) {
        // Invalid JSON → keep defaults, silently ignore corrupt file
    }
}

// ── Persistence ───────────────────────────────────────────────────────────────

void Config::save() const {
    std::ofstream f(path_);
    if (!f.is_open()) {
        throw std::runtime_error(
            "Config::save(): cannot open '" + path_.string() + "' for writing");
    }
    f << data_.dump(4) << '\n';
}

void Config::reload() {
    load();
}

// ── Inspection ────────────────────────────────────────────────────────────────

std::string Config::dump() const {
    return data_.dump(4);
}

} // namespace sunray
