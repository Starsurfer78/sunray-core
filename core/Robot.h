#pragma once
// Robot.h — Main robot class for Sunray Core.
//
// Dependency-injected: receives HardwareInterface, Config, and Logger via
// constructor. No global state, no Singleton pattern.
//
// Lifecycle:
//   1. Construct with DI dependencies.
//   2. Call init() once — opens hardware, sets initial LED state.
//      Returns false on hardware failure (caller should abort).
//   3. Call loop() to run the control loop until stop() is called, or
//      call run() manually for single-step testing.
//
// Control loop cadence: LOOP_PERIOD_MS (20 ms → 50 Hz nominal).
// The hardware driver (hw_->run()) handles its own internal timing for
// AT+M/S/V frames independently of this period.
//
// Op state machine (A.4) and navigation (A.5) will be added as sub-objects
// injected or constructed here. Their call sites are already stubbed below.
//
// Thread safety: run() and stop() may be called from different threads.
// All other methods are not thread-safe.

#include "hal/HardwareInterface.h"
#include "hal/GpsDriver/GpsDriver.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Schedule.h"
#include "core/WebSocketServer.h"
#include "core/op/Op.h"
#include "core/navigation/StateEstimator.h"
#include "core/navigation/Map.h"
#include "core/navigation/LineTracker.h"

#include <filesystem>
#include <memory>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace sunray {

class MqttClient;  // forward declaration — full header included only in Robot.cpp

// ── Robot ─────────────────────────────────────────────────────────────────────

class Robot {
public:
    // ── Construction ──────────────────────────────────────────────────────────

    /// Construct with injected dependencies.
    /// hw:     ownership transferred (unique_ptr). Must not be nullptr.
    /// config: shared ownership (shared_ptr). Must not be nullptr.
    /// logger: shared ownership (shared_ptr). Must not be nullptr.
    Robot(std::unique_ptr<HardwareInterface> hw,
          std::shared_ptr<Config>            config,
          std::shared_ptr<Logger>            logger);

    ~Robot() = default;

