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
#include "core/MqttClient.h"

#include <filesystem>
#include <fstream>
#include <sstream>
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
    , stateEst_   (config_)
    , map_        (config_)
    , lineTracker_(config_)
    , startTime_  (std::chrono::steady_clock::now())
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

    running_.store(false);
    now_ms_  = 0;
    sensors_ = hw_->readSensors();
    previousSensors_ = sensors_;
    lastImu_ = {};

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
    try {
        const unsigned long dt_ms = tickTiming();
        tickHardware();
        tickSchedule();
        tickObstacleCleanup();
        tickStateEstimation(dt_ms);
        OpContext ctx = assembleOpContext();
        tickSafetyGuards(ctx);
        tickStateMachine(ctx);
        if (tickDiag()) return;
        tickButtonControl();
        tickManualDrive();
        tickSafetyStop(ctx);
        updateStatusLeds();
        tickTelemetry();
        ++controlLoops_;
    } catch (const std::exception& e) {
        try { hw_->setMotorPwm(0, 0, 0); } catch (...) {}
        running_.store(false);
        logger_->error(TAG, std::string("Unhandled exception in run(): ") + e.what());
    } catch (...) {
        try { hw_->setMotorPwm(0, 0, 0); } catch (...) {}
        running_.store(false);
        logger_->error(TAG, "Unhandled non-standard exception in run()");
    }
}

unsigned long Robot::tickTiming() {
    const unsigned long prev_ms = now_ms_;
    now_ms_ = static_cast<unsigned long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count());
    return now_ms_ - prev_ms;
}

void Robot::tickHardware() {
    hw_->run();

    odometry_ = hw_->readOdometry();
    sensors_  = hw_->readSensors();
    battery_  = hw_->readBattery();
    lastImu_  = hw_->readImu();
    if (lastImu_.valid) {
        stateEst_.updateImu(lastImu_.yaw, lastImu_.roll, lastImu_.pitch);
    }
}

void Robot::tickSchedule() {
    if (now_ms_ - lastScheduleCheck_ms_ < 60000UL) return;

    lastScheduleCheck_ms_ = now_ms_;
    const bool active = schedule_.isActiveNow();
    if (active == scheduleWasActive_) return;

    OpContext ctx{*hw_, *config_, *logger_, opMgr_};
    ctx.sensors = sensors_;
    ctx.battery = battery_;
    ctx.odometry = odometry_;
    ctx.x = stateEst_.x();
    ctx.y = stateEst_.y();
    ctx.heading = stateEst_.heading();
    ctx.insidePerimeter = true;
    ctx.isDockingRoute = false;
    ctx.gpsHasFix = stateEst_.gpsHasFix();
    ctx.gpsHasFloat = stateEst_.gpsHasFloat();
    ctx.gpsFixAge_ms = (gpsLastFixTime_ms_ == 0) ? now_ms_
                     : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    ctx.stateEst = &stateEst_;
    ctx.map = &map_;
    ctx.lineTracker = &lineTracker_;
    ctx.now_ms = now_ms_;
    if (active) {
        if (Op* op = opMgr_.activeOp()) op->onTimetableStartMowing(ctx);
    } else {
        if (Op* op = opMgr_.activeOp()) op->onTimetableStopMowing(ctx);
    }
    scheduleWasActive_ = active;
}

void Robot::tickObstacleCleanup() {
    if (now_ms_ - lastObstacleCleanup_ms_ < 1000UL) return;
    map_.cleanupExpiredObstacles(now_ms_);
    lastObstacleCleanup_ms_ = now_ms_;
}

