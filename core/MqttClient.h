#pragma once
// MqttClient.h — MQTT publisher/subscriber for robot telemetry and commands.
//
// When mqtt_enabled=true in config, connects to an MQTT broker and:
//
// Publishes at 10 Hz to {prefix}/<topic>:
//   state    — full telemetry JSON (same fields as WebSocket)
//   op       — active op name string        (e.g. "Mow")
//   gps/sol  — GPS solution text            (e.g. "RTK Fix")
//   gps/pos  — "lat,lon"                    (0.0,0.0 until GPS available)
//
// Subscribes:
//   {prefix}/cmd  — "start" | "stop" | "dock" | "charge"
//
// Config keys:
//   mqtt_enabled, mqtt_host, mqtt_port, mqtt_keepalive_s,
//   mqtt_topic_prefix, mqtt_user, mqtt_pass
//
// Requires libmosquitto on the build host (apt install libmosquitto-dev).
// If SUNRAY_HAVE_MOSQUITTO is not defined at compile time all methods are
// no-op stubs — the class still exists, links, and is safe to construct.

#include "core/Config.h"
#include "core/Logger.h"
#include "core/WebSocketServer.h"   // for TelemetryData

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace sunray {

class MqttClient {
public:
    using CommandCallback = std::function<void(const std::string& cmd,
                                               const nlohmann::json& params)>;

    explicit MqttClient(std::shared_ptr<Config> config,
                        std::shared_ptr<Logger> logger);
    ~MqttClient();

    MqttClient(const MqttClient&)            = delete;
    MqttClient& operator=(const MqttClient&) = delete;
    MqttClient(MqttClient&&)                 = delete;
    MqttClient& operator=(MqttClient&&)      = delete;

    /// Start MQTT connection + 10 Hz publish thread.
    /// No-op if mqtt_enabled=false or libmosquitto not available.
    void start();

    /// Graceful shutdown: stop push thread, disconnect, join.
    void stop();

    bool isRunning() const { return running_.load(); }

    /// Store latest telemetry snapshot for the next publish cycle.
    /// Thread-safe — called from Robot at 50 Hz.
    void pushTelemetry(const WebSocketServer::TelemetryData& data);

    /// Register callback invoked when a command arrives on {prefix}/cmd.
    /// Callback is called from the mosquitto network thread — must be thread-safe.
    void onCommand(CommandCallback cb);

private:
    std::shared_ptr<Config> config_;
    std::shared_ptr<Logger> logger_;
    std::atomic<bool>       running_{false};

    struct Impl;
    std::unique_ptr<Impl> impl_;

    static constexpr const char* TAG              = "MqttClient";
    static constexpr int         PUSH_INTERVAL_MS = 100;  // 10 Hz
};

} // namespace sunray
