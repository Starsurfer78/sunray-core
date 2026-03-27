// MqttClient.cpp — MQTT client implementation.
//
// Uses libmosquitto when SUNRAY_HAVE_MOSQUITTO is defined (set by CMake if
// libmosquitto is found via pkg-config).  Otherwise every method is a no-op
// stub so the binary still links and runs without an MQTT broker.
//
// Threading model:
//   mosquitto_loop_start() owns its own network I/O thread (reconnect included).
//   A second push thread publishes the latest telemetry snapshot at 10 Hz.
//   Both threads call mosquitto_publish() which is thread-safe under loop_start().

#include "core/MqttClient.h"

#ifdef SUNRAY_HAVE_MOSQUITTO
#include <mosquitto.h>
#endif

#include <chrono>
#include <mutex>
#include <thread>

namespace sunray {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct MqttClient::Impl {
    // Connection parameters (set in start())
    std::string host      = "localhost";
    int         port      = 1883;
    int         keepalive = 60;
    std::string prefix    = "sunray";
    int         qos       = 0;

    // Telemetry snapshot (producer: pushTelemetry / consumer: push thread)
    std::mutex                     telMutex;
    WebSocketServer::TelemetryData latestTel;
    bool                           hasTel = false;

    // Command callback (set by onCommand(), called from mosquitto message cb)
    std::mutex              cmdMutex;
    MqttClient::CommandCallback onCmd;

    // Push thread
    std::thread pushThread;

    // Back-pointer for callbacks (set in start() before loop_start)
    std::atomic<bool>* running = nullptr;
    Logger*            log     = nullptr;

#ifdef SUNRAY_HAVE_MOSQUITTO
    struct mosquitto* mosq = nullptr;

    // ── Static mosquitto callbacks ─────────────────────────────────────────

    static void onConnectCb(struct mosquitto* /*mosq*/, void* ud, int rc) {
        auto* impl = static_cast<Impl*>(ud);
        if (rc == MOSQ_ERR_SUCCESS) {
            impl->log->info("MqttClient",
                "Connected to " + impl->host + ":" + std::to_string(impl->port)
                + " prefix=" + impl->prefix);
            const std::string cmdTopic = impl->prefix + "/cmd";
            mosquitto_subscribe(impl->mosq, nullptr, cmdTopic.c_str(), impl->qos);
        } else {
            impl->log->warn("MqttClient",
                "Connect failed rc=" + std::to_string(rc)
                + " (" + mosquitto_strerror(rc) + ") — will retry");
        }
    }

    static void onDisconnectCb(struct mosquitto* /*mosq*/, void* ud, int rc) {
        auto* impl = static_cast<Impl*>(ud);
        if (rc != 0 && impl->running && impl->running->load()) {
            impl->log->warn("MqttClient",
                "Disconnected unexpectedly (rc=" + std::to_string(rc)
                + ") — reconnecting");
        }
    }

    static void onMessageCb(struct mosquitto* /*mosq*/, void* ud,
                             const struct mosquitto_message* msg) {
        if (!msg || !msg->payload || msg->payloadlen <= 0) return;
        auto* impl = static_cast<Impl*>(ud);
        const std::string payload(static_cast<const char*>(msg->payload),
                                  static_cast<size_t>(msg->payloadlen));
        std::lock_guard<std::mutex> lk(impl->cmdMutex);
        if (impl->onCmd) {
            impl->onCmd(payload, nlohmann::json{});
        }
    }

    // ── Publish helper ────────────────────────────────────────────────────

