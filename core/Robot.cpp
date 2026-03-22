// Robot.cpp — Main robot class implementation.
//
// Control loop overview (one run() call):
//   1. hw_->run()         — driver tick (AT+M/S/V frames, LED, fan, WiFi poll)
//   2. readOdometry/Sensors/Battery — sensor snapshot
//   3. Build OpContext    — populate now_ms, GPS stubs
//   4. checkBattery()     — low-voltage → dock/shutdown events
//   5. opMgr_.tick(ctx)   — checkStop + Op::run()
//   6. updateStatusLeds() — LED_2 from active op name
//   7. ++controlLoops_

#include "core/Robot.h"

#include <filesystem>
#include <thread>
#include <chrono>
#include <stdexcept>

namespace sunray {

// ── Construction ───────────────────────────────────────────────────────────────

Robot::Robot(std::unique_ptr<HardwareInterface> hw,
             std::shared_ptr<Config>            config,
             std::shared_ptr<Logger>            logger)
    : hw_         (std::move(hw))
    , config_     (std::move(config))
    , logger_     (std::move(logger))
    , startTime_  (std::chrono::steady_clock::now())
    , stateEst_   (config_)
    , map_        (config_)
    , lineTracker_(config_)
{
    if (!hw_)     throw std::invalid_argument("Robot: hw must not be nullptr");
    if (!config_) throw std::invalid_argument("Robot: config must not be nullptr");
    if (!logger_) throw std::invalid_argument("Robot: logger must not be nullptr");
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

bool Robot::init() {
    logger_->info(TAG, "Initialising hardware...");

    if (!hw_->init()) {
        logger_->error(TAG, "Hardware init failed — aborting");
        return false;
    }

    hw_->setLed(LedId::LED_1, LedState::OFF);
    hw_->setLed(LedId::LED_2, LedState::OFF);
    hw_->setLed(LedId::LED_3, LedState::OFF);

    running_.store(false);
    now_ms_  = 0;

    logger_->info(TAG, "Robot id: " + hw_->getRobotId());
    logger_->info(TAG, "Init complete — entering IDLE");
    return true;
}

void Robot::loop() {
    running_.store(true);

    while (running_.load()) {
        auto start = std::chrono::steady_clock::now();

        run();

        auto elapsed   = std::chrono::steady_clock::now() - start;
        auto remaining = std::chrono::milliseconds(LOOP_PERIOD_MS) - elapsed;
        if (remaining > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(remaining);
        }
    }

    logger_->info(TAG, "Loop exited — shutting down hardware");
    hw_->setMotorPwm(0, 0, 0);
    hw_->keepPowerOn(false);
}

void Robot::run() {
    // ── 1. Monotonic clock ────────────────────────────────────────────────────
    const unsigned long prev_ms = now_ms_;
    now_ms_ = static_cast<unsigned long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count());
    const unsigned long dt_ms = now_ms_ - prev_ms;

    // ── 2. Driver tick ────────────────────────────────────────────────────────
    hw_->run();

    // ── 3. Sensor reads ───────────────────────────────────────────────────────
    odometry_ = hw_->readOdometry();
    sensors_  = hw_->readSensors();
    battery_  = hw_->readBattery();

    // ── 4. State estimation (odometry dead-reckoning) ─────────────────────────
    stateEst_.update(odometry_, dt_ms);
    // TODO (Phase 2): stateEst_.updateGps(gps.posE, gps.posN, gps.isFix, gps.isFloat);

    // ── 5. Build OpContext ────────────────────────────────────────────────────
    OpContext ctx{
        .hw       = *hw_,
        .config   = *config_,
        .logger   = *logger_,
        .opMgr    = opMgr_,
        .sensors  = sensors_,
        .battery  = battery_,
        .odometry = odometry_,
        // Navigation state from StateEstimator
        .x                = stateEst_.x(),
        .y                = stateEst_.y(),
        .heading          = stateEst_.heading(),
        .insidePerimeter  = map_.isLoaded()
                             ? map_.isInsideAllowedArea(stateEst_.x(), stateEst_.y())
                             : true,
        .isDockingRoute   = map_.isDocking(),
        .gpsHasFix        = stateEst_.gpsHasFix(),
        .gpsHasFloat      = stateEst_.gpsHasFloat(),
        .gpsFixAge_ms     = 9999999,   // TODO (Phase 2): real fix age
        .now_ms           = now_ms_,
        // Navigation objects
        .stateEst         = &stateEst_,
        .map              = &map_,
        .lineTracker      = &lineTracker_,
    };

    // ── 6. Battery guard ──────────────────────────────────────────────────────
    checkBattery(ctx);

    // ── 7. Op state machine tick ──────────────────────────────────────────────
    opMgr_.tick(ctx);

    // ── 8. Manual drive (joystick) — only in Idle, watchdog 500 ms ───────────
    {
        const uint64_t driveTs = manualDriveTs_ms_.load();
        const bool     inIdle  = (activeOpName() == "Idle");
        const bool     fresh   = (now_ms_ - driveTs < 500UL);
        if (inIdle && fresh && driveTs > 0) {
            const float lin = manualLinear1000_.load()  / 1000.f;
            const float ang = manualAngular1000_.load() / 1000.f;
            constexpr float MAX_PWM = 0.35f;  // 35 % max in manual mode
            float left  = (lin - ang * 0.5f) * MAX_PWM;
            float right = (lin + ang * 0.5f) * MAX_PWM;
            left  = left  < -1.f ? -1.f : left  > 1.f ? 1.f : left;
            right = right < -1.f ? -1.f : right > 1.f ? 1.f : right;
            hw_->setMotorPwm(left, right, 0);
        }
    }

    // ── 9. Safety: bumper/lift/motor fault → force motor stop ─────────────────
    if (sensors_.bumperLeft || sensors_.bumperRight || sensors_.lift || sensors_.motorFault) {
        // Let the active Op handle it via events (Op::onObstacle etc.).
        // But always guarantee motors stop immediately.
        hw_->setMotorPwm(0, 0, 0);
    }

    // ── 10. Status LEDs ───────────────────────────────────────────────────────
    updateStatusLeds();

    // ── 11. WebSocket telemetry push ──────────────────────────────────────────
    if (ws_) ws_->pushTelemetry(buildTelemetry());

    ++controlLoops_;
}

void Robot::stop() {
    running_.store(false);
}

// ── Manual drive (joystick) ────────────────────────────────────────────────────

void Robot::manualDrive(float linear, float angular) {
    // Clamp to [-1..1], store scaled as int for lock-free cross-thread access.
    auto clamp1 = [](float v) { return v < -1.f ? -1.f : v > 1.f ? 1.f : v; };
    manualLinear1000_.store(static_cast<int>(clamp1(linear)  * 1000.f));
    manualAngular1000_.store(static_cast<int>(clamp1(angular) * 1000.f));
    manualDriveTs_ms_.store(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count()));
}

