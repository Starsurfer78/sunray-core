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
    sensors_ = hw_->readSensors();
    previousSensors_ = sensors_;

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

    // ── 4. Timetable check (C.11) — once per minute ───────────────────────────
    if (now_ms_ - lastScheduleCheck_ms_ >= 60000UL) {
        lastScheduleCheck_ms_ = now_ms_;
        const bool active = schedule_.isActiveNow();
        if (active != scheduleWasActive_) {
            OpContext ctx{
                .hw = *hw_, .config = *config_, .logger = *logger_,
                .opMgr = opMgr_, .sensors = sensors_, .battery = battery_,
                .odometry = odometry_, .x = stateEst_.x(), .y = stateEst_.y(),
                .heading = stateEst_.heading(), .insidePerimeter = true,
                .isDockingRoute = false,
                .gpsHasFix   = stateEst_.gpsHasFix(),
                .gpsHasFloat = stateEst_.gpsHasFloat(),
                .gpsFixAge_ms = (gpsLastFixTime_ms_ == 0) ? 9999999u
                                  : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_),
                .now_ms      = now_ms_,
                .stateEst    = &stateEst_, .map = &map_, .lineTracker = &lineTracker_,
            };
            if (active) {
                if (Op* op = opMgr_.activeOp()) op->onTimetableStartMowing(ctx);
            } else {
                if (Op* op = opMgr_.activeOp()) op->onTimetableStopMowing(ctx);
            }
            scheduleWasActive_ = active;
        }
    }

    // ── 5. Obstacle cleanup (C.7) — once per second ───────────────────────────
    if (now_ms_ - lastObstacleCleanup_ms_ >= 1000UL) {
        map_.cleanupExpiredObstacles(now_ms_);
        lastObstacleCleanup_ms_ = now_ms_;
    }

    // ── 5. State estimation ───────────────────────────────────────────────────
    stateEst_.update(odometry_, dt_ms);
    if (gps_) {
        lastGps_ = gps_->getData();
        if (lastGps_.valid) {
            stateEst_.updateGps(
                lastGps_.relPosE, lastGps_.relPosN,
                lastGps_.solution == GpsSolution::Fixed,
                lastGps_.solution == GpsSolution::Float);
            gpsLastFixTime_ms_ = now_ms_;  // BUG-006 fix
        }
        // Push NMEA line to WebSocket when it changes
        if (ws_ && !lastGps_.nmeaGGA.empty() &&
            lastGps_.nmeaGGA != lastNmeaGGA_) {
            lastNmeaGGA_ = lastGps_.nmeaGGA;
            ws_->broadcastNmea(lastNmeaGGA_);
        }
    }

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
        .gpsFixAge_ms     = (gpsLastFixTime_ms_ == 0) ? 9999999u
                              : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_),  // BUG-006 fix
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

    // ── 8. Diagnostic motor test (C.10b) ─────────────────────────────────────
    {
        std::lock_guard<std::mutex> lk(diagMutex_);
        if (diagReq_.active && !diagReq_.done) {
            if (diagReq_.startMs == 0) diagReq_.startMs = now_ms_;
            auto toPwm = [](float normalized) -> int {
                const float clamped = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
                return static_cast<int>(clamped * 255.0f);
            };
            const int pwm = toPwm(diagReq_.pwm);

            // Apply PWM to the target motor
            if      (diagReq_.motor == "left")  hw_->setMotorPwm(pwm, 0,   0);
            else if (diagReq_.motor == "right") hw_->setMotorPwm(0,   pwm, 0);
            else if (diagReq_.motor == "mow")   hw_->setMotorPwm(0,   0,   pwm);
            else if (diagReq_.motor == "both")  hw_->setMotorPwm(pwm, pwm, 0);

            // Accumulate incremental ticks (average L+R for "both")
            if      (diagReq_.motor == "left")  diagReq_.accTicks += std::abs(odometry_.leftTicks);
            else if (diagReq_.motor == "right") diagReq_.accTicks += std::abs(odometry_.rightTicks);
            else if (diagReq_.motor == "mow")   diagReq_.accTicks += std::abs(odometry_.mowTicks);
            else if (diagReq_.motor == "both")  diagReq_.accTicks += (std::abs(odometry_.leftTicks) + std::abs(odometry_.rightTicks)) / 2;

            const bool timeDone    = (diagReq_.ticksTarget == 0)
                                   && (now_ms_ - diagReq_.startMs >= diagReq_.duration_ms);
            const bool tickDone    = (diagReq_.ticksTarget > 0)
                                   && (std::abs(diagReq_.accTicks) >= diagReq_.ticksTarget);
            const bool safeTimeout = (now_ms_ - diagReq_.startMs >= 15000u);  // always
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
            return;  // skip normal control path during diag
        }
    }

    // ── 9. Manual drive (joystick) — only in Idle, watchdog 500 ms ───────────
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
            hw_->setMotorPwm(static_cast<int>(left * 255.0f),
                             static_cast<int>(right * 255.0f), 0);
        }
    }

    // ── 10. Safety: bumper/lift/motor fault → force motor stop ────────────────
    if (sensors_.bumperLeft || sensors_.bumperRight || sensors_.lift || sensors_.motorFault) {
        // Let the active Op handle it via events (Op::onObstacle etc.).
        // But always guarantee motors stop immediately.
        hw_->setMotorPwm(0, 0, 0);

        if (Op* op = opMgr_.activeOp()) {
            // Edge-triggered events (fire only once on contact)
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
        }
    }
    previousSensors_ = sensors_;

    // ── 11. Status LEDs ───────────────────────────────────────────────────────
    updateStatusLeds();

    // ── 12. Telemetry push (WebSocket + MQTT) ────────────────────────────────
    const auto td = buildTelemetry();
    if (ws_)   ws_->pushTelemetry(td);
    if (mqtt_) mqtt_->pushTelemetry(td);

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
                                unsigned gpsFixAge_ms,
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
        .gpsFixAge_ms    = gpsFixAge_ms,
        .now_ms          = now_ms,
        .stateEst        = est,
        .map             = map,
        .lineTracker     = tracker,
    };
}

