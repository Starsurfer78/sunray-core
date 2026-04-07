// test_websocket_server.cpp — Unit tests for WebSocketServer.
//
// Does NOT start a real server (no network required).
// Tests cover:
//   - buildTelemetryJson() format (frozen, must not change)
//   - Default TelemetryData values
//   - Constructor/destructor (no crash, no server started)
//   - pushTelemetry() + onCommand() registration (no crash)

#include "core/WebSocketServer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include <cmath>

using sunray::WebSocketServer;

// ── buildTelemetryJson ────────────────────────────────────────────────────────

TEST_CASE("WebSocketServer: buildTelemetryJson — mandatory fields present", "[ws]")
{
    WebSocketServer::TelemetryData d;
    d.op = "Mow";
    d.x = 1.5f;
    d.y = 2.5f;
    d.heading = 0.785f;
    d.battery_v = 25.4f;
    d.charge_v = 0.0f;
    d.gps_sol = 4;
    d.gps_text = "RTK";
    d.gps_acc = 0.021f;
    d.gps_num_corr_signals = 12;
    d.gps_lat = 51.12345678;
    d.gps_lon = 7.12345678;
    d.bumper_l = false;
    d.bumper_r = true;
    d.motor_err = false;
    d.uptime_s = 123;

    const std::string json = WebSocketServer::buildTelemetryJson(d);

    // Must be valid JSON
    auto j = nlohmann::json::parse(json); // throws on invalid JSON

    REQUIRE(j["type"] == "state");
    REQUIRE(j["op"] == "Mow");
    REQUIRE(j["gps_sol"] == 4);
    REQUIRE(j["gps_text"] == "RTK");
    REQUIRE(std::abs(j["gps_acc"].get<double>() - 0.021) < 1e-6);
    REQUIRE(j["gps_num_corr_signals"] == 12);
    REQUIRE(j["charger_connected"] == false);
    REQUIRE(j["bumper_l"] == false);
    REQUIRE(j["bumper_r"] == true);
    REQUIRE(j["motor_err"] == false);
    REQUIRE(j["uptime_s"] == 123);
    REQUIRE(j["runtime_health"] == "ok");
    REQUIRE(j["mcu_connected"] == false);
    REQUIRE(j["mcu_comm_loss"] == false);
    REQUIRE(j["gps_signal_lost"] == false);
    REQUIRE(j["gps_fix_timeout"] == false);
    REQUIRE(j["battery_low"] == false);
    REQUIRE(j["battery_critical"] == false);
    REQUIRE(j["recovery_active"] == false);
    REQUIRE(j["watchdog_event_active"] == false);
    REQUIRE(j["state_phase"] == "idle");
    REQUIRE(j["resume_target"] == "");
    REQUIRE(j["event_reason"] == "none");
    REQUIRE(j["error_code"] == "");
}

TEST_CASE("WebSocketServer: buildTelemetryJson — numeric fields are numbers", "[ws]")
{
    WebSocketServer::TelemetryData d;
    const std::string json = WebSocketServer::buildTelemetryJson(d);
    auto j = nlohmann::json::parse(json);

    REQUIRE(j["x"].is_number());
    REQUIRE(j["y"].is_number());
    REQUIRE(j["heading"].is_number());
    REQUIRE(j["battery_v"].is_number());
    REQUIRE(j["charge_v"].is_number());
    REQUIRE(j["charger_connected"].is_boolean());
    REQUIRE(j["mcu_connected"].is_boolean());
    REQUIRE(j["mcu_comm_loss"].is_boolean());
    REQUIRE(j["gps_signal_lost"].is_boolean());
    REQUIRE(j["gps_fix_timeout"].is_boolean());
    REQUIRE(j["battery_low"].is_boolean());
    REQUIRE(j["battery_critical"].is_boolean());
    REQUIRE(j["recovery_active"].is_boolean());
    REQUIRE(j["watchdog_event_active"].is_boolean());
    REQUIRE(j["gps_acc"].is_number());
    REQUIRE(j["gps_num_corr_signals"].is_number());
    REQUIRE(j["gps_lat"].is_number());
    REQUIRE(j["gps_lon"].is_number());
    REQUIRE(j["ts_ms"].is_number_unsigned());
    REQUIRE(j["state_since_ms"].is_number_unsigned());
}

TEST_CASE("WebSocketServer: buildTelemetryJson — mandatory debug keys present", "[ws]")
{
    // Required keys for frontend + debug telemetry.
    const std::string json = WebSocketServer::buildTelemetryJson({});
    auto j = nlohmann::json::parse(json);

    const std::vector<std::string> required = {
        "type", "op", "x", "y", "heading",
        "battery_v", "charge_v", "charger_connected",
        "gps_sol", "gps_text", "gps_acc", "gps_num_corr_signals", "gps_lat", "gps_lon",
        "bumper_l", "bumper_r", "motor_err", "uptime_s",
        "mcu_v", "pi_v", "imu_h", "imu_r", "imu_p",
        "diag_active", "diag_ticks", "ekf_health", "runtime_health",
        "mcu_connected", "mcu_comm_loss", "gps_signal_lost", "gps_fix_timeout",
        "battery_low", "battery_critical", "recovery_active", "watchdog_event_active",
        "ts_ms", "state_since_ms", "state_phase", "resume_target", "event_reason", "error_code",
        "ui_message", "ui_severity", "history_backend_ready", "session_id", "session_started_at_ms"};
    for (const auto &key : required)
    {
        INFO("Checking key: " << key);
        REQUIRE(j.contains(key));
    }
}