// ── Commands ───────────────────────────────────────────────────────────────────

// Helper: build a minimal OpContext for operator commands.
// Navigation objects are included so that Op::begin() can start the map route.
static OpContext makeCommandCtx(HardwareInterface& hw, Config& config, Logger& logger,
                                OpManager& opMgr,
                                const SensorData& sensors, const BatteryData& battery,
                                const OdometryData& odometry, unsigned long now_ms,
                                float x, float y, float heading,
                                nav::StateEstimator* est, nav::Map* map,
                                nav::LineTracker* tracker)
{
    return OpContext{
        .hw              = hw,
        .config          = config,
        .logger          = logger,
        .opMgr           = opMgr,
        .sensors         = sensors,
        .battery         = battery,
        .odometry        = odometry,
        .x               = x,
        .y               = y,
        .heading         = heading,
        .insidePerimeter = true,
        .isDockingRoute  = false,
        .gpsHasFix       = est ? est->gpsHasFix()   : false,
        .gpsHasFloat     = est ? est->gpsHasFloat()  : false,
        .gpsFixAge_ms    = 9999999,
        .now_ms          = now_ms,
        .stateEst        = est,
        .map             = map,
        .lineTracker     = tracker,
    };
}

void Robot::startMowing() {
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Mow");
}

void Robot::startDocking() {
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Dock");
}