void Robot::tickStateEstimation(unsigned long dt_ms) {
    stateEst_.update(odometry_, dt_ms);
    if (!gps_) return;

    lastGps_ = gps_->getData();
    if (lastGps_.valid) {
        stateEst_.updateGps(
            lastGps_.relPosE, lastGps_.relPosN,
            lastGps_.solution == GpsSolution::Fixed,
            lastGps_.solution == GpsSolution::Float);
        gpsLastFixTime_ms_ = now_ms_;
    }
    if (ws_ && !lastGps_.nmeaGGA.empty() && lastGps_.nmeaGGA != lastNmeaGGA_) {
        lastNmeaGGA_ = lastGps_.nmeaGGA;
        ws_->broadcastNmea(lastNmeaGGA_);
    }
}

OpContext Robot::assembleOpContext() {
    OpContext ctx{*hw_, *config_, *logger_, opMgr_};
    ctx.sensors = sensors_;
    ctx.battery = battery_;
    ctx.odometry = odometry_;
    ctx.x = stateEst_.x();
    ctx.y = stateEst_.y();
    ctx.heading = stateEst_.heading();
    ctx.insidePerimeter = map_.isLoaded()
                        ? map_.isInsideAllowedArea(stateEst_.x(), stateEst_.y())
                        : true;
    ctx.isDockingRoute = map_.isDocking();
    ctx.gpsHasFix = stateEst_.gpsHasFix();
    ctx.gpsHasFloat = stateEst_.gpsHasFloat();
    ctx.gpsFixAge_ms = (gpsLastFixTime_ms_ == 0) ? now_ms_
                    : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    ctx.resumeBlockedByMapChange = resumeBlockedByMapChange_;
    ctx.stateEst = &stateEst_;
    ctx.map = &map_;
    ctx.lineTracker = &lineTracker_;
    ctx.now_ms = now_ms_;
    return ctx;
}

void Robot::tickSafetyGuards(OpContext& ctx) {
    checkBattery(ctx);
    monitorGpsResilience(ctx);
}

void Robot::tickStateMachine(OpContext& ctx) {
    opMgr_.tick(ctx);
}

bool Robot::tickDiag() {
    std::lock_guard<std::mutex> lk(diagMutex_);
    if (!diagReq_.active || diagReq_.done) return false;

    if (diagReq_.startMs == 0) diagReq_.startMs = now_ms_;
    auto toPwm = [](float normalized) -> int {
        const float clamped = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
        return static_cast<int>(clamped * 255.0f);
    };
    const int pwm = toPwm(diagReq_.pwm);

    if      (diagReq_.motor == "left")  hw_->setMotorPwm(pwm, 0,   0);
    else if (diagReq_.motor == "right") hw_->setMotorPwm(0,   pwm, 0);
    else if (diagReq_.motor == "mow")   hw_->setMotorPwm(0,   0,   pwm);
    else if (diagReq_.motor == "both")  hw_->setMotorPwm(pwm, pwm, 0);

    if      (diagReq_.motor == "left")  diagReq_.accTicks += std::abs(odometry_.leftTicks);
    else if (diagReq_.motor == "right") diagReq_.accTicks += std::abs(odometry_.rightTicks);
    else if (diagReq_.motor == "mow")   diagReq_.accTicks += std::abs(odometry_.mowTicks);
    else if (diagReq_.motor == "both")  diagReq_.accTicks += (std::abs(odometry_.leftTicks) + std::abs(odometry_.rightTicks)) / 2;

    const bool timeDone    = (diagReq_.ticksTarget == 0)
                           && (now_ms_ - diagReq_.startMs >= diagReq_.duration_ms);
    const bool tickDone    = (diagReq_.ticksTarget > 0)
                           && (std::abs(diagReq_.accTicks) >= diagReq_.ticksTarget);
    const bool safeTimeout = (now_ms_ - diagReq_.startMs >= 15000u);
    if (timeDone || tickDone || safeTimeout) {
        hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
        const int cfgTpr = config_->get<int>("ticks_per_revolution",
                         config_->get<int>("ticks_per_rev", 120));
        nlohmann::json r;
        r["ok"]                   = true;
        r["ticks"]                = diagReq_.accTicks;
        r["ticks_target"]         = diagReq_.ticksTarget;
        r["ticks_per_rev_config"] = cfgTpr;
        diagReq_.resultJson = r.dump();
        diagReq_.done = true;
        diagCv_.notify_all();
    }
    return true;
}