void Robot::startMowing() {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? 9999999u
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Mow");
}

void Robot::startMowingZones(const std::vector<std::string>& zoneIds) {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? 9999999u
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, &stateEst_, &map_, &lineTracker_);
    map_.startMowingZones(stateEst_.x(), stateEst_.y(), zoneIds);
    opMgr_.changeOperationTypeByOperator(ctx, "Mow");
}

void Robot::startDocking() {
    unsigned age = (gpsLastFixTime_ms_ == 0) ? 9999999u
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, &stateEst_, &map_, &lineTracker_);
    opMgr_.changeOperationTypeByOperator(ctx, "Dock");
}

void Robot::emergencyStop() {
    hw_->setMotorPwm(0, 0, 0);
    unsigned age = (gpsLastFixTime_ms_ == 0) ? 9999999u
                   : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
    auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                              sensors_, battery_, odometry_, now_ms_,
                              stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                              age, &stateEst_, &map_, &lineTracker_);
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
    d.gps_lat   = lastGps_.lat;
    d.gps_lon   = lastGps_.lon;
    d.bumper_l  = sensors_.bumperLeft;
    d.bumper_r  = sensors_.bumperRight;
    d.lift      = sensors_.lift;
    d.motor_err = sensors_.motorFault;
    d.uptime_s  = now_ms_ / 1000UL;
    d.mcu_version = hw_->getMcuFirmwareVersion();

    const auto imu = hw_->readImu();
    if (imu.valid) {
        d.imu_heading = imu.yaw   * 180.0f / M_PI;
        d.imu_roll    = imu.roll  * 180.0f / M_PI;
        d.imu_pitch   = imu.pitch * 180.0f / M_PI;
        stateEst_.updateImu(imu.yaw, imu.roll, imu.pitch);
    }
    d.ekf_health = stateEst_.fusionMode();

    {
        std::unique_lock<std::mutex> lk(diagMutex_);
        d.diag_active = diagReq_.active;
        d.diag_ticks  = diagReq_.accTicks;
    }
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
        hw_->setBuzzer(true);   // BUG-002 fix: alert user on critical battery
        hw_->keepPowerOn(false);
        running_.store(false);
        if (Op* op = opMgr_.activeOp()) op->onBatteryUndervoltage(ctx);  // BUG-001 fix
    } else if (battery_.voltage < lowV) {
        logger_->warn(TAG, "Battery low (" + std::to_string(battery_.voltage) + "V) — docking");
        if (Op* op = opMgr_.activeOp()) op->onBatteryLowShouldDock(ctx);  // BUG-001 fix
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
    return nlohmann::json::parse(result);
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
    return nlohmann::json::parse(result);
}

} // namespace sunray
