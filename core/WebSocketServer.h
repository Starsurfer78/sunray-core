#pragma once
// WebSocketServer.h — Crow-based WebSocket + HTTP server.
//
// WebSocket endpoint: ws://host/ws/telemetry
// Push:     10 Hz (100 ms interval), or keepalive {"type":"ping"} if no new data
// Format:   see buildTelemetryJson() — byte-for-byte identical to
//           sunray/mission_api.cpp:254-274 (frozen format, frontend parses it)
//
// WebSocket commands received (from frontend / Mission Service):
//   {"cmd":"start"}                   → onCommand("start",  {})
//   {"cmd":"stop"}                    → onCommand("stop",   {})
//   {"cmd":"dock"}                    → onCommand("dock",   {})
//   {"cmd":"charge"}                  → onCommand("charge", {})
//   {"cmd":"setpos","lon":f,"lat":f}  → onCommand("setpos", {lon, lat})
//
// HTTP endpoints:
//   GET  /                    → serves webRoot/index.html
//   GET  /assets/<path>       → serves webRoot/assets/<file>
//   GET  /api/config          → returns config JSON
//   PUT  /api/config          → updates config keys, saves to disk
//   POST /api/sim/<action>    → sim commands (bumper/gps/lift) via onSimCommand()
//
// Threading:
//   Robot control loop calls pushTelemetry() at 50 Hz (thread A).
//   Crow I/O runs in its own thread pool (thread B).
//   A background push thread broadcasts at 10 Hz (thread C).
//   All shared state is protected by mutexes.
//
// Pimpl: Crow headers are included only in WebSocketServer.cpp — not here.

#include "core/Config.h"
#include "core/Logger.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace sunray {

class WebSocketServer {
public:
    // ── Telemetry snapshot ────────────────────────────────────────────────────
    // Filled by Robot::run() every 20 ms; broadcast every 100 ms.

    struct TelemetryData {
        std::string  op        = "Idle";  ///< active Op name (frozen set)
        float        x         = 0.0f;   ///< local east  (m)
        float        y         = 0.0f;   ///< local north (m)
        float        heading   = 0.0f;   ///< heading (rad, 0 = east)
        float        battery_v = 0.0f;   ///< battery voltage (V)
        float        charge_v  = 0.0f;   ///< charger output voltage (V)
        int          gps_sol   = 0;      ///< NMEA quality (0=none, 4=RTK, 5=float)
        std::string  gps_text  = "---";  ///< human-readable GPS quality
        double       gps_lat   = 0.0;    ///< WGS-84 latitude  (Phase 2)
        double       gps_lon   = 0.0;    ///< WGS-84 longitude (Phase 2)
        bool         bumper_l  = false;
        bool         bumper_r  = false;
        bool         lift      = false;  ///< lift sensor (C.10)
        bool         motor_err = false;
        unsigned long uptime_s = 0;      ///< seconds since robot start
        bool         diag_active = false; ///< true while a diag motor test is running
        long         diag_ticks  = 0;     ///< accumulated encoder ticks in current diag test
        std::string  mcu_version = "";   ///< MCU firmware version (e.g. "rm18-v1.0")
        float        imu_heading = 0.0f; ///< integrated yaw [deg]
        float        imu_roll    = 0.0f; ///< roll [deg]
        float        imu_pitch   = 0.0f; ///< pitch [deg]
        std::string  ekf_health = "Odo"; ///< fusion mode: "EKF+GPS" | "EKF+IMU" | "Odo"
    };

    // ── Callbacks ─────────────────────────────────────────────────────────────

    using CommandCallback    = std::function<void(const std::string& cmd,
                                                  const nlohmann::json& params)>;
    using SimCommandCallback = std::function<void(const std::string& action,
                                                  const nlohmann::json& params)>;
    /// Diagnostic callback: invoked by POST /api/diag/<action>.
    /// Returns a JSON result that is sent back as the HTTP response body (C.10b).
    using DiagCallback       = std::function<nlohmann::json(const std::string& action,
                                                             const nlohmann::json& params)>;
    /// Schedule GET callback: returns current schedule as JSON array (C.11).
    using ScheduleGetCallback = std::function<nlohmann::json()>;
    /// Schedule PUT callback: receives new schedule JSON array, returns {"ok":true/false} (C.11).
    using SchedulePutCallback = std::function<nlohmann::json(const nlohmann::json&)>;

    // ── Construction ──────────────────────────────────────────────────────────

    explicit WebSocketServer(std::shared_ptr<Config> config,
                             std::shared_ptr<Logger> logger);
    ~WebSocketServer();