void Robot::tickButtonControl() {
    // End short button-beep pulse.
    if (buttonBeepOffAt_ms_ != 0 && now_ms_ >= buttonBeepOffAt_ms_) {
        hw_->setBuzzer(false);
        buttonBeepOffAt_ms_ = 0;
    }

    const bool pressed = sensors_.stopButton;
    const std::string opName = activeOpName();
    const bool canRunHoldActions = (opName == "Idle" || opName == "Charge");

    if (pressed && !stopButtonPrev_ && !canRunHoldActions) {
        emergencyStop();
    }

    if (pressed) {
        if (!buttonHoldActive_) {
            buttonHoldStart_ms_ = now_ms_;
            buttonLastFeedback_s_ = 0;
            buttonHoldActive_ = true;
        }

        const unsigned heldSeconds = static_cast<unsigned>((now_ms_ - buttonHoldStart_ms_) / 1000UL);
        if (heldSeconds > buttonLastFeedback_s_) {
            buttonLastFeedback_s_ = heldSeconds;
            hw_->setBuzzer(true);
            buttonBeepOffAt_ms_ = now_ms_ + 100;
        }
    } else if (stopButtonPrev_ && buttonHoldActive_) {
        const unsigned heldSeconds = static_cast<unsigned>((now_ms_ - buttonHoldStart_ms_) / 1000UL);

        if (buttonBeepOffAt_ms_ != 0) {
            hw_->setBuzzer(false);
            buttonBeepOffAt_ms_ = 0;
        }

        if (heldSeconds >= 9) {
            emergencyStop();
            hw_->keepPowerOn(false);
            running_.store(false);
            logger_->warn(TAG, "Stop button long-press >=9s — shutdown requested");
        } else if (heldSeconds >= 6) {
            startMowing();
            logger_->info(TAG, "Stop button long-press >=6s — start mowing");
        } else if (heldSeconds >= 5) {
            startDocking();
            logger_->info(TAG, "Stop button long-press >=5s — start docking");
        } else if (heldSeconds >= 1) {
            emergencyStop();
            logger_->warn(TAG, "Stop button short-press >=1s — emergency stop");
        }

        buttonHoldStart_ms_ = 0;
        buttonLastFeedback_s_ = 0;
        buttonHoldActive_ = false;
    }

    stopButtonPrev_ = pressed;
}

void Robot::tickManualDrive() {
    const uint64_t driveTs = manualDriveTs_ms_.load();
    const bool     inIdle  = (activeOpName() == "Idle");
    const bool     fresh   = (now_ms_ - driveTs < 500UL);
    if (!(inIdle && fresh && driveTs > 0) || sensors_.stopButton) return;

    const float lin = manualLinear1000_.load()  / 1000.f;
    const float ang = manualAngular1000_.load() / 1000.f;
    constexpr float MAX_PWM = 0.35f;
    float left  = (lin - ang * 0.5f) * MAX_PWM;
    float right = (lin + ang * 0.5f) * MAX_PWM;
    left  = left  < -1.f ? -1.f : left  > 1.f ? 1.f : left;
    right = right < -1.f ? -1.f : right > 1.f ? 1.f : right;
    hw_->setMotorPwm(static_cast<int>(left * 255.0f),
                     static_cast<int>(right * 255.0f), 0);
}