    // Non-copyable, non-movable (owns hardware).
    Robot(const Robot&)            = delete;
    Robot& operator=(const Robot&) = delete;
    Robot(Robot&&)                 = delete;
    Robot& operator=(Robot&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Initialise hardware. Must be called once before loop() or run().
    /// Returns false if hw_->init() fails — caller should abort.
    bool init();

    /// Run the control loop synchronously until stop() is called.
    /// Sleeps for the remainder of LOOP_PERIOD_MS after each run() call to
    /// maintain ~50 Hz cadence without busy-waiting.
    void loop();

    /// Execute exactly one control-loop iteration (sensor read, state update,
    /// actuator output). Exposed for testing and single-step operation.
    void run();

    /// Request a graceful shutdown. Sets the running_ flag to false, causing
    /// loop() to return after the current run() call completes.
    /// Safe to call from a signal handler or another thread.
    void stop();

    // ── State accessors (read-only, for WebSocket telemetry etc.) ─────────────

    bool          isRunning()      const { return running_.load(); }
    unsigned long controlLoops()  const { return controlLoops_; }

    /// Active Op name for telemetry (e.g. "Mow", "Idle", "Charge").
    std::string   activeOpName()  const;

    /// Last sensor snapshot (updated each run() call).
    OdometryData  lastOdometry()  const;
    SensorData    lastSensors()   const;
    BatteryData   lastBattery()   const;

    // ── Commands (from WebSocket server / mission service) ────────────────────

    /// Operator commands — dispatched to OpManager::changeOperationTypeByOperator.
    void startMowing();
    void startDocking();
    void emergencyStop();

    /// Start mowing only the specified zones (C.12).
    /// zoneIds: list of zone IDs from the map JSON. Empty = all zones (same as startMowing()).
    void startMowingZones(const std::vector<std::string>& zoneIds);

    /// Access OpManager for direct command dispatch (e.g. from WebSocket server).
    OpManager& opManager() { return opMgr_; }

    /// Optional: attach a WebSocket server to receive telemetry pushes.
    /// Call before loop(). Does NOT transfer ownership.
    void setWebSocketServer(WebSocketServer* ws) { ws_ = ws; }

    /// Optional: attach an MQTT client to receive telemetry pushes.
    /// Call before loop(). Does NOT transfer ownership.
    void setMqttClient(MqttClient* mqtt) { mqtt_ = mqtt; }

    /// Optional: attach a GPS driver. Ownership transferred.
    /// Call before loop(). GPS data is polled each run() cycle.
    void setGpsDriver(std::unique_ptr<GpsDriver> gps) { gps_ = std::move(gps); }

    /// Load map from JSON file. Returns true on success.
    /// Call before startMowing() / startDocking().
    bool loadMap(const std::filesystem::path& path);

    /// Load schedule from JSON file (C.11). Returns true on success.
    bool loadSchedule(const std::filesystem::path& path);

    /// Replace schedule entries from JSON (C.11). Returns true on success.
    bool setSchedule(const nlohmann::json& arr);

    /// Return current schedule as JSON array (C.11).
    nlohmann::json getSchedule() const;

    /// Direct pose override (e.g. from AT+P command / Mission Service setpos).
    void setPose(float x, float y, float heading);

    /// Manual drive command from joystick (WebSocket thread-safe).
    /// linear: [-1..1] forward/backward, angular: [-1..1] left/right.
    /// Only applied in Idle state. Auto-stops if no command within 500 ms.
    void manualDrive(float linear, float angular);

    // ── Diagnostic commands (C.10b) ───────────────────────────────────────────

    /// Spin one motor at pwm until the given number of revolutions are completed,
    /// or for duration_ms if revolutions == 0 (time-based fallback).
    /// motor = "left" | "right". Blocks until done or 15 s safety timeout.
    /// Returns JSON: {"ok":true,"ticks":N,"ticks_target":T,"ticks_per_rev_config":C}.
    /// Only allowed in Idle state; returns error otherwise.
    nlohmann::json diagRunMotor(const std::string& motor, float pwm,
                                unsigned duration_ms, int revolutions = 0);

    /// Switch the mow motor on/off without leaving Idle state (C.10b).
    /// Returns JSON: {"ok":true} or {"ok":false,"error":"..."}.
    nlohmann::json diagMowMotor(bool on);

    /// Drive straight for distance_m at low speed, return tick totals (C.10b).
    nlohmann::json diagDriveStraight(float distance_m);

    /// Start IMU calibration (gyro bias estimation). Blocks for ~5 seconds.
    nlohmann::json diagImuCalib();

    /// Current estimated pose (from StateEstimator).
    float poseX()       const { return stateEst_.x(); }
    float poseY()       const { return stateEst_.y(); }
    float poseHeading() const { return stateEst_.heading(); }

private:
    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Update LED_2 (status) and LED_3 (GPS) from active Op name.
    void updateStatusLeds();

    /// Check battery voltage; fires onBatteryLow/Undervoltage events on active Op.
    void checkBattery(OpContext& ctx);
    /// Monitor GPS quality/fix age and fire resilience events with hysteresis.
    void monitorGpsResilience(OpContext& ctx);
    void armMissionResumeGuard();
    std::string currentStatePhase() const;
    std::string currentResumeTarget() const;
    static std::string computeMapFingerprint(const std::filesystem::path& path);

    // ── Dependencies ──────────────────────────────────────────────────────────

    std::unique_ptr<HardwareInterface> hw_;
    std::shared_ptr<Config>            config_;
    std::shared_ptr<Logger>            logger_;
    OpManager                          opMgr_;

    // ── Navigation (A.5) ──────────────────────────────────────────────────────
    nav::StateEstimator stateEst_;    ///< odometry dead-reckoning + GPS fusion stub
    nav::Map            map_;         ///< waypoint polygons (loaded via loadMap())
    nav::LineTracker    lineTracker_; ///< Stanley controller

    // ── Runtime state ─────────────────────────────────────────────────────────

    WebSocketServer*  ws_   = nullptr;  ///< optional, not owned
    MqttClient*       mqtt_ = nullptr;  ///< optional, not owned
    std::unique_ptr<GpsDriver> gps_;  ///< optional GPS driver (owned)

    GpsData       lastGps_;             ///< last GPS snapshot from gps_->getData()
    ImuData       lastImu_;             ///< last IMU snapshot from hw_->readImu()
    std::string   lastNmeaGGA_;         ///< last NMEA GGA line forwarded to WebSocket
    unsigned long gpsLastFixTime_ms_      = 0;  ///< now_ms_ when last valid GPS fix arrived (BUG-006)
    bool          gpsNoSignalLatched_     = false;
    bool          gpsFixTimeoutLatched_   = false;
    unsigned long gpsRecoveryStart_ms_    = 0;  ///< set when signal first comes back
    unsigned long lastObstacleCleanup_ms_ = 0;  ///< last cleanupExpiredObstacles() call (C.7)
    unsigned long lastScheduleCheck_ms_   = 0;  ///< last timetable check timestamp (C.11)
    bool          scheduleWasActive_      = false;  ///< previous timetable state (C.11)

    Schedule      schedule_;              ///< weekly mowing timetable (C.11)
    std::filesystem::path schedulePath_; ///< path for schedule persistence
    std::string currentMapFingerprint_;
    std::string activeMissionMapFingerprint_;
    bool        resumeBlockedByMapChange_ = false;

    std::atomic<bool>     running_{false};
    unsigned long         controlLoops_ = 0;

    // Manual drive (set from WebSocket thread, read in run())
    // Stored as int × 1000 for lock-free atomic access on all platforms.
    std::atomic<int>      manualLinear1000_{0};
    std::atomic<int>      manualAngular1000_{0};
    std::atomic<uint64_t> manualDriveTs_ms_{0};

    OdometryData odometry_;
    SensorData   sensors_;
    SensorData   previousSensors_;  ///< for edge-detection of safety events
    BatteryData  battery_;

    unsigned long                          now_ms_    = 0;
    std::chrono::steady_clock::time_point  startTime_;

    // ── Diagnostic motor test state (C.10b) ───────────────────────────────────
    // Scheduled from HTTP handler thread; executed in run(); result signalled back.

    struct DiagReq {
        std::string  motor;           ///< "left" | "right" | "mow" | "both"
        float        pwm       = 0.1f;
        unsigned     duration_ms = 3000;
        int          ticksTarget = 0; ///< 0 = time-based; >0 = stop after this many ticks
        bool         active    = false;
        unsigned long startMs  = 0;
        long         accTicks  = 0;   ///< accumulated encoder ticks during test
        bool         done      = false;
        std::string  resultJson;
    };
    mutable std::mutex      diagMutex_;
    std::condition_variable diagCv_;
    DiagReq                 diagReq_;

    // ── Constants ─────────────────────────────────────────────────────────────

    /// Build a telemetry snapshot from current robot state.
    WebSocketServer::TelemetryData buildTelemetry() const;

    /// Nominal control-loop period — 50 Hz.
    static constexpr int LOOP_PERIOD_MS = 20;

    /// Log tag used in all Logger calls.
    static constexpr const char* TAG = "Robot";
};

} // namespace sunray