TEST_CASE("WebSocketServer: buildTelemetryJson — op names pass through", "[ws]")
{
    // All frozen Op names used by Mission Service
    for (const auto &opName : {"Idle", "Undock", "NavToStart", "Mow", "Dock",
                               "Charge", "WaitRain", "GpsWait",
                               "EscapeReverse", "EscapeForward", "Error"})
    {
        WebSocketServer::TelemetryData d;
        d.op = opName;
        auto j = nlohmann::json::parse(WebSocketServer::buildTelemetryJson(d));
        REQUIRE(j["op"] == opName);
    }
}

TEST_CASE("WebSocketServer: buildTelemetryJson — default data is valid JSON", "[ws]")
{
    WebSocketServer::TelemetryData d; // all defaults
    REQUIRE_NOTHROW([&]()
                    {
        auto parsed = nlohmann::json::parse(WebSocketServer::buildTelemetryJson(d));
        (void)parsed; }());
}

TEST_CASE("WebSocketServer: buildTelemetryJson — debug fields pass through", "[ws][a9]")
{
    WebSocketServer::TelemetryData d;
    d.ts_ms = 12345;
    d.state_since_ms = 12000;
    d.state_phase = "gps_recovery";
    d.resume_target = "Mow";
    d.event_reason = "gps_signal_lost";
    d.error_code = "ERR_GPS_TIMEOUT";
    d.ui_message = "GPS-Signal verloren";
    d.ui_severity = "warn";
    d.history_backend_ready = true;
    d.session_id = "session-123";
    d.session_started_at_ms = 1700000000000LL;
    d.runtime_health = "degraded";
    d.mcu_connected = true;
    d.gps_signal_lost = true;
    d.recovery_active = true;
    d.watchdog_event_active = false;

    auto j = nlohmann::json::parse(WebSocketServer::buildTelemetryJson(d));
    REQUIRE(j["ts_ms"] == 12345);
    REQUIRE(j["state_since_ms"] == 12000);
    REQUIRE(j["state_phase"] == "gps_recovery");
    REQUIRE(j["resume_target"] == "Mow");
    REQUIRE(j["event_reason"] == "gps_signal_lost");
    REQUIRE(j["error_code"] == "ERR_GPS_TIMEOUT");
    REQUIRE(j["ui_message"] == "GPS-Signal verloren");
    REQUIRE(j["ui_severity"] == "warn");
    REQUIRE(j["history_backend_ready"] == true);
    REQUIRE(j["session_id"] == "session-123");
    REQUIRE(j["session_started_at_ms"] == 1700000000000LL);
    REQUIRE(j["runtime_health"] == "degraded");
    REQUIRE(j["mcu_connected"] == true);
    REQUIRE(j["gps_signal_lost"] == true);
    REQUIRE(j["recovery_active"] == true);
}

// ── API surface ───────────────────────────────────────────────────────────────

TEST_CASE("WebSocketServer: constructor/destructor — no crash without start()", "[ws]")
{
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    REQUIRE_NOTHROW(static_cast<void>(WebSocketServer(config, logger)));
}

TEST_CASE("WebSocketServer: pushTelemetry — no crash when not running", "[ws]")
{
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    WebSocketServer::TelemetryData d;
    d.op = "Idle";
    REQUIRE_NOTHROW(ws.pushTelemetry(d));
}

TEST_CASE("WebSocketServer: onCommand — callback registered without crash", "[ws]")
{
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    bool called = false;
    ws.onCommand([&called](const std::string &, const nlohmann::json &)
                 { called = true; });
    // Callback is registered; it will be invoked when a WS message arrives.
    // We cannot invoke it here without a real connection — just verify no crash.
    REQUIRE_FALSE(called);
}

TEST_CASE("WebSocketServer: isRunning — false before start()", "[ws]")
{
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    REQUIRE_FALSE(ws.isRunning());
}

TEST_CASE("WebSocketServer: HTTP auth rejects missing token when api_token is set", "[ws][auth][a7]")
{
    REQUIRE_FALSE(WebSocketServer::isHttpAuthorizedForToken("secret", "", ""));
}

TEST_CASE("WebSocketServer: HTTP auth accepts X-Api-Token header", "[ws][auth][a7]")
{
    REQUIRE(WebSocketServer::isHttpAuthorizedForToken("secret", "secret", ""));
}

TEST_CASE("WebSocketServer: HTTP auth accepts Bearer token", "[ws][auth][a7]")
{
    REQUIRE(WebSocketServer::isHttpAuthorizedForToken("secret", "", "Bearer secret"));
}

TEST_CASE("WebSocketServer: HTTP auth is open only when api_token is empty", "[ws][auth][a7]")
{
    REQUIRE(WebSocketServer::isHttpAuthorizedForToken("", "", ""));
}

TEST_CASE("WebSocketServer: WS auth rejects command without token when api_token is set", "[ws][auth][a7]")
{
    REQUIRE_FALSE(WebSocketServer::isWsCommandAuthorizedForToken("secret", nlohmann::json{{"cmd", "start"}}));
}

TEST_CASE("WebSocketServer: WS auth accepts matching token", "[ws][auth][a7]")
{
    REQUIRE(WebSocketServer::isWsCommandAuthorizedForToken(
        "secret", nlohmann::json{{"cmd", "start"}, {"token", "secret"}}));
}