void Robot::tickSafetyStop(OpContext& ctx) {
    if (sensors_.bumperLeft || sensors_.bumperRight || sensors_.lift || sensors_.motorFault) {
        hw_->setMotorPwm(0, 0, 0);

        if (Op* op = opMgr_.activeOp()) {
            if ((sensors_.bumperLeft  && !previousSensors_.bumperLeft) ||
                (sensors_.bumperRight && !previousSensors_.bumperRight)) {
                op->onObstacle(ctx);
            }
            if (sensors_.lift && !previousSensors_.lift) {
                op->onLiftTriggered(ctx);
            }
            if (sensors_.motorFault && !previousSensors_.motorFault) {
                op->onMotorError(ctx);
            }
            if (sensors_.rain && !previousSensors_.rain) {
                op->onRainTriggered(ctx);
            }
        }
    }
    previousSensors_ = sensors_;
}

void Robot::tickTelemetry() {
    const auto td = buildTelemetry();
    if (ws_)   ws_->pushTelemetry(td);
    if (mqtt_) mqtt_->pushTelemetry(td);
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
                                unsigned gpsFixAge_ms,
                                bool resumeBlockedByMapChange,
                                nav::StateEstimator* est, nav::Map* map,
                                nav::LineTracker* tracker)
{
    OpContext ctx{hw, config, logger, opMgr};
    ctx.sensors = sensors;
    ctx.battery = battery;
    ctx.odometry = odometry;
    ctx.x = x;
    ctx.y = y;
    ctx.heading = heading;
    ctx.insidePerimeter = true;
    ctx.isDockingRoute = false;
    ctx.gpsHasFix = est ? est->gpsHasFix() : false;
    ctx.gpsHasFloat = est ? est->gpsHasFloat() : false;
    ctx.gpsFixAge_ms = gpsFixAge_ms;
    ctx.resumeBlockedByMapChange = resumeBlockedByMapChange;
    ctx.stateEst = est;
    ctx.map = map;
    ctx.lineTracker = tracker;
    ctx.now_ms = now_ms;
    return ctx;
}

void Robot::startMowing() {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, resumeBlockedByMapChange_, &stateEst_, &map_, &lineTracker_);
    armMissionResumeGuard();
    opMgr_.changeOperationTypeByOperator(ctx, "Mow");
}

void Robot::startMowingZones(const std::vector<std::string>& zoneIds) {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, resumeBlockedByMapChange_, &stateEst_, &map_, &lineTracker_);
    armMissionResumeGuard();
    map_.startMowingZones(stateEst_.x(), stateEst_.y(), zoneIds);
    opMgr_.changeOperationTypeByOperator(ctx, "Mow");
}

void Robot::startDocking() {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, resumeBlockedByMapChange_, &stateEst_, &map_, &lineTracker_);
    armMissionResumeGuard();
    opMgr_.changeOperationTypeByOperator(ctx, "Dock");
}

void Robot::emergencyStop() {
    hw_->setMotorPwm(0, 0, 0);
    unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, resumeBlockedByMapChange_, &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Idle");
    resumeBlockedByMapChange_ = false;
    activeMissionMapFingerprint_.clear();
    logger_->warn(TAG, "Emergency stop");
}