    void publish(const std::string& subtopic, const std::string& payload) {
        if (!mosq) return;
        const std::string topic = prefix + "/" + subtopic;
        mosquitto_publish(mosq, nullptr, topic.c_str(),
                          static_cast<int>(payload.size()),
                          payload.c_str(), qos, /*retain=*/false);
    }
#endif // SUNRAY_HAVE_MOSQUITTO
};

// ── Construction ──────────────────────────────────────────────────────────────

MqttClient::MqttClient(std::shared_ptr<Config> config,
                       std::shared_ptr<Logger> logger)
    : config_(std::move(config))
    , logger_(std::move(logger))
    , impl_(std::make_unique<Impl>())
{}

MqttClient::~MqttClient() {
    stop();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void MqttClient::start() {
    if (!config_->get<bool>("mqtt_enabled", false)) {
        logger_->info(TAG, "MQTT disabled (mqtt_enabled=false)");
        return;
    }

#ifndef SUNRAY_HAVE_MOSQUITTO
    logger_->warn(TAG,
        "MQTT not available — libmosquitto was not found at build time. "
        "On Raspberry Pi: sudo apt install libmosquitto-dev, then rebuild.");
    return;
#else
    impl_->host      = config_->get<std::string>("mqtt_host",         "localhost");
    impl_->port      = config_->get<int>        ("mqtt_port",         1883);
    impl_->keepalive = config_->get<int>        ("mqtt_keepalive_s",  60);
    impl_->prefix    = config_->get<std::string>("mqtt_topic_prefix", "sunray");
    impl_->running   = &running_;
    impl_->log       = logger_.get();

    mosquitto_lib_init();

    impl_->mosq = mosquitto_new(/*id=*/nullptr, /*clean_session=*/true,
                                /*userdata=*/impl_.get());
    if (!impl_->mosq) {
        logger_->error(TAG, "mosquitto_new() failed — out of memory?");
        mosquitto_lib_cleanup();
        return;
    }

    const std::string user = config_->get<std::string>("mqtt_user", "");
    const std::string pass = config_->get<std::string>("mqtt_pass", "");
    if (!user.empty()) {
        mosquitto_username_pw_set(impl_->mosq, user.c_str(), pass.c_str());
    }

    mosquitto_connect_callback_set(   impl_->mosq, Impl::onConnectCb);
    mosquitto_disconnect_callback_set(impl_->mosq, Impl::onDisconnectCb);
    mosquitto_message_callback_set(   impl_->mosq, Impl::onMessageCb);

    // Exponential backoff reconnect: 1 s → 30 s
    mosquitto_reconnect_delay_set(impl_->mosq, 1, 30, /*exponential=*/true);

    const int rc = mosquitto_connect_async(impl_->mosq,
                                           impl_->host.c_str(),
                                           impl_->port,
                                           impl_->keepalive);
    if (rc != MOSQ_ERR_SUCCESS) {
        logger_->warn(TAG,
            "Initial connect attempt: " + std::string(mosquitto_strerror(rc))
            + " — will retry in background");
    }

    // Start network I/O thread (handles connect, keepalive, reconnect)
    mosquitto_loop_start(impl_->mosq);
    running_.store(true);

    // Start 10 Hz publish thread
    impl_->pushThread = std::thread([this] {
        while (running_.load()) {
            WebSocketServer::TelemetryData td;
            bool hasTel = false;
            {
                std::lock_guard<std::mutex> lk(impl_->telMutex);
                td     = impl_->latestTel;
                hasTel = impl_->hasTel;
            }
            if (hasTel) {
                const std::string stateJson =
                    WebSocketServer::buildTelemetryJson(td);
                impl_->publish("state",   stateJson);
                impl_->publish("op",      td.op);
                impl_->publish("gps/sol", td.gps_text);
                impl_->publish("gps/pos",
                    std::to_string(td.gps_lat) + ","
                    + std::to_string(td.gps_lon));
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PUSH_INTERVAL_MS));
        }
    });

    logger_->info(TAG, "Started");
#endif // SUNRAY_HAVE_MOSQUITTO
}

void MqttClient::stop() {
    if (!running_.exchange(false)) return;  // already stopped / never started

#ifdef SUNRAY_HAVE_MOSQUITTO
    if (impl_->pushThread.joinable()) impl_->pushThread.join();

    if (impl_->mosq) {
        mosquitto_disconnect(impl_->mosq);
        mosquitto_loop_stop(impl_->mosq, /*force=*/true);
        mosquitto_destroy(impl_->mosq);
        impl_->mosq = nullptr;
    }
    mosquitto_lib_cleanup();
#endif
}

// ── Data feed ─────────────────────────────────────────────────────────────────

void MqttClient::pushTelemetry(const WebSocketServer::TelemetryData& data) {
    std::lock_guard<std::mutex> lk(impl_->telMutex);
    impl_->latestTel = data;
    impl_->hasTel    = true;
}

// ── Command registration ──────────────────────────────────────────────────────

void MqttClient::onCommand(CommandCallback cb) {
    std::lock_guard<std::mutex> lk(impl_->cmdMutex);
    impl_->onCmd = std::move(cb);
}

} // namespace sunray
