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
        {"driver",              "serial"},      // "serial" = Alfred/STM32, "sim" = Simulation
        {"driver_port",         "/dev/ttyS0"},
        {"driver_baud",         115200},
        {"i2c_bus",             "/dev/i2c-1"},

        // ── GPS ──────────────────────────────────────────────────────────────
        {"gps_port",            "/dev/serial/by-id/usb-u-blox_AG_-_www.u-blox.com_u-blox_GNSS_receiver-if00"},
        {"gps_baud",            115200},
        {"gps_configure",       false},
        {"gps_config_filter",   true},          // Elevationsfilter fuer stabiles RTK-Signal
        {"gps_wait_timeout_ms", 600000},        // 0 = warten bis Signal da
        {"gps_require_valid",   true},          // REQUIRE_VALID_GPS
        {"gps_speed_detection", true},          // GPS_SPEED_DETECTION
        {"gps_motion_detection",true},          // GPS_MOTION_DETECTION
        {"gps_motion_detection_timeout_s", 5},  // GPS_MOTION_DETECTION_TIMEOUT
        {"gps_no_motion_threshold_m", 0.05},

        // ── NTRIP ────────────────────────────────────────────────────────────
        {"ntrip_enabled",       false},
        {"ntrip_host",          "www.sapos-nw-ntrip.de"},
        {"ntrip_port",          2101},
        {"ntrip_mount",         "VRS_3_4G_NW"},
        {"ntrip_user",          "user"},
        {"ntrip_pass",          "pass"},

        // ── Odometrie ────────────────────────────────────────────────────────
        {"ticks_per_revolution", 320},          // Alfred RM24
        {"wheel_diameter_m",    0.205},         // Alfred (205 mm)
        {"wheel_base_m",        0.390},         // Alfred (39 cm Mitte-Mitte)

        // ── Roboter-Geometrie ─────────────────────────────────────────────────
        {"robot_length_m",      0.60},          // MOWER_SIZE = 60 cm
        {"robot_width_m",       0.43},
        {"gps_offset_x_m",      0.0},           // Antennen-Offset im Roboter-KS (x=vorwaerts)
        {"gps_offset_y_m",      0.19},          // 19 cm vor Radachse

        // ── Motor PID ─────────────────────────────────────────────────────────
        {"motor_pid_lp",        0.0},           // Encoder-Tiefpassfilter (0=deaktiviert)
        {"motor_pid_kp",        0.5},
        {"motor_pid_ki",        0.01},
        {"motor_pid_kd",        0.01},

        // ── Motorstrom-Grenzen / Fehlerbehandlung ─────────────────────────────
        {"motor_fault_current_a",        3.0},
        {"motor_overload_current_a",     1.2},
        {"motor_too_low_current_a",      0.005}, // Blockade-Erkennung (0=deaktiviert)
        {"motor_overload_speed_ms",      0.1},   // Geschwindigkeit bei Ueberlast
        {"enable_fault_detection",       true},
        {"enable_overload_detection",    false},  // false=verlangsamen, true=stoppen
        {"enable_fault_obstacle_avoidance", true},
        {"fault_max_successive_count",   5},     // Max. aufeinanderfolgende Fehler

        // ── Maehmotor ─────────────────────────────────────────────────────────
        {"mow_fault_current_a",      8.0},
        {"mow_overload_current_a",   2.0},
        {"mow_too_low_current_a",    0.005},     // Blockade-Erkennung (0=deaktiviert)
        {"mow_toggle_dir",           true},      // Drehrichtung bei jedem Start wechseln
        {"enable_mow_motor",         true},      // false zum Testen ohne Messer

        // ── Navigation (Stanley-Regler) ───────────────────────────────────────
        {"stanley_k",           0.5},
        {"stanley_k_normal",    0.1},           // Alfred: STANLEY_CONTROL_K_NORMAL
        {"stanley_p_normal",    1.1},           // Alfred: STANLEY_CONTROL_P_NORMAL
        {"stanley_k_slow",      0.1},           // Alfred: STANLEY_CONTROL_K_SLOW
        {"stanley_p_slow",      1.1},           // Alfred: STANLEY_CONTROL_P_SLOW
        {"motor_set_speed_ms",  0.3},
        {"dock_linear_speed_ms",0.1},
        {"target_reached_tolerance_m", 0.1},

        // ── Entfuehrungs-Erkennung ────────────────────────────────────────────
        {"kidnap_detect",               true},
        {"kidnap_detect_tolerance_m",   1.0},

        // ── Akku ──────────────────────────────────────────────────────────────
        {"battery_low_v",           25.5},      // GO_HOME_VOLTAGE
        {"battery_critical_v",      18.9},      // BAT_UNDERVOLTAGE
        {"battery_full_v",          30.0},      // BAT_FULL_VOLTAGE
        {"battery_full_current_a",  -0.1},      // Ladestrom unter dem = voll
        {"battery_full_slope",      0.002},     // BAT_FULL_SLOPE (V/min)

        // ── Temperatur ────────────────────────────────────────────────────────
        {"overheat_temp_c",     85},
        {"too_cold_temp_c",     5},
        {"use_temp_sensor",     true},          // false wenn kein HTU21D verbaut

        // ── Peripherals ──────────────────────────────────────────────────────
        {"buzzer_enabled",          true},
        {"port_expander_addr",      "0x20"},
        {"bumper_deadtime_ms",      1000},
        {"bumper_invert",           false},
        {"enable_lift_detection",   true},
        {"lift_obstacle_avoidance", true},      // false=nur Messer aus, true=Ausweichen
        {"lift_invert",             false},
        {"enable_tilt_detection",   true},

        // ── Hinderniserkennung ────────────────────────────────────────────────
        {"obstacle_diameter_m", 1.2},
        {"obstacle_loop_max_count",   5},
        {"obstacle_loop_window_ms",   30000},

        // ── Undocking (UndockOp) ──────────────────────────────────────────────
        {"undock_speed_ms",             0.08},  // UNDOCK_SPEED
        {"undock_distance_m",           0.32},
        {"undock_charger_timeout_ms",   5000},  // UNDOCK_CHARGER_OFF_TIMEOUT
        {"undock_position_timeout_ms",  10000},
        {"undock_heading_tolerance_rad",0.175},
        {"undock_encoder_check_ms",     1000},
        {"undock_encoder_min_ticks",    5},
        {"undock_ignore_gps_distance_m",2.0},

        // ── Docking ──────────────────────────────────────────────────────────
        {"dock_trackslow_distance_m",       5.0},
        {"dock_auto_start",                 true},   // nach Laden automatisch wieder mähen
        {"dock_front_side",                 true},   // mit Vorderseite einfahren
        {"dock_retry_max_attempts",         3},
        {"dock_retry_approach_ms",          2000},
        {"dock_retry_contact_timeout_ms",   3000},
        {"preflight_min_voltage",           26.5},
        {"preflight_gps_timeout_ms",        60000},

        // ── Stuck recovery (StuckRecoveryOp) ──────────────────────────────────
        {"stuck_detect_timeout_ms",     5000},
        {"stuck_detect_min_speed_ms",   0.01},
        {"stuck_recovery_max_attempts", 3},
        {"stuck_recovery_reverse_ms",   2000},
        {"stuck_recovery_pause_ms",     1000},
        {"stuck_recovery_forward_ms",   1500},

        // ── MQTT ─────────────────────────────────────────────────────────────
        {"mqtt_enabled",        false},
        {"mqtt_host",           "localhost"},
        {"mqtt_port",           1883},
        {"mqtt_keepalive_s",    60},
        {"mqtt_topic_prefix",   "sunray"},
        {"mqtt_user",           ""},
        {"mqtt_pass",           ""},
        {"api_token",           ""},

        // ── Sonstiges ────────────────────────────────────────────────────────
        {"rain_delay_min",      60},
        {"map_path",            "/etc/sunray/map.json"},
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