bool Robot::loadMap(const std::filesystem::path& path) {
    if (map_.load(path)) {
        const std::string newFingerprint = computeMapFingerprint(path);
        if (!currentMapFingerprint_.empty() && currentMapFingerprint_ != newFingerprint &&
            activeOpName() != "Idle") {
            resumeBlockedByMapChange_ = true;
            logger_->warn(TAG, "Map changed while mission active — resume blocked");
        }
        currentMapFingerprint_ = newFingerprint;
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

std::string Robot::currentStatePhase() const {
    const std::string opName = activeOpName();
    if (opName == "Idle") return "idle";
    if (opName == "Undock") return "undocking";
    if (opName == "NavToStart") return "navigating_to_start";
    if (opName == "Mow") return "mowing";
    if (opName == "Dock") return "docking";
    if (opName == "Charge") return "charging";
    if (opName == "WaitRain") return "waiting_for_rain";
    if (opName == "GpsWait") return "gps_recovery";
    if (opName == "EscapeReverse" || opName == "EscapeForward") return "obstacle_recovery";
    if (opName == "Error") return "fault";
    return "unknown";
}

std::string Robot::currentResumeTarget() const {
    if (const Op* op = opMgr_.activeOp(); op && op->nextOp != nullptr) {
        return op->nextOp->name();
    }
    return "";
}

std::string Robot::computeMapFingerprint(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return "";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return std::to_string(std::hash<std::string>{}(buffer.str()));
}

// ── Accessors ─────────────────────────────────────────────────────────────────

OdometryData Robot::lastOdometry() const { return odometry_; }
SensorData   Robot::lastSensors()  const { return sensors_;  }
BatteryData  Robot::lastBattery()  const { return battery_;  }

// ── Private helpers ────────────────────────────────────────────────────────────

void Robot::armMissionResumeGuard() {
    activeMissionMapFingerprint_ = currentMapFingerprint_;
    resumeBlockedByMapChange_ = false;
}

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
    const std::string opName = activeOpName();
    const float lowV      = config_->get<float>("battery_low_v",      25.5f);
    const float criticalV = config_->get<float>("battery_critical_v", 18.9f);

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
    d.gps_lat   = lastGps_.lat;
    d.gps_lon   = lastGps_.lon;
    d.gps_acc   = lastGps_.hAccuracy;
    d.bumper_l  = sensors_.bumperLeft;
    d.bumper_r  = sensors_.bumperRight;
    d.lift      = sensors_.lift;
    d.motor_err = sensors_.motorFault;
    d.uptime_s  = now_ms_ / 1000UL;
    d.mcu_version = hw_->getMcuFirmwareVersion();

    const auto& imu = lastImu_;
    if (imu.valid) {
        d.imu_heading = imu.yaw   * 180.0f / M_PI;
        d.imu_roll    = imu.roll  * 180.0f / M_PI;
        d.imu_pitch   = imu.pitch * 180.0f / M_PI;
    }
    d.ekf_health = stateEst_.fusionMode();
    d.ts_ms = now_ms_;
    if (Op* op = opMgr_.activeOp()) {
        d.state_since_ms = op->startTime_ms;
    }
    d.state_phase = currentStatePhase();
    d.resume_target = currentResumeTarget();

    if (resumeBlockedByMapChange_) {
        d.event_reason = "resume_blocked_map_changed";
    } else if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < criticalV) {
        d.event_reason = "battery_critical";
    } else if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < lowV) {
        d.event_reason = "battery_low";
    } else if (sensors_.rain) {
        d.event_reason = "rain_detected";
    } else if (sensors_.lift) {
        d.event_reason = "lift_triggered";
    } else if (sensors_.motorFault) {
        d.event_reason = "motor_fault";
    } else if (sensors_.bumperLeft || sensors_.bumperRight) {
        d.event_reason = "bumper_triggered";
    } else if (battery_.chargerConnected) {
        d.event_reason = "charger_connected";
    } else if (lineTracker_.kidnapped()) {
        d.event_reason = "kidnap_detected";
    } else if (gpsFixTimeoutLatched_) {
        d.event_reason = "gps_fix_timeout";
    } else if (gpsNoSignalLatched_) {
        d.event_reason = "gps_signal_lost";
    } else if (opName == "Undock") {
        d.event_reason = "undocking";
    } else if (opName == "NavToStart") {
        d.event_reason = "navigating_to_start";
    } else if (opName == "Mow") {
        d.event_reason = "mission_active";
    } else if (opName == "Dock") {
        d.event_reason = "docking";
    } else if (opName == "Charge") {
        d.event_reason = "charging";
    } else if (opName == "WaitRain") {
        d.event_reason = "waiting_for_rain";
    } else if (opName == "GpsWait") {
        d.event_reason = "gps_recovery_wait";
    } else if (opName == "EscapeReverse" || opName == "EscapeForward") {
        d.event_reason = "obstacle_recovery";
    } else {
        d.event_reason = "none";
    }

    if (opName == "Error") {
        if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < criticalV) {
            d.error_code = "ERR_BATTERY_CRITICAL";
        } else if (sensors_.lift) {
            d.error_code = "ERR_LIFT";
        } else if (sensors_.motorFault) {
            d.error_code = "ERR_MOTOR_FAULT";
        } else if (gpsFixTimeoutLatched_) {
            d.error_code = "ERR_GPS_TIMEOUT";
        } else {
            d.error_code = "ERR_GENERIC";
        }
    }

    {
        std::unique_lock<std::mutex> lk(diagMutex_);
        d.diag_active = diagReq_.active;
        d.diag_ticks  = diagReq_.accTicks;
    }
    return d;
}