    // Non-copyable / non-movable (owns thread + network state)
    WebSocketServer(const WebSocketServer&)            = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    WebSocketServer(WebSocketServer&&)                 = delete;
    WebSocketServer& operator=(WebSocketServer&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Start Crow + push thread in background. Returns immediately.
    /// Port is read from config key "ws_port" (default 8765).
    void start();

    /// Graceful shutdown: stop push loop, stop Crow, join thread.
    void stop();

    bool isRunning() const { return running_.load(); }

    // ── Configuration ─────────────────────────────────────────────────────────

    /// Set directory from which GET / and GET /assets/* are served.
    /// If empty (default), no static routes are registered.
    /// Must be called before start().
    void setWebRoot(const std::string& distDir);

    // ── Data feed (called from Robot control-loop thread at 50 Hz) ────────────

    /// Thread-safe: stores the latest telemetry snapshot for the next broadcast.
    void pushTelemetry(const TelemetryData& data);

    /// Broadcast a raw NMEA sentence to all connected WebSocket clients.
    /// Sends {"type":"nmea","line":"<sentence>"} immediately.
    /// Thread-safe: may be called from any thread.
    void broadcastNmea(const std::string& line);

    // ── Command registration ──────────────────────────────────────────────────

    /// Register the callback invoked on each incoming WebSocket command.
    /// Called from Crow's I/O thread — the callback must be thread-safe.
    void onCommand(CommandCallback cb);

    /// Register the callback for POST /api/sim/<action> requests.
    /// Only active in --sim mode; not called if unregistered.
    void onSimCommand(SimCommandCallback cb);

    /// Register the callback for POST /api/diag/<action> requests (C.10b).
    /// Callback may block (runs in Crow HTTP thread). Returns JSON response body.
    void onDiag(DiagCallback cb);

    /// Register callbacks for GET/PUT /api/schedule (C.11).
    void onScheduleGet(ScheduleGetCallback cb);
    void onSchedulePut(SchedulePutCallback cb);

    // ── Map API ───────────────────────────────────────────────────────────────

    /// Set the path of the map JSON file served by GET /api/map and written
    /// by POST /api/map.  Must be called before start().
    void setMapPath(const std::string& mapPath);

    /// Register a callback invoked after POST /api/map saves the new map file.
    /// The callback should reload the map into the running Robot instance.
    /// Returns true on success (reported back to the HTTP caller).
    using MapReloadCallback = std::function<bool()>;
    void onMapReload(MapReloadCallback cb);

    // ── Testable helper ───────────────────────────────────────────────────────

    /// Serialize to the frozen telemetry JSON format (no trailing newline).
    /// Pure function — no side effects. Used by both the push loop and tests.
    static std::string buildTelemetryJson(const TelemetryData& data);

private:
    std::shared_ptr<Config> config_;
    std::shared_ptr<Logger> logger_;

    std::atomic<bool> running_{false};
    std::thread       serverThread_;

    std::string webRoot_;   ///< path to webui/dist (empty = no static serving)

    // Latest telemetry snapshot (written by pushTelemetry, read by push loop)
    mutable std::mutex telemetryMutex_;
    TelemetryData      latestTelemetry_;
    bool               hasNewTelemetry_ = false;

    // NMEA queue: broadcastNmea() enqueues here; only serverThread_ dequeues+sends
    // (BUG-005: prevents concurrent send_text() calls from Robot + push threads)
    std::mutex               nmeaMutex_;
    std::queue<std::string>  nmeaQueue_;

    // WebSocket command callback
    std::mutex      cmdMutex_;
    CommandCallback commandCallback_;

    // Simulator command callback (POST /api/sim/*)
    std::mutex         simCmdMutex_;
    SimCommandCallback simCommandCallback_;

    // Diagnostic callback (POST /api/diag/*) — C.10b
    std::mutex    diagCbMutex_;
    DiagCallback  diagCallback_;

    // Schedule callbacks (GET/PUT /api/schedule) — C.11
    std::mutex            schedCbMutex_;
    ScheduleGetCallback   schedGetCb_;
    SchedulePutCallback   schedPutCb_;

    // Map file path + reload callback (GET/POST /api/map)
    std::string      mapPath_;
    std::mutex       mapReloadMutex_;
    MapReloadCallback mapReloadCallback_;

    // Pimpl: hides all Crow types from this header
    struct Impl;
    std::unique_ptr<Impl> impl_;

    static constexpr const char* TAG              = "WsServer";
    static constexpr int         PUSH_INTERVAL_MS = 100;

    void setupHttpRoutes();
};

} // namespace sunray