void Robot::emergencyStop() {
    hw_->setMotorPwm(0, 0, 0);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Idle");
    logger_->warn(TAG, "Emergency stop");
}

bool Robot::loadMap(const std::filesystem::path& path) {
    if (map_.load(path)) {
        logger_->info(TAG, "Map loaded: " + path.string());
        return true;
    }
    logger_->error(TAG, "Map load failed: " + path.string());
    return false;
}

void Robot::setPose(float x, float y, float heading) {
    stateEst_.setPose(x, y, heading);
}

std::string Robot::activeOpName() const {
    Op* op = opMgr_.activeOp();
    return op ? op->name() : "Idle";
}

// ── Accessors ─────────────────────────────────────────────────────────────────

OdometryData Robot::lastOdometry() const { return odometry_; }
SensorData   Robot::lastSensors()  const { return sensors_;  }
BatteryData  Robot::lastBattery()  const { return battery_;  }

// ── Private helpers ────────────────────────────────────────────────────────────

void Robot::updateStatusLeds() {
    const std::string opName = activeOpName();
    if (opName == "Error") {
        hw_->setLed(LedId::LED_2, LedState::RED);
    } else {
        hw_->setLed(LedId::LED_2, LedState::GREEN);
    }
    // LED_3: GPS quality (green = fix, red = no fix, off = no GPS)
    if (stateEst_.gpsHasFix()) {
        hw_->setLed(LedId::LED_3, LedState::GREEN);
    } else if (stateEst_.gpsHasFloat()) {
        hw_->setLed(LedId::LED_3, LedState::RED);
    } else {
        hw_->setLed(LedId::LED_3, LedState::OFF);
    }
}

WebSocketServer::TelemetryData Robot::buildTelemetry() const {
    WebSocketServer::TelemetryData d;
    d.op        = activeOpName();
    d.x         = stateEst_.x();
    d.y         = stateEst_.y();
    d.heading   = stateEst_.heading();
    d.battery_v = battery_.voltage;
    d.charge_v  = battery_.chargeVoltage;
    // GPS quality: derive from StateEstimator state
    if (stateEst_.gpsHasFix()) {
        d.gps_sol  = 4;
        d.gps_text = "RTK";
    } else if (stateEst_.gpsHasFloat()) {
        d.gps_sol  = 5;
        d.gps_text = "Float";
    } else {
        d.gps_sol  = 0;
        d.gps_text = "---";
    }
    // gps_lat/lon: Phase 2 — real WGS-84 coords from GPS receiver
    d.gps_lat   = 0.0;
    d.gps_lon   = 0.0;
    d.bumper_l  = sensors_.bumperLeft;
    d.bumper_r  = sensors_.bumperRight;
    d.motor_err = sensors_.motorFault;
    d.uptime_s  = now_ms_ / 1000UL;
    return d;
}

void Robot::checkBattery(OpContext& ctx) {
    const float lowV      = config_->get<float>("battery_low_v",      21.0f);
    const float criticalV = config_->get<float>("battery_critical_v", 20.0f);

    if (battery_.voltage < 0.1f) return;  // no MCU data yet
    if (battery_.chargerConnected) return; // charging — no guard needed

    if (battery_.voltage < criticalV) {
        logger_->error(TAG, "Battery critical (" + std::to_string(battery_.voltage) + "V)");
        hw_->setMotorPwm(0, 0, 0);
        hw_->setBuzzer(false);
        hw_->keepPowerOn(false);
        running_.store(false);
        opMgr_.activeOp()->onBatteryUndervoltage(ctx);
    } else if (battery_.voltage < lowV) {
        logger_->warn(TAG, "Battery low (" + std::to_string(battery_.voltage) + "V) — docking");
        opMgr_.activeOp()->onBatteryLowShouldDock(ctx);
    }
}

} // namespace sunray