void Robot::checkBattery(OpContext& ctx) {
    const float lowV      = config_->get<float>("battery_low_v",      25.5f);
    const float criticalV = config_->get<float>("battery_critical_v", 18.9f);

    if (battery_.voltage < 0.1f) return;  // no MCU data yet
    if (battery_.chargerConnected) return; // charging — no guard needed

    if (battery_.voltage < criticalV) {
        logger_->error(TAG, "Battery critical (" + std::to_string(battery_.voltage) + "V)");
        hw_->setMotorPwm(0, 0, 0);
        hw_->setBuzzer(true);   // BUG-002 fix: alert user on critical battery
        hw_->keepPowerOn(false);
        running_.store(false);
        if (Op* op = opMgr_.activeOp()) op->onBatteryUndervoltage(ctx);  // BUG-001 fix
    } else if (battery_.voltage < lowV) {
        logger_->warn(TAG, "Battery low (" + std::to_string(battery_.voltage) + "V) — docking");
        if (Op* op = opMgr_.activeOp()) op->onBatteryLowShouldDock(ctx);  // BUG-001 fix
    }
}

void Robot::monitorGpsResilience(OpContext& ctx) {
    // GPS resilience is only meaningful when a GPS driver is attached.
    if (!gps_) return;

    const unsigned long noSignalMs = static_cast<unsigned long>(
        config_->get<int>("gps_no_signal_ms", 3000));
    const unsigned long fixTimeoutMs = static_cast<unsigned long>(
        config_->get<int>("gps_fix_timeout_ms", 120000));
    const unsigned long recoverHysteresisMs = static_cast<unsigned long>(
        config_->get<int>("gps_recover_hysteresis_ms", 1000));

    const bool hasSignal = ctx.gpsHasFix || ctx.gpsHasFloat;

    if (hasSignal) {
        if (gpsRecoveryStart_ms_ == 0) gpsRecoveryStart_ms_ = ctx.now_ms;

        // Clear latches only after stable recovery to avoid flapping.
        if ((gpsNoSignalLatched_ || gpsFixTimeoutLatched_) &&
            (ctx.now_ms - gpsRecoveryStart_ms_ >= recoverHysteresisMs)) {
            gpsNoSignalLatched_ = false;
            gpsFixTimeoutLatched_ = false;
            logger_->info(TAG, "GPS recovered (hysteresis passed)");
        }
        return;
    }

    gpsRecoveryStart_ms_ = 0;

    Op* op = opMgr_.activeOp();
    if (!op) return;

    // Strongest event first: prolonged outage.
    if (!gpsFixTimeoutLatched_ && ctx.gpsFixAge_ms >= fixTimeoutMs) {
        gpsFixTimeoutLatched_ = true;
        gpsNoSignalLatched_ = true;
        op->onGpsFixTimeout(ctx);
        return;
    }

    // Short outage: degrade to GPS wait.
    if (!gpsNoSignalLatched_ && ctx.gpsFixAge_ms >= noSignalMs) {
        gpsNoSignalLatched_ = true;
        op->onGpsNoSignal(ctx);
    }
}

// ── Schedule methods (C.11) ────────────────────────────────────────────────────

