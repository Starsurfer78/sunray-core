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

using sunray::WebSocketServer;

// ── buildTelemetryJson ────────────────────────────────────────────────────────

TEST_CASE("WebSocketServer: buildTelemetryJson — mandatory fields present", "[ws]") {
    WebSocketServer::TelemetryData d;
    d.op        = "Mow";
    d.x         = 1.5f;
    d.y         = 2.5f;
    d.heading   = 0.785f;
    d.battery_v = 25.4f;
    d.charge_v  = 0.0f;
    d.gps_sol   = 4;
    d.gps_text  = "RTK";
    d.gps_lat   = 51.12345678;
    d.gps_lon   = 7.12345678;
    d.bumper_l  = false;
    d.bumper_r  = true;
    d.motor_err = false;
    d.uptime_s  = 123;

    const std::string json = WebSocketServer::buildTelemetryJson(d);

    // Must be valid JSON
    auto j = nlohmann::json::parse(json);  // throws on invalid JSON

    REQUIRE(j["type"]      == "state");
    REQUIRE(j["op"]        == "Mow");
    REQUIRE(j["gps_sol"]   == 4);
    REQUIRE(j["gps_text"]  == "RTK");
    REQUIRE(j["bumper_l"]  == false);
    REQUIRE(j["bumper_r"]  == true);
    REQUIRE(j["motor_err"] == false);
    REQUIRE(j["uptime_s"]  == 123);
}

TEST_CASE("WebSocketServer: buildTelemetryJson — numeric fields are numbers", "[ws]") {
    WebSocketServer::TelemetryData d;
    const std::string json = WebSocketServer::buildTelemetryJson(d);
    auto j = nlohmann::json::parse(json);

    REQUIRE(j["x"].is_number());
    REQUIRE(j["y"].is_number());
    REQUIRE(j["heading"].is_number());
    REQUIRE(j["battery_v"].is_number());
    REQUIRE(j["charge_v"].is_number());
    REQUIRE(j["gps_lat"].is_number());
    REQUIRE(j["gps_lon"].is_number());
}

TEST_CASE("WebSocketServer: buildTelemetryJson — all 14 keys present", "[ws]") {
    // Frozen key set — any missing key breaks the Mission Service / frontend
    const std::string json = WebSocketServer::buildTelemetryJson({});
    auto j = nlohmann::json::parse(json);

    const std::vector<std::string> required = {
        "type", "op", "x", "y", "heading",
        "battery_v", "charge_v",
        "gps_sol", "gps_text", "gps_lat", "gps_lon",
        "bumper_l", "bumper_r", "motor_err", "uptime_s"
    };
    for (const auto& key : required) {
        INFO("Checking key: " << key);
        REQUIRE(j.contains(key));
    }
}

TEST_CASE("WebSocketServer: buildTelemetryJson — op names pass through", "[ws]") {
    // All frozen Op names used by Mission Service
    for (const auto& opName : {"Idle", "Mow", "Dock", "Charge",
                                "EscapeReverse", "GpsWait", "Error"}) {
        WebSocketServer::TelemetryData d;
        d.op = opName;
        auto j = nlohmann::json::parse(WebSocketServer::buildTelemetryJson(d));
        REQUIRE(j["op"] == opName);
    }
}

TEST_CASE("WebSocketServer: buildTelemetryJson — default data is valid JSON", "[ws]") {
    WebSocketServer::TelemetryData d;  // all defaults
    REQUIRE_NOTHROW(nlohmann::json::parse(WebSocketServer::buildTelemetryJson(d)));
}

// ── API surface ───────────────────────────────────────────────────────────────

TEST_CASE("WebSocketServer: constructor/destructor — no crash without start()", "[ws]") {
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    REQUIRE_NOTHROW(WebSocketServer ws(config, logger));
}

TEST_CASE("WebSocketServer: pushTelemetry — no crash when not running", "[ws]") {
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    WebSocketServer::TelemetryData d;
    d.op = "Idle";
    REQUIRE_NOTHROW(ws.pushTelemetry(d));
}

TEST_CASE("WebSocketServer: onCommand — callback registered without crash", "[ws]") {
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    bool called = false;
    ws.onCommand([&called](const std::string&, const nlohmann::json&) {
        called = true;
    });
    // Callback is registered; it will be invoked when a WS message arrives.
    // We cannot invoke it here without a real connection — just verify no crash.
    REQUIRE_FALSE(called);
}

TEST_CASE("WebSocketServer: isRunning — false before start()", "[ws]") {
    auto config = std::make_shared<sunray::Config>("/nonexistent/config.json");
    auto logger = std::make_shared<sunray::NullLogger>();
    WebSocketServer ws(config, logger);
    REQUIRE_FALSE(ws.isRunning());
}
