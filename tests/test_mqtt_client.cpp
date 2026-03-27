// test_mqtt_client.cpp — Unit tests for MqttClient.
//
// Does NOT connect to a real broker (no network required).
// Tests cover:
//   - Construction and destruction (no crash)
//   - start() with mqtt_enabled=false is a no-op (no running_ flag set)
//   - pushTelemetry() is safe to call when stopped
//   - onCommand() can be registered without crashing
//   - Config keys are read correctly

#include "core/MqttClient.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/WebSocketServer.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <memory>

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::shared_ptr<sunray::Config> makeConfig(bool enabled = false,
                                                   const std::string& host = "localhost") {
    auto cfg = std::make_shared<sunray::Config>("/nonexistent/config.json");
    cfg->set("mqtt_enabled",      enabled);
    cfg->set("mqtt_host",         host);
    cfg->set("mqtt_port",         1883);
    cfg->set("mqtt_topic_prefix", "sunray_test");
    return cfg;
}

static std::shared_ptr<sunray::Logger> makeLogger() {
    return std::make_shared<sunray::NullLogger>();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("MqttClient: construct and destruct without crash", "[mqtt]") {
    auto client = std::make_unique<sunray::MqttClient>(makeConfig(), makeLogger());
    // Destructor must not crash even without start()
    REQUIRE_FALSE(client->isRunning());
}

TEST_CASE("MqttClient: start() with mqtt_enabled=false — no-op", "[mqtt]") {
    sunray::MqttClient client(makeConfig(false), makeLogger());
    client.start();
    // Disabled → must not be running
    REQUIRE_FALSE(client.isRunning());
    client.stop();  // safe to call when not running
}

TEST_CASE("MqttClient: stop() without start() — no crash", "[mqtt]") {
    sunray::MqttClient client(makeConfig(), makeLogger());
    client.stop();  // must not crash
    REQUIRE_FALSE(client.isRunning());
}

TEST_CASE("MqttClient: pushTelemetry() safe when stopped", "[mqtt]") {
    sunray::MqttClient client(makeConfig(), makeLogger());
    sunray::WebSocketServer::TelemetryData td;
    td.op        = "Mow";
    td.battery_v = 25.4f;
    td.gps_text  = "RTK Fix";
    // Must not crash even though client is not running
    REQUIRE_NOTHROW(client.pushTelemetry(td));
}

TEST_CASE("MqttClient: onCommand() registration — no crash", "[mqtt]") {
    sunray::MqttClient client(makeConfig(), makeLogger());
    bool called = false;
    client.onCommand([&called](const std::string& cmd, const nlohmann::json&) {
        called = (cmd == "start");
    });
    REQUIRE_FALSE(called);  // callback not triggered without a broker message
}

TEST_CASE("MqttClient: config keys read from Config", "[mqtt]") {
    auto cfg = makeConfig(false, "mybroker.local");
    cfg->set("mqtt_port",         8883);
    cfg->set("mqtt_topic_prefix", "robot1");
    cfg->set("mqtt_keepalive_s",  30);

    // Just verify Config::get() returns the expected values
    REQUIRE(cfg->get<std::string>("mqtt_host",         "") == "mybroker.local");
    REQUIRE(cfg->get<int>        ("mqtt_port",         0)  == 8883);
    REQUIRE(cfg->get<std::string>("mqtt_topic_prefix", "") == "robot1");
    REQUIRE(cfg->get<int>        ("mqtt_keepalive_s",  0)  == 30);
    REQUIRE(cfg->get<bool>       ("mqtt_enabled",   true)  == false);
}

TEST_CASE("MqttClient: multiple stop() calls are safe", "[mqtt]") {
    sunray::MqttClient client(makeConfig(), makeLogger());
    client.stop();
    client.stop();  // idempotent
    REQUIRE_FALSE(client.isRunning());
}