bool Robot::loadSchedule(const std::filesystem::path& path) {
    schedulePath_ = path;
    if (!schedule_.load(path)) {
        logger_->warn(TAG, "No schedule file at " + path.string() + " — starting empty");
        return false;
    }
    logger_->info(TAG, "Schedule loaded: " + std::to_string(schedule_.entries().size()) + " entries");
    return true;
}

bool Robot::setSchedule(const nlohmann::json& arr) {
    if (!schedule_.fromJson(arr)) return false;
    if (!schedulePath_.empty()) schedule_.save(schedulePath_);
    return true;
}

nlohmann::json Robot::getSchedule() const {
    return schedule_.toJson();
}

// ── Diagnostic methods (C.10b) ─────────────────────────────────────────────────

nlohmann::json Robot::diagRunMotor(const std::string& motor, float pwm,
                                   unsigned duration_ms, int revolutions) {
    if (activeOpName() != "Idle")
        return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
    if (motor != "left" && motor != "right" && motor != "mow")
        return {{"ok", false}, {"error", "Unbekannter Motor: " + motor}};

    const int ticksPerRev = config_->get<int>("ticks_per_revolution",
                        config_->get<int>("ticks_per_rev", 1060));
    const int ticksTarget = (revolutions > 0)
        ? revolutions * ticksPerRev
        : 0;

    std::unique_lock<std::mutex> lk(diagMutex_);
    diagReq_ = DiagReq{motor, pwm, duration_ms, ticksTarget, true, 0, 0, false, ""};
    const bool ok = diagCv_.wait_for(lk, std::chrono::seconds(15),
                                     [this]{ return diagReq_.done; });
    if (!ok) {
        diagReq_.active = false;
        hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
        return {{"ok", false}, {"error", "Timeout"}};
    }
    const std::string result = diagReq_.resultJson;
    diagReq_ = {};
    try {
        return nlohmann::json::parse(result);
    } catch (const std::exception& e) {
        logger_->error(TAG, std::string("diagRunMotor result parse failed: ") + e.what());
        return {{"ok", false}, {"error", "Ungueltige Diagnose-Antwort"}};
    }
}

nlohmann::json Robot::diagImuCalib() {
    if (activeOpName() != "Idle")
        return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};

    hw_->calibrateImu();
    // Wait for calibration to finish (approx 5 seconds)
    // We could make it async, but for a diag command, blocking is fine.
    std::this_thread::sleep_for(std::chrono::milliseconds(5500));

    return {{"ok", true}, {"message", "IMU-Kalibrierung abgeschlossen"}};
}

nlohmann::json Robot::diagMowMotor(bool on) {
    if (activeOpName() != "Idle")
        return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
    hw_->setMotorPwm(0, 0, on ? 255 : 0);
    return {{"ok", true}, {"on", on}};
}

nlohmann::json Robot::diagDriveStraight(float distance_m) {
    if (activeOpName() != "Idle")
        return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};

    const unsigned duration_ms = static_cast<unsigned>(distance_m / 0.15f * 1000.0f); // ~0.15 m/s

    // Run both motors simultaneously using the diag state machine (virtual "both" motor).
    // We reuse the diag slot but apply both left+right PWM in run().
    std::unique_lock<std::mutex> lk(diagMutex_);
    diagReq_ = DiagReq{"both", 0.15f, duration_ms, 0, true, 0, 0, false, ""};
    const bool ok = diagCv_.wait_for(lk, std::chrono::seconds(30),
                                     [this]{ return diagReq_.done; });
    if (!ok) {
        diagReq_.active = false;
        hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
        return {{"ok", false}, {"error", "Timeout"}};
    }
    const std::string result = diagReq_.resultJson;
    diagReq_ = {};
    try {
        return nlohmann::json::parse(result);
    } catch (const std::exception& e) {
        logger_->error(TAG, std::string("diagDriveStraight result parse failed: ") + e.what());
        return {{"ok", false}, {"error", "Ungueltige Diagnose-Antwort"}};
    }
}

} // namespace sunray
