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
#include "core/navigation/MissionCompiler.h"
#include "core/navigation/RouteValidator.h"
#include "core/navigation/RuntimeState.h"
#include "core/navigation/WaypointExecutor.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace sunray
{

    namespace
    {
        std::string encodeWayType(nav::WayType type)
        {
            switch (type)
            {
            case nav::WayType::PERIMETER:
                return "perimeter";
            case nav::WayType::EXCLUSION:
                return "exclusion";
            case nav::WayType::DOCK:
                return "dock";
            case nav::WayType::MOW:
                return "mow";
            case nav::WayType::FREE:
            default:
                return "free";
            }
        }

        std::string encodeRouteSemantic(nav::RouteSemantic sem)
        {
            switch (sem)
            {
            case nav::RouteSemantic::COVERAGE_EDGE:
                return "coverage_edge";
            case nav::RouteSemantic::COVERAGE_INFILL:
                return "coverage_infill";
            case nav::RouteSemantic::TRANSIT_WITHIN_ZONE:
                return "transit_within_zone";
            case nav::RouteSemantic::TRANSIT_BETWEEN_COMPONENTS:
                return "transit_between_components";
            case nav::RouteSemantic::TRANSIT_INTER_ZONE:
                return "transit_inter_zone";
            case nav::RouteSemantic::DOCK_APPROACH:
                return "dock_approach";
            case nav::RouteSemantic::RECOVERY:
                return "recovery";
            case nav::RouteSemantic::UNKNOWN:
            default:
                return "unknown";
            }
        }

        uint64_t steadyNowMs()
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count());
        }
    } // namespace

    // ── Construction ───────────────────────────────────────────────────────────────

    Robot::Robot(std::unique_ptr<HardwareInterface> hw,
                 std::shared_ptr<Config> config,
                 std::shared_ptr<Logger> logger)
        : hw_(std::move(hw)), config_(std::move(config)), logger_(std::move(logger)), stateEst_(config_), map_(config_), runtimeState_(config_), lineTracker_(config_), startTime_(std::chrono::steady_clock::now())
    {
        if (!hw_)
            throw std::invalid_argument("Robot: hw must not be nullptr");
        if (!config_)
            throw std::invalid_argument("Robot: config must not be nullptr");
        if (!logger_)
            throw std::invalid_argument("Robot: logger must not be nullptr");
    }

    // ── Lifecycle ──────────────────────────────────────────────────────────────────

    bool Robot::init()
    {
        logger_->info(TAG, "Initialising hardware...");

        if (!hw_->init())
        {
            logger_->error(TAG, "Hardware init failed — aborting");
            return false;
        }

        running_.store(false);
        now_ms_ = 0;
        sensors_ = hw_->readSensors();
        battery_ = hw_->readBattery();
        previousSensors_ = sensors_;
        previousChargerConnected_ = battery_.chargerConnected;
        lastImu_ = {};
        imuCalibrationExpected_ = false;
        imuCalibrationActive_ = false;
        hw_->setLed(LedId::LED_1, LedState::OFF);
        hw_->setLed(LedId::LED_2, LedState::OFF);
        hw_->setLed(LedId::LED_3, LedState::OFF);

        logger_->info(TAG, "Robot id: " + hw_->getRobotId());
        if (!historyDb_.init(*config_, *logger_))
        {
            logger_->error(TAG, "History database init failed");
            return false;
        }
        eventRecorder_.bind(&historyDb_, logger_.get());
        lastRecordedOpName_ = activeOpName();
        if (config_->get<bool>("imu_auto_calibrate_on_start", true))
        {
            logger_->info(TAG, "IMU-Kalibrierung gestartet — Roboter bitte nicht bewegen");
            hw_->calibrateImu();
            imuCalibrationExpected_ = true;
            imuCalibrationActive_ = true;
        }
        logger_->info(TAG, "Init complete — entering IDLE");
        return true;
    }

    void Robot::loop()
    {
        running_.store(true);

        while (running_.load())
        {
            auto start = std::chrono::steady_clock::now();

            run();

            auto elapsed = std::chrono::steady_clock::now() - start;
            auto remaining = std::chrono::milliseconds(LOOP_PERIOD_MS) - elapsed;
            if (remaining > std::chrono::milliseconds(0))
            {
                std::this_thread::sleep_for(remaining);
            }
        }

        logger_->info(TAG, "Loop exited — shutting down hardware");
        hw_->setLed(LedId::LED_1, LedState::OFF);
        hw_->setLed(LedId::LED_2, LedState::RED);
        hw_->setLed(LedId::LED_3, LedState::OFF);
        hw_->setMotorPwm(0, 0, 0);
        hw_->keepPowerOn(false);
    }

    void Robot::setStmFlashMaintenance(bool active, uint64_t recoveryGraceMs)
    {
        stmFlashMaintenanceActive_.store(active);
        if (active)
        {
            stmFlashMaintenanceUntil_ms_.store(0);
            logger_->info(TAG, "STM flash maintenance window active");
            return;
        }

        uint64_t untilMs = 0;
        if (recoveryGraceMs > 0)
        {
            untilMs = steadyNowMs() + recoveryGraceMs;
        }
        stmFlashMaintenanceUntil_ms_.store(untilMs);
        logger_->info(TAG, "STM flash maintenance window cleared");
    }

    void Robot::run()
    {
        try
        {
            const unsigned long dt_ms = tickTiming();
            lastDt_ms_ = dt_ms;
            tickHardware();
            tickSchedule();
            tickObstacleCleanup();
            tickStateEstimation(dt_ms);
            processPendingExternalCommands();
            OpContext ctx = assembleOpContext();
            tickSafetyGuards(ctx);
            tickStateMachine(ctx);
            if (tickDiag())
            {
                updateStatusLeds();
                tickTelemetry();
                return;
            }
            tickButtonControl();
            tickUserFeedback();
            tickManualDrive(ctx);
            tickSafetyStop(ctx);
            tickSessionTracking();
            updateStatusLeds();
            tickTelemetry();
            ++controlLoops_;
        }
        catch (const std::exception &e)
        {
            try
            {
                hw_->setMotorPwm(0, 0, 0);
            }
            catch (...)
            {
            }
            running_.store(false);
            logger_->error(TAG, std::string("Unhandled exception in run(): ") + e.what());
        }
        catch (...)
        {
            try
            {
                hw_->setMotorPwm(0, 0, 0);
            }
            catch (...)
            {
            }
            running_.store(false);
            logger_->error(TAG, "Unhandled non-standard exception in run()");
        }
    }

    unsigned long Robot::tickTiming()
    {
        const unsigned long prev_ms = now_ms_;
        now_ms_ = static_cast<unsigned long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime_)
                .count());
        return now_ms_ - prev_ms;
    }

    void Robot::tickHardware()
    {
        hw_->run();

        odometry_ = hw_->readOdometry();
        sensors_ = hw_->readSensors();
        // Debounce lift sensor: require signal stable for 150 ms to filter motor-start vibration.
        if (!sensors_.lift) {
            liftActiveStart_ms_ = 0;
        } else {
            if (liftActiveStart_ms_ == 0) liftActiveStart_ms_ = now_ms_;
            sensors_.lift = (now_ms_ - liftActiveStart_ms_ >= 150UL);
        }
        battery_ = hw_->readBattery();
        if (battery_.chargerConnected && !previousChargerConnected_)
        {
            buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::ChargerConnected);
        }
        previousChargerConnected_ = battery_.chargerConnected;
        lastImu_ = hw_->readImu();
        if (imuCalibrationExpected_)
        {
            if (lastImu_.calibrating)
            {
                imuCalibrationActive_ = true;
            }
            else if (imuCalibrationActive_)
            {
                logger_->info(TAG, "IMU-Kalibrierung abgeschlossen");
                imuCalibrationExpected_ = false;
                imuCalibrationActive_ = false;
            }
        }
        if (lastImu_.valid)
        {
            stateEst_.updateImu(lastImu_.yaw, lastImu_.roll, lastImu_.pitch);
        }
    }

    void Robot::tickSchedule()
    {
        if (now_ms_ - lastScheduleCheck_ms_ < 60000UL)
            return;

        lastScheduleCheck_ms_ = now_ms_;
        const bool active = schedule_.isActiveNow();
        if (active == scheduleWasActive_)
            return;

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
        if (active)
        {
            const std::string opName = activeOpName();
            if (opName == "Idle" || opName == "Charge")
            {
                startMowing();
            }
            else if (Op *op = opMgr_.activeOp())
            {
                op->onTimetableStartMowing(ctx);
            }
        }
        else
        {
            if (Op *op = opMgr_.activeOp())
                op->onTimetableStopMowing(ctx);
        }
        scheduleWasActive_ = active;
    }

    void Robot::tickObstacleCleanup()
    {
        if (now_ms_ - lastObstacleCleanup_ms_ < 1000UL)
            return;
        map_.cleanupExpiredObstacles(now_ms_);
        lastObstacleCleanup_ms_ = now_ms_;
    }

    void Robot::tickStateEstimation(unsigned long dt_ms)
    {
        stateEst_.update(odometry_, dt_ms);
        if (!gps_)
            return;

        lastGps_ = gps_->getData();
        if (lastGps_.valid)
        {
            gpsSignalEver_ = true;
            stateEst_.updateGps(
                lastGps_.relPosE, lastGps_.relPosN,
                lastGps_.solution == GpsSolution::Fixed,
                lastGps_.solution == GpsSolution::Float);
            gpsLastFixTime_ms_ = now_ms_;
        }
        if (ws_ && !lastGps_.nmeaGGA.empty() && lastGps_.nmeaGGA != lastNmeaGGA_)
        {
            lastNmeaGGA_ = lastGps_.nmeaGGA;
            ws_->broadcastNmea(lastNmeaGGA_);
        }
    }

    OpContext Robot::assembleOpContext()
    {
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
        ctx.isDockingRoute = runtimeState_.isDocking();
        ctx.gpsHasFix = stateEst_.gpsHasFix();
        ctx.gpsHasFloat = stateEst_.gpsHasFloat();
        ctx.gpsFixAge_ms = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                     : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        ctx.resumeBlockedByMapChange = resumeBlockedByMapChange_;
        ctx.stateEst = &stateEst_;
        ctx.map = &map_;
        ctx.runtimeState = &runtimeState_;
        ctx.lineTracker = &lineTracker_;
        ctx.waypointExecutor = &waypointExecutor_;
        ctx.driveController = &driveController_;
        ctx.now_ms = now_ms_;
        ctx.dt_ms = lastDt_ms_;
        ctx.imuYaw = lastImu_.yaw;
        ctx.imuValid = lastImu_.valid;
        return ctx;
    }

    void Robot::tickSafetyGuards(OpContext &ctx)
    {
        checkBattery(ctx);
        monitorMcuConnectivity(ctx);
        monitorGpsResilience(ctx);
        monitorPerimeterSafety(ctx);
        monitorMapChangeSafety(ctx);
        monitorStuckDetection(ctx);
        monitorMowOverload(ctx);
        monitorOpWatchdog(ctx);
    }

    void Robot::tickStateMachine(OpContext &ctx)
    {
        const std::string beforeOp = lastRecordedOpName_;
        opMgr_.tick(ctx);
        const std::string afterOp = activeOpName();
        if (afterOp != beforeOp)
        {
            driveController_.reset();
            const std::string eventReason = transitionEventReason(beforeOp, afterOp);
            const std::string message =
                messages::humanReadableTransitionMessage(
                    beforeOp, afterOp, eventReason,
                    afterOp == "Error" ? currentErrorCode() : "");
            const std::string severity = afterOp == "Error"                                                                                                                 ? "error"
                                         : (afterOp == "GpsWait" || afterOp == "Dock" || afterOp == "WaitRain" || afterOp == "EscapeReverse" || afterOp == "EscapeForward") ? "warn"
                                                                                                                                                                            : "info";
            recordEvent(afterOp == "Error" ? "error" : "info",
                        "state_transition",
                        eventReason,
                        message,
                        afterOp == "Error" ? currentErrorCode() : "");
            if (beforeOp == "Mow" && afterOp != "Mow" && sessionActive_)
            {
                currentSession_.endedAtMs = wallClockNowMs();
                currentSession_.durationMs = currentSession_.endedAtMs - currentSession_.startedAtMs;
                currentSession_.batteryEndV = battery_.voltage;
                currentSession_.endReason = eventReason;
                if (sessionGpsAccuracyCount_ > 0)
                {
                    currentSession_.meanGpsAccuracyM = static_cast<float>(
                        sessionGpsAccuracySum_ / static_cast<double>(sessionGpsAccuracyCount_));
                }
                persistCurrentSession(true);
                sessionActive_ = false;
                sessionGpsAccuracySum_ = 0.0;
                sessionGpsAccuracyCount_ = 0;
                sessionLastPersist_ms_ = 0;
            }
            if (afterOp == "Idle" || afterOp == "Charge" || afterOp == "Error")
            {
                clearActiveMissionTracking();
            }
            if (afterOp == "Idle" || afterOp == "Charge" || afterOp == "NavToStart" || afterOp == "Undock")
            {
                stuckRecoveryCount_ = 0;
                if (afterOp != "Error")
                {
                    stuckRecoveryExhaustedLatched_ = false;
                }
            }
            // If GPS was already lost before NavToStart became active (latch fired while
            // still in Idle), reset the latch so NavToStartOp::onGpsNoSignal is
            // dispatched on the very next cycle rather than being silently suppressed.
            if (afterOp == "NavToStart" && gps_ &&
                !stateEst_.gpsHasFix() && !stateEst_.gpsHasFloat())
            {
                gpsNoSignalLatched_ = false;
                gpsSignalLostStart_ms_ = 0;
            }
            if (eventReason != "none" || afterOp == "Error")
            {
                showUiNotice(message, severity, eventReason,
                             afterOp == "Error" ? 12000 : 6000);
            }
            if (afterOp == "Mow" && !sessionActive_)
            {
                currentSession_ = {};
                currentSession_.startedAtMs = wallClockNowMs();
                currentSession_.id = "session-" + std::to_string(currentSession_.startedAtMs);
                currentSession_.batteryStartV = battery_.voltage;
                currentSession_.batteryEndV = battery_.voltage;
                nlohmann::json meta = {
                    {"source", "core"},
                    {"state_phase", "mowing"},
                };
                if (!currentMapFingerprint_.empty())
                {
                    meta["map_fingerprint"] = currentMapFingerprint_;
                }
                currentSession_.metadataJson = meta.dump();
                sessionLastX_ = stateEst_.x();
                sessionLastY_ = stateEst_.y();
                sessionGpsAccuracySum_ = 0.0;
                sessionGpsAccuracyCount_ = 0;
                sessionLastPersist_ms_ = 0;
                if (lastGps_.hAccuracy > 0.0f)
                {
                    currentSession_.meanGpsAccuracyM = lastGps_.hAccuracy;
                    currentSession_.maxGpsAccuracyM = lastGps_.hAccuracy;
                    sessionGpsAccuracySum_ = lastGps_.hAccuracy;
                    sessionGpsAccuracyCount_ = 1;
                }
                sessionActive_ = true;
                persistCurrentSession(true);
            }
            lastRecordedOpName_ = afterOp;
        }
    }

    bool Robot::tickDiag()
    {
        std::lock_guard<std::mutex> lk(diagMutex_);
        if (!diagReq_.active || diagReq_.done)
            return false;

        if (sensors_.stopButton)
        {
            hw_->setMotorPwm(0, 0, 0);
            diagReq_.active = false;
            diagReq_.done = true;
            diagReq_.resultJson = R"({"ok":false,"error":"Stop button pressed"})";
            diagCv_.notify_all();
            return false;
        }

        if (diagReq_.startMs == 0)
            diagReq_.startMs = now_ms_;
        if (!diagReq_.poseCaptured)
        {
            diagReq_.startX = stateEst_.x();
            diagReq_.startY = stateEst_.y();
            diagReq_.startHeading = stateEst_.heading();
            diagReq_.poseCaptured = true;
        }
        auto toPwm = [](float normalized) -> int
        {
            const float clamped = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
            return static_cast<int>(clamped * 255.0f);
        };
        const int leftPwm = toPwm(diagReq_.leftPwm);
        const int rightPwm = toPwm(diagReq_.rightPwm);
        const int mowPwm = toPwm(diagReq_.mowPwm);

        hw_->setMotorPwm(leftPwm, rightPwm, mowPwm);

        diagReq_.leftTicks += std::abs(odometry_.leftTicks);
        diagReq_.rightTicks += std::abs(odometry_.rightTicks);
        diagReq_.mowTicks += std::abs(odometry_.mowTicks);

        if (diagReq_.motor == "left")
        {
            diagReq_.accTicks = diagReq_.leftTicks;
        }
        else if (diagReq_.motor == "right")
        {
            diagReq_.accTicks = diagReq_.rightTicks;
        }
        else if (diagReq_.motor == "mow")
        {
            diagReq_.accTicks = diagReq_.mowTicks;
        }
        else
        {
            diagReq_.accTicks = (diagReq_.leftTicks + diagReq_.rightTicks) / 2;
        }

        const bool hasTickTarget = diagReq_.leftTicksTarget > 0 || diagReq_.rightTicksTarget > 0 || diagReq_.mowTicksTarget > 0;
        const bool leftDone = (diagReq_.leftTicksTarget <= 0) || (std::abs(diagReq_.leftTicks) >= diagReq_.leftTicksTarget);
        const bool rightDone = (diagReq_.rightTicksTarget <= 0) || (std::abs(diagReq_.rightTicks) >= diagReq_.rightTicksTarget);
        const bool mowDone = (diagReq_.mowTicksTarget <= 0) || (std::abs(diagReq_.mowTicks) >= diagReq_.mowTicksTarget);
        const bool timeDone = !hasTickTarget && (now_ms_ - diagReq_.startMs >= diagReq_.duration_ms);
        const bool tickDone = hasTickTarget && leftDone && rightDone && mowDone;
        const bool safeTimeout = (now_ms_ - diagReq_.startMs >= diagReq_.duration_ms);
        if (timeDone || tickDone || safeTimeout)
        {
            hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
            const int cfgTpr = config_->get<int>("ticks_per_revolution",
                                                 config_->get<int>("ticks_per_rev", 325));
            const float dx = stateEst_.x() - diagReq_.startX;
            const float dy = stateEst_.y() - diagReq_.startY;
            const float distance = std::sqrt(dx * dx + dy * dy);
            auto normalizeAngleRad = [](float angle)
            {
                while (angle > static_cast<float>(M_PI))
                    angle -= 2.0f * static_cast<float>(M_PI);
                while (angle < -static_cast<float>(M_PI))
                    angle += 2.0f * static_cast<float>(M_PI);
                return angle;
            };
            const float headingDeltaDeg =
                normalizeAngleRad(stateEst_.heading() - diagReq_.startHeading) * 180.0f / static_cast<float>(M_PI);
            nlohmann::json r;
            r["ok"] = true;
            r["ticks"] = diagReq_.accTicks;
            r["ticks_target"] = std::max({diagReq_.leftTicksTarget, diagReq_.rightTicksTarget, diagReq_.mowTicksTarget});
            r["ticks_per_rev_config"] = cfgTpr;
            r["left_ticks"] = diagReq_.leftTicks;
            r["right_ticks"] = diagReq_.rightTicks;
            r["mow_ticks"] = diagReq_.mowTicks;
            r["left_ticks_target"] = diagReq_.leftTicksTarget;
            r["right_ticks_target"] = diagReq_.rightTicksTarget;
            r["mow_ticks_target"] = diagReq_.mowTicksTarget;
            r["distance_m"] = distance;
            r["distance_target_m"] = diagReq_.targetDistance_m;
            r["heading_delta_deg"] = headingDeltaDeg;
            r["target_angle_deg"] = diagReq_.targetAngle_deg;
            diagReq_.resultJson = r.dump();
            diagReq_.done = true;
            diagCv_.notify_all();
        }
        return true;
    }

    void Robot::tickButtonControl()
    {
        const bool pressed = sensors_.stopButton;
        const std::string opName = activeOpName();
        const bool canRunHoldActions = (opName == "Idle" || opName == "Charge");

        if (pressed && !stopButtonPrev_ && !canRunHoldActions)
        {
            emergencyStop();
        }

        if (pressed)
        {
            if (!buttonHoldActive_)
            {
                buttonHoldStart_ms_ = now_ms_;
                buttonLastFeedback_s_ = 0;
                buttonHoldActive_ = true;
            }

            const unsigned heldSeconds = static_cast<unsigned>((now_ms_ - buttonHoldStart_ms_) / 1000UL);
            if (heldSeconds > buttonLastFeedback_s_)
            {
                buttonLastFeedback_s_ = heldSeconds;
                buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::ButtonTick);
            }
        }
        else if (stopButtonPrev_ && buttonHoldActive_)
        {
            const unsigned heldSeconds = static_cast<unsigned>((now_ms_ - buttonHoldStart_ms_) / 1000UL);
            const ButtonHoldAction action =
                resolveButtonHoldAction(heldSeconds, buttonHoldThresholds_);

            buzzerSequencer_.stop(*hw_);

            if (action == ButtonHoldAction::Shutdown)
            {
                buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::ShutdownRequested);
                emergencyStop();
                hw_->keepPowerOn(false);
                running_.store(false);
                logger_->warn(TAG, "Stop button long-press >=9s — shutdown requested");
            }
            else if (action == ButtonHoldAction::StartMowing)
            {
                buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::StartMowingAccepted);
                startMowing();
                logger_->info(TAG, "Stop button long-press >=6s — start mowing");
            }
            else if (action == ButtonHoldAction::StartDocking)
            {
                buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::StartDockingAccepted);
                startDocking();
                logger_->info(TAG, "Stop button long-press >=5s — start docking");
            }
            else if (action == ButtonHoldAction::EmergencyStop)
            {
                buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::EmergencyStop);
                emergencyStop();
                logger_->warn(TAG, "Stop button short-press >=1s — emergency stop");
            }

            buttonHoldStart_ms_ = 0;
            buttonLastFeedback_s_ = 0;
            buttonHoldActive_ = false;
        }

        stopButtonPrev_ = pressed;
    }

    void Robot::tickUserFeedback()
    {
        buzzerSequencer_.tick(*hw_, now_ms_);
    }

    void Robot::tickManualDrive(OpContext &ctx)
    {
        const uint64_t driveTs = manualDriveTs_ms_.load();
        const std::string op = activeOpName();
        // Charge excluded: driving while charging risks dock-pin disconnect → Error op.
        const bool inIdle   = (op == "Idle");
        const bool inManual = (op == "Manual");
        const bool fresh    = (driveTs > 0 && now_ms_ - driveTs < 500UL);

        if (inIdle && fresh)
        {
            // First fresh joystick command while idle → enter Manual op.
            opMgr_.changeOperationTypeByOperator(ctx, "Manual");
            return;
        }

        if (inManual)
        {
            if (!fresh)
            {
                // No command for >500 ms → return to Idle.
                opMgr_.changeOperationTypeByOperator(ctx, "Idle");
                return;
            }

            if (sensors_.stopButton || sensors_.bumperLeft || sensors_.bumperRight
                || sensors_.lift || sensors_.motorFault)
            {
                static uint64_t lastStopLogTs = 0;
                if (now_ms_ - lastStopLogTs > 5000UL)
                {
                    logger_->warn(TAG, "Joystick blocked: safety sensor active");
                    lastStopLogTs = now_ms_;
                }
                hw_->setMotorPwm(0, 0, 0);
                return;
            }

            driveController_.reset();

            const float lin = manualLinear1000_.load() / 1000.f;
            const float ang = manualAngular1000_.load() / 1000.f;
            constexpr float MAX_PWM = 0.35f;
            float left  = (lin - ang * 0.5f) * MAX_PWM;
            float right = (lin + ang * 0.5f) * MAX_PWM;
            left  = left  < -1.f ? -1.f : left  > 1.f ? 1.f : left;
            right = right < -1.f ? -1.f : right > 1.f ? 1.f : right;
            hw_->setMotorPwm(static_cast<int>(left  * 255.0f),
                             static_cast<int>(right * 255.0f), 0);
        }
    }

    void Robot::tickSafetyStop(OpContext &ctx)
    {
        if (sensors_.bumperLeft || sensors_.bumperRight || sensors_.lift || sensors_.motorFault)
        {
            driveController_.reset();
            hw_->setMotorPwm(0, 0, 0);

            if (Op *op = opMgr_.activeOp())
            {
                if ((sensors_.bumperLeft && !previousSensors_.bumperLeft) ||
                    (sensors_.bumperRight && !previousSensors_.bumperRight))
                {
                    const std::string message = messages::humanReadableReasonMessage("bumper_triggered");
                    recordEvent("warn", "safety_event", "bumper_triggered", message);
                    showUiNotice(message, "warn", "bumper_triggered", 4000);
                    op->onObstacle(ctx);
                }
                if (sensors_.lift && !previousSensors_.lift)
                {
                    const std::string message =
                        messages::humanReadableReasonMessage("lift_triggered", "ERR_LIFT");
                    recordEvent("error", "safety_event", "lift_triggered", message, "ERR_LIFT");
                    showUiNotice(message, "error", "lift_triggered", 12000);
                    op->onLiftTriggered(ctx);
                }
                if (sensors_.motorFault && !previousSensors_.motorFault)
                {
                    const std::string message =
                        messages::humanReadableReasonMessage("motor_fault", "ERR_MOTOR_FAULT");
                    recordEvent("error", "safety_event", "motor_fault", message, "ERR_MOTOR_FAULT");
                    showUiNotice(message, "error", "motor_fault", 12000);
                    op->onMotorError(ctx);
                }
                if (sensors_.rain && !previousSensors_.rain)
                {
                    const std::string message = messages::humanReadableReasonMessage("rain_detected");
                    recordEvent("info", "safety_event", "rain_detected", message);
                    showUiNotice(message, "warn", "rain_detected", 6000);
                    op->onRainTriggered(ctx);
                }
            }
        }
        else
        {
            if (Op *op = opMgr_.activeOp())
            {
                if (sensors_.rain && !previousSensors_.rain)
                {
                    const std::string message = messages::humanReadableReasonMessage("rain_detected");
                    recordEvent("info", "safety_event", "rain_detected", message);
                    showUiNotice(message, "warn", "rain_detected", 6000);
                    op->onRainTriggered(ctx);
                }
            }
        }
        previousSensors_ = sensors_;
    }

    void Robot::tickTelemetry()
    {
        const auto td = buildTelemetry();
        if (ws_)
            ws_->pushTelemetry(td);
        if (mqtt_)
            mqtt_->pushTelemetry(td);

        // N5.1/N5.2: push live map overlay (obstacles + active detour) to REST cache.
        if (ws_)
        {
            nlohmann::json overlay;
            nlohmann::json obsArray = nlohmann::json::array();
            for (const auto &obs : map_.obstacles())
            {
                nlohmann::json o;
                o["x"] = obs.center.x;
                o["y"] = obs.center.y;
                o["r"] = obs.radius_m;
                o["hits"] = obs.hitCount;
                obsArray.push_back(std::move(o));
            }
            overlay["obstacles"] = obsArray;

            const auto &det = runtimeSnapshot_.activeDetour;
            nlohmann::json detourJson;
            detourJson["active"] = det.active;
            nlohmann::json dptsArray = nlohmann::json::array();
            for (const auto &p : det.route.points)
            {
                nlohmann::json pt;
                pt["x"] = p.p.x;
                pt["y"] = p.p.y;
                dptsArray.push_back(std::move(pt));
            }
            detourJson["waypoints"] = dptsArray;
            if (det.hasReentryPoint)
            {
                detourJson["reentryX"] = det.reentryPoint.x;
                detourJson["reentryY"] = det.reentryPoint.y;
            }
            overlay["detour"] = detourJson;
            ws_->pushLiveOverlay(std::move(overlay));
        }
    }

    void Robot::tickSessionTracking()
    {
        if (!sessionActive_)
            return;

        const float dx = stateEst_.x() - sessionLastX_;
        const float dy = stateEst_.y() - sessionLastY_;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance > 0.02f)
        {
            currentSession_.distanceM += distance;
            sessionLastX_ = stateEst_.x();
            sessionLastY_ = stateEst_.y();
        }

        currentSession_.batteryEndV = battery_.voltage;
        currentSession_.durationMs = wallClockNowMs() - currentSession_.startedAtMs;

        if (lastGps_.hAccuracy > 0.0f)
        {
            sessionGpsAccuracySum_ += static_cast<double>(lastGps_.hAccuracy);
            sessionGpsAccuracyCount_++;
            currentSession_.meanGpsAccuracyM = static_cast<float>(
                sessionGpsAccuracySum_ / static_cast<double>(sessionGpsAccuracyCount_));
            if (lastGps_.hAccuracy > currentSession_.maxGpsAccuracyM)
            {
                currentSession_.maxGpsAccuracyM = lastGps_.hAccuracy;
            }
        }

        persistCurrentSession(false);
    }

    void Robot::persistCurrentSession(bool force)
    {
        if (!sessionActive_)
            return;
        if (!force && now_ms_ - sessionLastPersist_ms_ < 2000UL)
            return;
        if (historyDb_.upsertSession(currentSession_, *logger_))
        {
            sessionLastPersist_ms_ = now_ms_;
        }
    }

    void Robot::stop()
    {
        running_.store(false);
    }

    // ── Manual drive (joystick) ────────────────────────────────────────────────────

    void Robot::manualDrive(float linear, float angular)
    {
        // Clamp to [-1..1], store scaled as int for lock-free cross-thread access.
        auto clamp1 = [](float v)
        { return v < -1.f ? -1.f : v > 1.f ? 1.f
                                           : v; };
        manualLinear1000_.store(static_cast<int>(clamp1(linear) * 1000.f));
        manualAngular1000_.store(static_cast<int>(clamp1(angular) * 1000.f));
        manualDriveTs_ms_.store(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime_)
                .count()));
    }

    void Robot::processPendingExternalCommands()
    {
        std::deque<PendingExternalCommand> pending;
        {
            std::lock_guard<std::mutex> lk(externalCmdMutex_);
            if (externalCmdQueue_.empty())
                return;
            pending.swap(externalCmdQueue_);
        }

        for (const auto &cmd : pending)
        {
            switch (cmd.type)
            {
            case PendingExternalCommand::Type::StartMowing:
                startMowing();
                break;
            case PendingExternalCommand::Type::StartMowingZones:
                startMowingZones(cmd.zoneIds);
                break;
            case PendingExternalCommand::Type::StartMowingMission:
                startMowingMission(cmd.missionId, cmd.zoneIds);
                break;
            case PendingExternalCommand::Type::StartDocking:
                startDocking();
                break;
            case PendingExternalCommand::Type::EmergencyStop:
                emergencyStop();
                break;
            case PendingExternalCommand::Type::SetPose:
                setPose(cmd.x, cmd.y, cmd.heading);
                break;
            case PendingExternalCommand::Type::ResumeActiveMission:
                resumeActiveMission();
                break;
            }
        }
    }

    // ── Commands ───────────────────────────────────────────────────────────────────

    void Robot::enqueueExternalCommand(PendingExternalCommand cmd)
    {
        std::lock_guard<std::mutex> lk(externalCmdMutex_);
        externalCmdQueue_.push_back(std::move(cmd));
    }

    void Robot::requestStartMowing()
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::StartMowing;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestStartMowingZones(const std::vector<std::string> &zoneIds)
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::StartMowingZones;
        cmd.zoneIds = zoneIds;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestStartMowingMission(const std::string &missionId,
                                          const std::vector<std::string> &zoneIds)
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::StartMowingMission;
        cmd.missionId = missionId;
        cmd.zoneIds = zoneIds;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestStartDocking()
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::StartDocking;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestEmergencyStop()
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::EmergencyStop;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestSetPose(float x, float y, float heading)
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::SetPose;
        cmd.x = x;
        cmd.y = y;
        cmd.heading = heading;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::requestResumeActiveMission()
    {
        PendingExternalCommand cmd;
        cmd.type = PendingExternalCommand::Type::ResumeActiveMission;
        enqueueExternalCommand(std::move(cmd));
    }

    void Robot::storePendingMissionPlan(nav::MissionPlan plan)
    {
        std::lock_guard<std::mutex> lk(pendingPlanMutex_);
        pendingMissionPlan_ = std::move(plan);
        hasPendingMissionPlan_ = true;
    }

    // Helper: build a minimal OpContext for operator commands.
    // Navigation objects are included so that Op::begin() can start the map route.
    static OpContext makeCommandCtx(HardwareInterface &hw, Config &config, Logger &logger,
                                    OpManager &opMgr,
                                    const SensorData &sensors, const BatteryData &battery,
                                    const OdometryData &odometry, unsigned long now_ms,
                                    float x, float y, float heading,
                                    unsigned gpsFixAge_ms,
                                    bool resumeBlockedByMapChange,
                                    nav::StateEstimator *est, nav::Map *map,
                                    nav::RuntimeState *runtimeState,
                                    nav::LineTracker *tracker,
                                    nav::WaypointExecutor *wpExec = nullptr)
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
        ctx.runtimeState = runtimeState;
        ctx.lineTracker = tracker;
        ctx.waypointExecutor = wpExec;
        ctx.now_ms = now_ms;
        return ctx;
    }

    bool Robot::canStartMowingMission()
    {
        if (!map_.isLoaded())
        {
            rejectStartMowingMission("start_rejected_no_map",
                                     "Start verweigert: keine aktive Karte geladen");
            return false;
        }
        if (map_.zones().empty())
        {
            rejectStartMowingMission("start_rejected_no_zones",
                                     "Start verweigert: Karte hat keine Mähzonen");
            return false;
        }
        return true;
    }

    nav::MissionPlan Robot::buildOperatorMissionPlan(const std::vector<std::string> &zoneIds) const
    {
        nlohmann::json mission = nlohmann::json::object();
        mission["id"] = zoneIds.empty() ? "adhoc-map" : "adhoc-zones";
        mission["name"] = zoneIds.empty() ? "Ad-hoc Full Map Start" : "Ad-hoc Zone Start";
        mission["zoneIds"] = nlohmann::json::array();
        mission["overrides"] = nlohmann::json::object();

        if (zoneIds.empty())
        {
            for (const auto &zone : map_.zones())
                mission["zoneIds"].push_back(zone.id);
        }
        else
        {
            for (const auto &zoneId : zoneIds)
                mission["zoneIds"].push_back(zoneId);
        }

        return nav::buildMissionPlanPreview(map_, mission);
    }

    void Robot::buildAndStartWaypointPlan(const nav::MissionPlan &plan)
    {
        waypointExecutor_.startPlan(plan.route);
        waypointExecutor_.seekToIndex(runtimeState_.mowPointsIdx());
    }

    bool Robot::canStartPlannedMission(const nav::MissionPlan &plan)
    {
        return canStartPlannedMission(plan.route);
    }

    bool Robot::canStartPlannedMission(const nav::RoutePlan &route)
    {
        if (!map_.isLoaded())
        {
            rejectStartMowingMission("start_rejected_no_map",
                                     "Start verweigert: keine aktive Karte geladen");
            return false;
        }
        if (route.points.empty())
        {
            rejectStartMowingMission("start_rejected_no_mow_points",
                                     "Start verweigert: Mission liefert keine Fahrbahn");
            return false;
        }

        const nav::RouteValidationResult vr = nav::RouteValidator::validate(map_, route);
        if (!vr.valid)
        {
            std::string summary = "Start verweigert: Missionsroute ungültig";
            if (!vr.errors.empty())
            {
                summary += " — ";
                summary += vr.errors.front().message;
                if (vr.errors.size() > 1)
                {
                    summary += " (+" + std::to_string(vr.errors.size() - 1) + " weitere Fehler)";
                }
            }
            for (const auto &err : vr.errors)
            {
                logger_->warn(TAG, std::string("RouteValidator: ") + err.message);
            }
            rejectStartMowingMission("start_rejected_invalid_route", summary);
            return false;
        }
        return true;
    }

    void Robot::rejectStartMowingMission(const std::string &eventReason,
                                         const std::string &message)
    {
        logger_->warn(TAG, message);
        showUiNotice(message, "warn", eventReason);
        buzzerSequencer_.play(*hw_, now_ms_, BuzzerPattern::StartRejected);
        recordEvent("warn", "start_rejected", eventReason, message);
    }

    nlohmann::json Robot::loadMissionDocumentById(const std::string &missionId) const
    {
        const std::string missionPath = config_->get<std::string>("mission_path", "");
        if (missionPath.empty())
            return nlohmann::json::object();

        std::ifstream in(missionPath);
        if (!in.is_open())
            return nlohmann::json::object();

        nlohmann::json missions;
        try
        {
            in >> missions;
        }
        catch (...)
        {
            return nlohmann::json::object();
        }
        if (!missions.is_array())
            return nlohmann::json::object();

        for (const auto &mission : missions)
        {
            if (!mission.is_object())
                continue;
            if (mission.value("id", std::string()) == missionId)
                return mission;
        }
        return nlohmann::json::object();
    }

    void Robot::startMowing()
    {
        if (!canStartMowingMission())
            return;

        // N6.3: resume interrupted mission (robot docked mid-run) instead of rebuilding.
        if (hasInterruptedMission_)
        {
            resumeActiveMission();
            return;
        }

        // N6.1: prefer the plan cached from the most-recent planner preview.
        nav::MissionPlan plan;
        {
            std::lock_guard<std::mutex> lk(pendingPlanMutex_);
            if (hasPendingMissionPlan_)
            {
                plan = std::move(pendingMissionPlan_);
                hasPendingMissionPlan_ = false;
            }
        }
        if (plan.route.points.empty())
            plan = buildOperatorMissionPlan();

        if (!canStartPlannedMission(plan))
            return;
        clearActiveMissionTracking();
        activateMissionPlan(plan);
        if (!runtimeState_.startPlannedMowing(map_, stateEst_.x(), stateEst_.y(), plan))
        {
            rejectStartMowingMission("start_rejected_no_mow_points",
                                     "Start verweigert: Mähroute konnte nicht aktiviert werden");
            clearActiveMissionTracking();
            return;
        }
        buildAndStartWaypointPlan(plan);

        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        armMissionResumeGuard();
        refreshActiveMissionProgress();
        recordEvent("info", "operator_command", "operator_start_mowing",
                    "Mähauftrag wurde gestartet");
        opMgr_.changeOperationTypeByOperator(ctx, "Mow");
    }

    void Robot::startMowingZones(const std::vector<std::string> &zoneIds)
    {
        if (!map_.isLoaded())
        {
            rejectStartMowingMission("start_rejected_no_map",
                                     "Start verweigert: keine aktive Karte geladen");
            return;
        }
        if (zoneIds.empty())
        {
            startMowing();
            return;
        }

        const nav::MissionPlan plan = buildOperatorMissionPlan(zoneIds);
        if (!canStartPlannedMission(plan))
            return;
        clearActiveMissionTracking();

        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        armMissionResumeGuard();
        activateMissionPlan(plan);
        if (!runtimeState_.startPlannedMowing(map_, stateEst_.x(), stateEst_.y(), plan))
        {
            rejectStartMowingMission("start_rejected_no_mow_points",
                                     "Start verweigert: Zonenroute konnte nicht aktiviert werden");
            clearActiveMissionTracking();
            return;
        }
        buildAndStartWaypointPlan(plan);
        refreshActiveMissionProgress();
        recordEvent("info", "operator_command", "operator_start_mowing_zones",
                    "Zonen-Mähauftrag wurde gestartet");
        opMgr_.changeOperationTypeByOperator(ctx, "Mow");
    }

    void Robot::startMowingMission(const std::string &missionId,
                                   const std::vector<std::string> &zoneIds)
    {
        if (!map_.isLoaded())
        {
            rejectStartMowingMission("start_rejected_no_map",
                                     "Start verweigert: keine aktive Karte geladen");
            return;
        }

        nlohmann::json missionDoc = loadMissionDocumentById(missionId);
        if (!missionDoc.is_object())
        {
            rejectStartMowingMission("start_rejected_no_mission",
                                     "Start verweigert: Mission konnte nicht geladen werden");
            return;
        }
        if (!zoneIds.empty())
            missionDoc["zoneIds"] = zoneIds;

        const nav::MissionPlan plan = nav::buildMissionPlanPreview(map_, missionDoc);
        if (!canStartPlannedMission(plan))
            return;

        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        armMissionResumeGuard();
        activateMissionPlan(plan);
        if (!runtimeState_.startPlannedMowing(map_, stateEst_.x(), stateEst_.y(), plan))
        {
            rejectStartMowingMission("start_rejected_no_mow_points",
                                     "Start verweigert: Missionsroute konnte nicht aktiviert werden");
            clearActiveMissionTracking();
            return;
        }
        buildAndStartWaypointPlan(plan);
        refreshActiveMissionProgress();
        recordEvent("info", "operator_command", "operator_start_mission",
                    "Mission wurde gestartet");
        opMgr_.changeOperationTypeByOperator(ctx, "Mow");
    }

    void Robot::startDocking()
    {
        // N6.3: preserve interrupted mission state so the operator can resume.
        if (!activeMissionPlan_.route.points.empty())
        {
            interruptedMissionPlan_ = activeMissionPlan_;
            interruptedMowPointsIdx_ = runtimeState_.mowPointsIdx();
            hasInterruptedMission_ = true;
        }
        clearActiveMissionTracking();
        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        armMissionResumeGuard();
        recordEvent("info", "operator_command", "operator_start_docking",
                    "Docking wurde gestartet");
        opMgr_.changeOperationTypeByOperator(ctx, "Dock");
    }

    void Robot::resumeActiveMission()
    {
        if (!hasInterruptedMission_ || interruptedMissionPlan_.route.points.empty())
        {
            rejectStartMowingMission("resume_rejected_no_plan",
                                     "Fortsetzung nicht möglich: kein unterbrochener Auftrag vorhanden");
            return;
        }
        if (!canStartPlannedMission(interruptedMissionPlan_))
            return;

        nav::MissionPlan plan = std::move(interruptedMissionPlan_);
        const int resumeIdx = interruptedMowPointsIdx_;
        interruptedMissionPlan_ = nav::MissionPlan{};
        interruptedMowPointsIdx_ = -1;
        hasInterruptedMission_ = false;

        // Pending preview plan is obsolete once we resume
        {
            std::lock_guard<std::mutex> lk(pendingPlanMutex_);
            hasPendingMissionPlan_ = false;
        }

        clearActiveMissionTracking();
        activateMissionPlan(plan);
        if (!runtimeState_.startPlannedMowing(map_, stateEst_.x(), stateEst_.y(), plan))
        {
            rejectStartMowingMission("resume_rejected_no_mow_points",
                                     "Fortsetzung verweigert: Route konnte nicht reaktiviert werden");
            clearActiveMissionTracking();
            return;
        }
        // N6.3: restore exact progress point instead of nearest-position heuristic.
        if (resumeIdx >= 0)
            runtimeState_.seekToMowIndex(resumeIdx);

        buildAndStartWaypointPlan(plan);

        if (resumeIdx >= 0)
            waypointExecutor_.seekToIndex(resumeIdx);

        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        armMissionResumeGuard();
        refreshActiveMissionProgress();
        recordEvent("info", "operator_command", "operator_resume_mission",
                    "Unterbrochener Mähauftrag wird fortgesetzt");
        opMgr_.changeOperationTypeByOperator(ctx, "Mow");
    }

    void Robot::emergencyStop()
    {
        clearActiveMissionTracking();
        driveController_.reset();
        hw_->setMotorPwm(0, 0, 0);
        const std::string previousOp = activeOpName();
        unsigned age = (gpsLastFixTime_ms_ == 0) ? now_ms_
                                                 : static_cast<unsigned>(now_ms_ - gpsLastFixTime_ms_);
        auto ctx = makeCommandCtx(*hw_, *config_, *logger_, opMgr_,
                                  sensors_, battery_, odometry_, now_ms_,
                                  stateEst_.x(), stateEst_.y(), stateEst_.heading(),
                                  age, resumeBlockedByMapChange_, &stateEst_, &map_, &runtimeState_, &lineTracker_, &waypointExecutor_);
        const std::string message =
            previousOp == "Idle"
                ? "Not-Stopp ausgeloest, Roboter bleibt in Bereitschaft"
                : "Not-Stopp ausgeloest, Roboter wurde gestoppt";
        showUiNotice(message, "warn", "operator_emergency_stop", 6000);
        recordEvent("warn", "operator_command", "operator_emergency_stop",
                    "Not-Stopp wurde ausgelöst");
        opMgr_.changeOperationTypeByOperator(ctx, "Idle");
        resumeBlockedByMapChange_ = false;
        activeMissionMapFingerprint_.clear();
        // N6.3: hard stop clears any interrupted mission
        interruptedMissionPlan_ = nav::MissionPlan{};
        interruptedMowPointsIdx_ = -1;
        hasInterruptedMission_ = false;

        {
            std::lock_guard<std::mutex> lk(diagMutex_);
            if (diagReq_.active)
            {
                diagReq_.active = false;
                diagReq_.done = true;
                diagCv_.notify_all();
                logger_->info(TAG, "Active diagnostic cancelled by emergency stop");
            }
        }

        logger_->warn(TAG, "Emergency stop");
    }

    bool Robot::loadMap(const std::filesystem::path &path)
    {
        if (map_.load(path))
        {
            const std::string newFingerprint = computeMapFingerprint(path);
            if (!currentMapFingerprint_.empty() && currentMapFingerprint_ != newFingerprint &&
                activeOpName() != "Idle")
            {
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

    nlohmann::json Robot::getMapJson() const
    {
        return map_.exportMapJson();
    }

    void Robot::clearActiveMissionTracking()
    {
        activeMissionPlan_ = nav::MissionPlan{};
        runtimeSnapshot_ = nav::RuntimeSnapshot{};
        runtimeState_.clear();
        if (ws_)
            ws_->pushActivePlan(nlohmann::json::object()); // N4.2: clear stored plan
    }

    void Robot::activateMissionPlan(const nav::MissionPlan &plan)
    {
        activeMissionPlan_ = plan;
        runtimeSnapshot_ = nav::RuntimeSnapshot{};
        runtimeSnapshot_.missionId = plan.missionId;
        runtimeSnapshot_.hasActiveMissionPlan = !plan.route.points.empty();

        // N4.2: push plan geometry to WebSocket server once so the dashboard
        // can fetch it via GET /api/mission/active-plan.
        if (ws_)
        {
            nlohmann::json wpArray = nlohmann::json::array();
            for (const auto &wp : nav::routePlanToWaypoints(plan.route))
                wpArray.push_back({{"x", wp.x}, {"y", wp.y}, {"mowOn", wp.mowOn}});
            nlohmann::json routePoints = nlohmann::json::array();
            for (const auto &point : plan.route.points)
            {
                routePoints.push_back({
                    {"p", {point.p.x, point.p.y}},
                    {"reverse", point.reverse},
                    {"slow", point.slow},
                    {"reverseAllowed", point.reverseAllowed},
                    {"clearance_m", point.clearance_m},
                    {"sourceMode", encodeWayType(point.sourceMode)},
                    {"semantic", encodeRouteSemantic(point.semantic)},
                    {"zoneId", point.zoneId},
                    {"componentId", point.componentId},
                });
            }
            nlohmann::json planJson = {
                {"missionId", plan.missionId},
                {"zoneOrder", plan.zoneOrder},
                {"waypoints", std::move(wpArray)},
                {"planRef",
                 {{"id", plan.missionId.empty() ? std::string("active-mission") : plan.missionId},
                  {"revision", static_cast<int>(plan.route.points.size())},
                  {"generatedAtMs", wallClockNowMs()}}},
                {"route",
                 {{"active", plan.route.active},
                  {"valid", plan.route.valid},
                  {"invalidReason", plan.route.invalidReason},
                  {"sourceMode", encodeWayType(plan.route.sourceMode)},
                  {"points", std::move(routePoints)}}},
                {"waypoints", std::move(wpArray)},
            };
            ws_->pushActivePlan(std::move(planJson));
        }
    }

    void Robot::refreshActiveMissionProgress()
    {
        runtimeSnapshot_.missionId = activeMissionPlan_.missionId;
        runtimeSnapshot_.hasActiveMissionPlan = !activeMissionPlan_.route.points.empty();
        runtimeSnapshot_.hasActiveZonePlan = false;
        runtimeSnapshot_.activeZoneId.clear();
        runtimeSnapshot_.activePointIndex = -1;
        runtimeSnapshot_.hasCurrentTarget = false;
        runtimeSnapshot_.completedRoute = nav::RoutePlan{};
        runtimeSnapshot_.activeDetour = nav::LocalDetour{};

        if (activeMissionPlan_.route.points.empty())
            return;

        if (runtimeState_.wayMode() == nav::WayType::MOW &&
            runtimeState_.mowPointsIdx() >= 0 &&
            runtimeState_.mowPointsIdx() < static_cast<int>(activeMissionPlan_.route.points.size()))
        {
            runtimeSnapshot_.activePointIndex = runtimeState_.mowPointsIdx();
            const auto &activePoint = activeMissionPlan_.route.points[runtimeState_.mowPointsIdx()];
            runtimeSnapshot_.activeZoneId = activePoint.zoneId;

            runtimeSnapshot_.completedRoute.sourceMode = activeMissionPlan_.route.sourceMode;
            runtimeSnapshot_.completedRoute.valid = activeMissionPlan_.route.valid;
            runtimeSnapshot_.completedRoute.invalidReason = activeMissionPlan_.route.invalidReason;
            runtimeSnapshot_.completedRoute.points.assign(
                activeMissionPlan_.route.points.begin(),
                activeMissionPlan_.route.points.begin() + runtimeState_.mowPointsIdx());
        }

        if (runtimeSnapshot_.activeZoneId.empty() && !activeMissionPlan_.zoneOrder.empty())
        {
            runtimeSnapshot_.activeZoneId =
                map_.zoneIdForPoint(runtimeState_.targetPoint().x, runtimeState_.targetPoint().y, activeMissionPlan_.zoneOrder);
        }

        if (runtimeState_.hasActiveMowingPlan() || runtimeState_.wayMode() == nav::WayType::FREE)
        {
            runtimeSnapshot_.currentTarget = runtimeState_.targetPoint();
            runtimeSnapshot_.hasCurrentTarget = true;
        }

        if (runtimeState_.wayMode() == nav::WayType::FREE && !runtimeState_.freeRoutePlan().points.empty())
        {
            runtimeSnapshot_.activeDetour.route = runtimeState_.freeRoutePlan();
            runtimeSnapshot_.activeDetour.reentryPoint = runtimeState_.targetPoint();
            runtimeSnapshot_.activeDetour.hasReentryPoint = true;
            runtimeSnapshot_.activeDetour.resumeZoneId = runtimeSnapshot_.activeZoneId;
            runtimeSnapshot_.activeDetour.resumePointIndex = runtimeSnapshot_.activePointIndex;
            runtimeSnapshot_.activeDetour.active = true;
        }

        for (const auto &zonePlan : activeMissionPlan_.zonePlans)
        {
            if (zonePlan.zoneId == runtimeSnapshot_.activeZoneId)
            {
                runtimeSnapshot_.hasActiveZonePlan = true;
                return;
            }
        }
    }

    int Robot::activeMissionZoneIndex() const
    {
        if (activeMissionPlan_.zoneOrder.empty())
            return 0;
        if (runtimeSnapshot_.activeZoneId.empty())
            return 1;

        for (std::size_t index = 0; index < activeMissionPlan_.zoneOrder.size(); ++index)
        {
            if (activeMissionPlan_.zoneOrder[index] == runtimeSnapshot_.activeZoneId)
                return static_cast<int>(index) + 1;
        }
        return 1;
    }

    nlohmann::json Robot::getHistoryEvents(unsigned limit) const
    {
        return historyDb_.listEvents(limit, *logger_);
    }

    nlohmann::json Robot::getHistorySessions(unsigned limit) const
    {
        return historyDb_.listSessions(limit, *logger_);
    }

    nlohmann::json Robot::getHistoryStatisticsSummary() const
    {
        return historyDb_.buildSummary(*logger_);
    }

    nlohmann::json Robot::clearHistory()
    {
        if (!historyDb_.enabled())
        {
            return nlohmann::json{{"ok", false}, {"error", "history database is disabled"}};
        }
        if (historyDb_.clear(*logger_))
        {
            return nlohmann::json{{"ok", true}};
        }
        else
        {
            return nlohmann::json{{"ok", false}, {"error", "failed to clear history database"}};
        }
    }

    void Robot::setPose(float x, float y, float heading)
    {
        stateEst_.setPose(x, y, heading);
    }

    std::string Robot::activeOpName() const
    {
        Op *op = opMgr_.activeOp();
        return op ? op->name() : "Idle";
    }

    std::string Robot::currentStatePhase() const
    {
        const std::string opName = activeOpName();
        if (opName == "Idle")
            return "idle";
        if (opName == "Manual")
            return "manual";
        if (opName == "Undock")
            return "undocking";
        if (opName == "NavToStart")
            return "navigating_to_start";
        if (opName == "Mow")
            return "mowing";
        if (opName == "Dock")
            return "docking";
        if (opName == "Charge")
            return "charging";
        if (opName == "WaitRain")
            return "waiting_for_rain";
        if (opName == "GpsWait")
            return "gps_recovery";
        if (opName == "EscapeReverse" || opName == "EscapeForward")
            return "obstacle_recovery";
        if (opName == "Error")
            return "fault";
        return "unknown";
    }

    std::string Robot::currentResumeTarget() const
    {
        if (const Op *op = opMgr_.activeOp(); op && op->nextOp != nullptr)
        {
            return op->nextOp->name();
        }
        return "";
    }

    std::string Robot::computeMapFingerprint(const std::filesystem::path &path)
    {
        std::ifstream in(path);
        if (!in)
            return "";
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return std::to_string(std::hash<std::string>{}(buffer.str()));
    }

    long long Robot::wallClockNowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    EventRecord Robot::makeEventRecord(const std::string &level,
                                       const std::string &eventType,
                                       const std::string &eventReason,
                                       const std::string &message,
                                       const std::string &errorCode) const
    {
        EventRecord event;
        event.ts_ms = now_ms_;
        event.wall_ts_ms = wallClockNowMs();
        event.level = level;
        event.module = TAG;
        event.eventType = eventType;
        event.statePhase = currentStatePhase();
        event.eventReason = eventReason;
        event.errorCode = errorCode;
        event.message = message;
        if (battery_.voltage > 0.0f)
            event.batteryV = battery_.voltage;
        event.gpsSol = currentGpsSolutionCode();
        event.x = stateEst_.x();
        event.y = stateEst_.y();
        return event;
    }

    void Robot::recordEvent(const std::string &level,
                            const std::string &eventType,
                            const std::string &eventReason,
                            const std::string &message,
                            const std::string &errorCode)
    {
        eventRecorder_.record(makeEventRecord(level, eventType, eventReason, message, errorCode));
    }

    int Robot::currentGpsSolutionCode() const
    {
        if (stateEst_.gpsHasFix())
            return 4;
        if (stateEst_.gpsHasFloat())
            return 5;
        return 0;
    }

    std::string Robot::currentErrorCode() const
    {
        const float criticalV = config_->get<float>("battery_critical_v", 18.9f);
        if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < criticalV)
        {
            return "ERR_BATTERY_CRITICAL";
        }
        if (stuckRecoveryExhaustedLatched_)
            return "ERR_STUCK";
        if (mcuCommLossLatched_)
            return "ERR_MCU_COMMS";
        if (sensors_.lift)
            return "ERR_LIFT";
        if (sensors_.motorFault)
            return "ERR_MOTOR_FAULT";
        if (gpsFixTimeoutLatched_)
            return "ERR_GPS_TIMEOUT";
        return "ERR_GENERIC";
    }

    std::string Robot::currentDominantEventReason() const
    {
        const std::string opName = activeOpName();
        const float lowV = config_->get<float>("battery_low_v", 25.5f);
        const float criticalV = config_->get<float>("battery_critical_v", 18.9f);

        if (uiMessageUntil_ms_ > now_ms_ && uiEventReason_ != "none")
        {
            return uiEventReason_;
        }
        if (resumeBlockedByMapChange_)
            return "resume_blocked_map_changed";
        if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < criticalV)
        {
            return "battery_critical";
        }
        if (battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < lowV)
        {
            return "battery_low";
        }
        if (stuckRecoveryExhaustedLatched_)
            return "stuck_recovery_exhausted";
        if (mcuCommLossLatched_)
            return "mcu_comm_lost";
        if (sensors_.rain)
            return "rain_detected";
        if (sensors_.lift)
            return "lift_triggered";
        if (sensors_.motorFault)
            return "motor_fault";
        if (sensors_.bumperLeft || sensors_.bumperRight)
            return "bumper_triggered";
        if (battery_.chargerConnected)
            return "charger_connected";
        // Stale kidnap flag is meaningless during manual joystick driving.
        if (lineTracker_.kidnapped() && opName != "Manual")
            return "kidnap_detected";
        if (gpsFixTimeoutLatched_)
            return "gps_fix_timeout";
        if (gpsNoSignalLatched_)
            return "gps_signal_lost";
        if (opName == "Manual")
            return "manual_drive";
        if (opName == "Undock")
            return "undocking";
        if (opName == "NavToStart")
            return "navigating_to_start";
        if (opName == "Mow")
            return "mission_active";
        if (opName == "Dock")
            return "docking";
        if (opName == "Charge")
            return "charging";
        if (opName == "WaitRain")
            return "waiting_for_rain";
        if (opName == "GpsWait")
            return "gps_recovery_wait";
        if (opName == "EscapeReverse" || opName == "EscapeForward")
            return "obstacle_recovery";
        return "none";
    }

    std::string Robot::transitionEventReason(const std::string &beforeOp,
                                             const std::string &afterOp) const
    {
        if (afterOp == "Error")
        {
            if (beforeOp == "Dock")
            {
                if (stuckRecoveryExhaustedLatched_)
                    return "stuck_recovery_exhausted";
                if (sensors_.lift)
                    return "lift_triggered";
                if (sensors_.motorFault)
                    return "motor_fault";
                if (gpsFixTimeoutLatched_)
                    return "gps_fix_timeout";
                return "dock_failed";
            }
            if (beforeOp == "Mow" && stuckRecoveryExhaustedLatched_)
            {
                return "stuck_recovery_exhausted";
            }
            if (beforeOp == "Undock")
            {
                if (sensors_.lift)
                    return "lift_triggered";
                if (sensors_.motorFault)
                    return "motor_fault";
                if (battery_.chargerConnected)
                    return "undock_charger_timeout";
                return "undock_failed";
            }
            return currentDominantEventReason();
        }

        if (beforeOp == "Dock" && afterOp == "GpsWait")
            return "gps_signal_lost";
        if (beforeOp == "Mow" && afterOp == "Dock" && batteryLowEventLatched_)
            return "battery_low";
        if (beforeOp == "NavToStart" && afterOp == "Dock" && gpsFixTimeoutLatched_)
            return "gps_fix_timeout";
        if (beforeOp == "Mow" && afterOp == "GpsWait")
            return "gps_signal_lost";
        if (beforeOp == "NavToStart" && afterOp == "GpsWait")
            return "gps_signal_lost";
        if (beforeOp == "Mow" && afterOp == "WaitRain")
            return "rain_detected";
        if ((beforeOp == "Mow" || beforeOp == "NavToStart" || beforeOp == "Dock") && afterOp == "EscapeReverse")
        {
            return "bumper_triggered";
        }
        return currentDominantEventReason();
    }

    void Robot::showUiNotice(const std::string &message,
                             const std::string &severity,
                             const std::string &eventReason,
                             uint64_t durationMs)
    {
        uiMessage_ = message;
        uiSeverity_ = severity;
        uiEventReason_ = eventReason;
        uiMessageUntil_ms_ = now_ms_ + durationMs;
    }

    // ── Accessors ─────────────────────────────────────────────────────────────────

    OdometryData Robot::lastOdometry() const { return odometry_; }
    SensorData Robot::lastSensors() const { return sensors_; }
    BatteryData Robot::lastBattery() const { return battery_; }

    // ── Private helpers ────────────────────────────────────────────────────────────

    void Robot::armMissionResumeGuard()
    {
        activeMissionMapFingerprint_ = currentMapFingerprint_;
        resumeBlockedByMapChange_ = false;
    }

    void Robot::updateStatusLeds()
    {
        const std::string opName = activeOpName();
        if (!odometry_.mcuConnected)
        {
            const bool blinkOn = ((now_ms_ / 500UL) % 2UL) == 0UL;
            hw_->setLed(LedId::LED_2, blinkOn ? LedState::RED : LedState::OFF);
        }
        else if (opName == "Error")
        {
            hw_->setLed(LedId::LED_2, LedState::RED);
        }
        else
        {
            hw_->setLed(LedId::LED_2, LedState::GREEN);
        }
        // LED_3: GPS quality (green = fix, red = no fix, off = no GPS)
        if (stateEst_.gpsHasFix())
        {
            hw_->setLed(LedId::LED_3, LedState::GREEN);
        }
        else if (stateEst_.gpsHasFloat())
        {
            hw_->setLed(LedId::LED_3, LedState::RED);
        }
        else
        {
            hw_->setLed(LedId::LED_3, LedState::OFF);
        }
    }

    WebSocketServer::TelemetryData Robot::buildTelemetry() const
    {
        WebSocketServer::TelemetryData d;
        const std::string opName = activeOpName();
        const float criticalV = config_->get<float>("battery_critical_v", 18.9f);
        const bool batteryCritical =
            battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < criticalV;
        const bool batteryLow =
            battery_.voltage > 0.1f && !battery_.chargerConnected && battery_.voltage < config_->get<float>("battery_low_v", 25.5f);
        const bool recoveryActive =
            !currentResumeTarget().empty() || opName == "GpsWait" || opName == "EscapeReverse" || opName == "EscapeForward" || resumeBlockedByMapChange_;
        const bool watchdogEventActive =
            uiMessageUntil_ms_ > now_ms_ && uiEventReason_ == "op_watchdog_timeout";

        d.op = activeOpName();
        d.x = stateEst_.x();
        d.y = stateEst_.y();
        d.heading = stateEst_.heading();
        d.battery_v = battery_.voltage;
        d.charge_v = battery_.chargeVoltage;
        d.charge_a = battery_.chargeCurrent;
        d.charger_connected = battery_.chargerConnected;
        // GPS quality: derive from StateEstimator state
        if (stateEst_.gpsHasFix())
        {
            d.gps_sol = 4;
            d.gps_text = "RTK";
        }
        else if (stateEst_.gpsHasFloat())
        {
            d.gps_sol = 5;
            d.gps_text = "Float";
        }
        else
        {
            d.gps_sol = 0;
            d.gps_text = "---";
        }
        d.gps_lat = lastGps_.lat;
        d.gps_lon = lastGps_.lon;
        d.gps_acc = lastGps_.hAccuracy;
        d.gps_num_sv = lastGps_.numSV;
        d.gps_num_corr_signals = lastGps_.numCorrSignals;
        d.gps_dgps_age_ms = lastGps_.dgpsAge_ms;
        d.bumper_l = sensors_.bumperLeft;
        d.bumper_r = sensors_.bumperRight;
        d.stop_button = sensors_.stopButton;
        d.lift = sensors_.lift;
        d.motor_err = sensors_.motorFault;
        d.mow_fault_pin = sensors_.mowFaultPin;
        d.mow_overload = sensors_.mowOverload;
        d.mow_permanent_fault = sensors_.mowPermanentFault;
        d.mow_ov_check = sensors_.mowOvCheck;
        d.uptime_s = now_ms_ / 1000UL;
        d.mcu_version = hw_->getMcuFirmwareVersion();

        const auto &imu = lastImu_;
        if (imu.valid)
        {
            d.imu_heading = imu.yaw * 180.0f / M_PI;
            d.imu_roll = imu.roll * 180.0f / M_PI;
            d.imu_pitch = imu.pitch * 180.0f / M_PI;
        }
        d.ekf_health = stateEst_.fusionMode();
        d.mcu_connected = odometry_.mcuConnected;
        d.mcu_comm_loss = mcuCommLossLatched_;
        d.gps_signal_lost = gpsNoSignalLatched_;
        d.gps_fix_timeout = gpsFixTimeoutLatched_;
        d.battery_low = batteryLow;
        d.battery_critical = batteryCritical;
        d.recovery_active = recoveryActive;
        d.watchdog_event_active = watchdogEventActive;
        d.ts_ms = now_ms_;
        if (Op *op = opMgr_.activeOp())
        {
            d.state_since_ms = op->startTime_ms;
        }
        d.state_phase = currentStatePhase();
        d.resume_target = currentResumeTarget();
        if (batteryCritical || opName == "Error" || mcuCommLossLatched_ || sensors_.lift || sensors_.motorFault || gpsFixTimeoutLatched_)
        {
            d.runtime_health = "fault";
        }
        else if (batteryLow || gpsNoSignalLatched_ || recoveryActive || watchdogEventActive)
        {
            d.runtime_health = "degraded";
        }
        else
        {
            d.runtime_health = "ok";
        }
        if (uiMessageUntil_ms_ > now_ms_)
        {
            d.ui_message = uiMessage_;
            d.ui_severity = uiSeverity_;
        }

        d.event_reason = currentDominantEventReason();
        d.history_backend_ready = historyDb_.enabled() && historyDb_.available();
        if (sessionActive_)
        {
            d.session_id = currentSession_.id;
            d.session_started_at_ms = currentSession_.startedAtMs;
        }
        d.mission_id = runtimeSnapshot_.missionId;
        d.mission_zone_count = static_cast<int>(activeMissionPlan_.zoneOrder.size());
        d.mission_zone_index = activeMissionZoneIndex();
        // N4.1: waypoint-level progress
        d.waypoint_index = runtimeSnapshot_.activePointIndex;
        d.waypoint_total = static_cast<int>(activeMissionPlan_.route.points.size());
        // N4.3: current navigation target
        d.target_x = runtimeSnapshot_.currentTarget.x;
        d.target_y = runtimeSnapshot_.currentTarget.y;
        d.has_target = runtimeSnapshot_.hasCurrentTarget;
        // N6.3: pending resume after dock
        d.has_interrupted_mission = hasInterruptedMission_;

        if (opName == "Error")
        {
            d.error_code = currentErrorCode();
        }

        {
            std::unique_lock<std::mutex> lk(diagMutex_);
            d.diag_active = diagReq_.active;
            d.diag_ticks = diagReq_.accTicks;
        }
        return d;
    }

    void Robot::checkBattery(OpContext &ctx)
    {
        const float lowV = config_->get<float>("battery_low_v", 25.5f);
        const float criticalV = config_->get<float>("battery_critical_v", 18.9f);

        if (battery_.voltage < 0.1f)
            return; // no MCU data yet
        if (battery_.chargerConnected)
        {
            batteryLowEventLatched_ = false;
            batteryCriticalEventLatched_ = false;
            return; // charging — no guard needed
        }

        if (battery_.voltage < criticalV)
        {
            if (!batteryCriticalEventLatched_)
            {
                const std::string message =
                    messages::humanReadableReasonMessage("battery_critical", "ERR_BATTERY_CRITICAL");
                recordEvent("error", "battery_event", "battery_critical",
                            message, "ERR_BATTERY_CRITICAL");
                showUiNotice(message, "error", "battery_critical", 12000);
                batteryCriticalEventLatched_ = true;
            }
            logger_->error(TAG, "Battery critical (" + std::to_string(battery_.voltage) + "V)");
            hw_->setMotorPwm(0, 0, 0);
            hw_->setBuzzer(true); // BUG-002 fix: alert user on critical battery
            if (Op *op = opMgr_.activeOp())
                op->onBatteryUndervoltage(ctx); // BUG-001 fix
            hw_->keepPowerOn(false);
            running_.store(false);
        }
        else if (battery_.voltage < lowV)
        {
            if (!batteryLowEventLatched_)
            {
                const std::string message = messages::humanReadableReasonMessage("battery_low");
                recordEvent("warn", "battery_event", "battery_low", message);
                showUiNotice(message, "warn", "battery_low", 6000);
                batteryLowEventLatched_ = true;
            }
            logger_->warn(TAG, "Battery low (" + std::to_string(battery_.voltage) + "V) — docking");
            if (Op *op = opMgr_.activeOp())
                op->onBatteryLowShouldDock(ctx); // BUG-001 fix
        }
        else
        {
            batteryLowEventLatched_ = false;
            batteryCriticalEventLatched_ = false;
        }
    }

    void Robot::monitorPerimeterSafety(OpContext &ctx)
    {
        const bool violated = !ctx.insidePerimeter;
        if (violated && previousInsidePerimeter_)
        {
            const std::string message =
                messages::humanReadableReasonMessage("perimeter_violated", "ERR_PERIMETER");
            recordEvent("error", "safety_event", "perimeter_violated", message, "ERR_PERIMETER");
            showUiNotice(message, "error", "perimeter_violated", 12000);
            if (Op *op = opMgr_.activeOp())
            {
                op->onPerimeterViolated(ctx);
            }
        }
        previousInsidePerimeter_ = ctx.insidePerimeter;
    }

    void Robot::monitorMapChangeSafety(OpContext &ctx)
    {
        if (resumeBlockedByMapChange_ && !previousResumeBlockedByMapChange_)
        {
            const std::string message =
                messages::humanReadableReasonMessage("resume_blocked_map_changed");
            recordEvent("warn", "map_event", "resume_blocked_map_changed", message);
            showUiNotice(message, "warn", "resume_blocked_map_changed", 8000);
            if (Op *op = opMgr_.activeOp())
            {
                op->onMapChanged(ctx);
            }
        }
        previousResumeBlockedByMapChange_ = resumeBlockedByMapChange_;
    }

    void Robot::monitorMcuConnectivity(OpContext &ctx)
    {
        const bool stmFlashMaintenance =
            stmFlashMaintenanceActive_.load() || steadyNowMs() < stmFlashMaintenanceUntil_ms_.load();

        if (stmFlashMaintenance)
        {
            if (ctx.odometry.mcuConnected)
            {
                mcuConnectedEver_ = true;
                if (mcuCommLossLatched_ && activeOpName() != "Error")
                {
                    mcuCommLossLatched_ = false;
                }
            }
            return;
        }

        if (ctx.odometry.mcuConnected)
        {
            mcuConnectedEver_ = true;
            if (mcuCommLossLatched_ && activeOpName() != "Error")
            {
                mcuCommLossLatched_ = false;
            }
            return;
        }

        if (!mcuConnectedEver_ || mcuCommLossLatched_)
            return;

        mcuCommLossLatched_ = true;
        clearActiveMissionTracking();
        driveController_.reset();
        hw_->setMotorPwm(0, 0, 0);

        const std::string message =
            messages::humanReadableReasonMessage("mcu_comm_lost", "ERR_MCU_COMMS");
        recordEvent("error", "safety_event", "mcu_comm_lost", message, "ERR_MCU_COMMS");
        showUiNotice(message, "error", "mcu_comm_lost", 12000);

        if (Op *op = opMgr_.activeOp())
        {
            op->requestOp(ctx, ctx.opMgr.error(), Op::PRIO_CRITICAL, false);
        }
    }

    void Robot::monitorStuckDetection(OpContext &ctx)
    {
        Op *const activeOp = opMgr_.activeOp();
        if (!activeOp || !ctx.odometry.mcuConnected || !ctx.stateEst)
        {
            stuckSince_ms_ = 0;
            return;
        }

        const std::string opName = activeOp->name();
        const bool supportedOp = opName == "Mow" || opName == "Dock" || opName == "Undock";
        if (!supportedOp)
        {
            stuckSince_ms_ = 0;
            return;
        }

        const float threshold = std::max(0.0f, ctx.config.get<float>("stuck_detect_min_speed_ms", 0.03f));
        const unsigned long timeoutMs = static_cast<unsigned long>(
            std::max(0, ctx.config.get<int>("stuck_detect_timeout_ms", 3000)));
        const float commandedSpeed = std::fabs(driveController_.lastCommandedLinear());
        const float measuredSpeed = std::fabs(ctx.stateEst->groundSpeed());
        const bool shouldBeMoving = commandedSpeed > threshold;
        const bool actuallyMoving = measuredSpeed > threshold;

        if (!shouldBeMoving || timeoutMs == 0)
        {
            stuckSince_ms_ = 0;
            return;
        }

        if (actuallyMoving)
        {
            stuckSince_ms_ = 0;
            stuckRecoveryCount_ = 0;
            return;
        }

        if (stuckSince_ms_ == 0)
        {
            stuckSince_ms_ = ctx.now_ms;
            return;
        }

        if (ctx.now_ms - stuckSince_ms_ < timeoutMs)
        {
            return;
        }

        stuckSince_ms_ = 0;
        ++stuckRecoveryCount_;
        const unsigned maxAttempts = std::max(1u, ctx.config.get<unsigned>("stuck_recovery_max_attempts", 3));
        if (stuckRecoveryCount_ >= maxAttempts)
        {
            stuckRecoveryExhaustedLatched_ = true;
            const std::string message =
                messages::humanReadableReasonMessage("stuck_recovery_exhausted", "ERR_STUCK");
            recordEvent("error", "safety_event", "stuck_recovery_exhausted", message, "ERR_STUCK");
            showUiNotice(message, "error", "stuck_recovery_exhausted", 12000);
            activeOp->requestOp(ctx, ctx.opMgr.error(), Op::PRIO_CRITICAL, false);
            return;
        }

        const std::string message = messages::humanReadableReasonMessage("stuck_detected");
        recordEvent("warn", "safety_event", "stuck_detected", message);
        showUiNotice(message, "warn", "stuck_detected", 6000);
        activeOp->onStuck(ctx);
    }

    void Robot::monitorMowOverload(OpContext &ctx)
    {
        // ESCAPE_LAWN: wenn der Mähmotor über einen konfigurierbaren Zeitraum
        // kontinuierlich überlastet ist, wird onStuck() ausgelöst — genau wie
        // bei mechanischer Blockierung. Nur aktiv während MowOp.
        if (!config_->get<bool>("escape_lawn_enabled", false))
            return;

        Op *const activeOp = opMgr_.activeOp();
        if (!activeOp || activeOp->name() != "Mow")
        {
            mowOverloadSince_ms_ = 0;
            return;
        }

        const bool overloaded = ctx.sensors.mowOverload || ctx.sensors.mowPermanentFault;
        if (!overloaded)
        {
            mowOverloadSince_ms_ = 0;
            return;
        }

        if (mowOverloadSince_ms_ == 0)
        {
            mowOverloadSince_ms_ = ctx.now_ms;
            return;
        }

        const unsigned long timeoutMs = static_cast<unsigned long>(
            ctx.config.get<int>("escape_lawn_timeout_ms", 3000));
        if (ctx.now_ms - mowOverloadSince_ms_ < timeoutMs)
            return;

        mowOverloadSince_ms_ = 0;
        const std::string message = messages::humanReadableReasonMessage("mow_overload_escape");
        recordEvent("warn", "safety_event", "mow_overload_escape", message);
        showUiNotice(message, "warn", "mow_overload_escape", 6000);
        activeOp->onStuck(ctx);
    }

    void Robot::monitorOpWatchdog(OpContext &ctx)
    {
        Op *op = opMgr_.activeOp();
        if (!op)
            return;

        const unsigned long timeoutMs = op->watchdogTimeoutMs(ctx);
        if (timeoutMs == 0 || ctx.now_ms < op->startTime_ms)
            return;

        if (ctx.now_ms - op->startTime_ms > timeoutMs)
        {
            const std::string message =
                messages::humanReadableReasonMessage("op_watchdog_timeout", "ERR_OP_TIMEOUT");
            recordEvent("error", "safety_event", "op_watchdog_timeout", message, "ERR_OP_TIMEOUT");
            showUiNotice(message, "error", "op_watchdog_timeout", 12000);
            op->onWatchdogTimeout(ctx);
        }
    }

    void Robot::monitorGpsResilience(OpContext &ctx)
    {
        // GPS resilience is only meaningful when a GPS driver is attached.
        if (!gps_)
            return;

        const unsigned long noSignalMs = static_cast<unsigned long>(
            config_->get<int>("gps_no_signal_ms", 3000));
        const unsigned long fixTimeoutMs = static_cast<unsigned long>(
            config_->get<int>("gps_fix_timeout_ms", 120000));
        const unsigned long recoverHysteresisMs = static_cast<unsigned long>(
            config_->get<int>("gps_recover_hysteresis_ms", 1000));

        const bool hasSignal = ctx.gpsHasFix || ctx.gpsHasFloat;

        if (hasSignal)
        {
            gpsSignalLostStart_ms_ = 0;
            if (gpsRecoveryStart_ms_ == 0)
                gpsRecoveryStart_ms_ = ctx.now_ms;

            // Clear latches only after stable recovery to avoid flapping.
            if ((gpsNoSignalLatched_ || gpsFixTimeoutLatched_) &&
                (ctx.now_ms - gpsRecoveryStart_ms_ >= recoverHysteresisMs))
            {
                gpsNoSignalLatched_ = false;
                gpsFixTimeoutLatched_ = false;
                const std::string message = messages::humanReadableReasonMessage("gps_recovered");
                recordEvent("info", "gps_event", "gps_recovered", message);
                showUiNotice(message, "info", "gps_recovered", 4000);
                logger_->info(TAG, "GPS recovered (hysteresis passed)");
            }
            return;
        }

        gpsRecoveryStart_ms_ = 0;
        if (gpsSignalLostStart_ms_ == 0)
            gpsSignalLostStart_ms_ = ctx.now_ms;

        Op *op = opMgr_.activeOp();
        if (!op)
            return;

        // Strongest event first: prolonged outage.
        if (!gpsFixTimeoutLatched_ && ctx.gpsFixAge_ms >= fixTimeoutMs)
        {
            gpsFixTimeoutLatched_ = true;
            gpsNoSignalLatched_ = true;
            const std::string message =
                messages::humanReadableReasonMessage("gps_fix_timeout", "ERR_GPS_TIMEOUT");
            recordEvent("error", "gps_event", "gps_fix_timeout",
                        message, "ERR_GPS_TIMEOUT");
            showUiNotice(message, "error", "gps_fix_timeout", 12000);
            op->onGpsFixTimeout(ctx);
            return;
        }

        // Short outage: degrade to GPS wait.
        if (!gpsNoSignalLatched_ && ctx.gpsFixAge_ms >= noSignalMs)
        {
            const unsigned long mowGpsCoastMs = static_cast<unsigned long>(
                config_->get<int>("mow_gps_coast_ms", 20000));
            const std::string fusionMode = ctx.stateEst ? ctx.stateEst->fusionMode() : "";
            const bool degradedFusionOperational =
                fusionMode == "EKF+IMU" || fusionMode == "Odo";
            const bool hadPriorGpsSignal = gpsSignalEver_;
            const bool mowingCoastAllowed =
                op->name() == "Mow" && mowGpsCoastMs > 0 && hadPriorGpsSignal && degradedFusionOperational && (ctx.now_ms - gpsSignalLostStart_ms_ < mowGpsCoastMs);

            if (mowingCoastAllowed)
            {
                return;
            }

            gpsNoSignalLatched_ = true;
            const std::string message = messages::humanReadableReasonMessage("gps_signal_lost");
            recordEvent("warn", "gps_event", "gps_signal_lost", message);
            showUiNotice(message, "warn", "gps_signal_lost", 6000);
            op->onGpsNoSignal(ctx);
        }
    }

    // ── Schedule methods (C.11) ────────────────────────────────────────────────────

    bool Robot::loadSchedule(const std::filesystem::path &path)
    {
        schedulePath_ = path;
        if (!schedule_.load(path))
        {
            logger_->warn(TAG, "No schedule file at " + path.string() + " — starting empty");
            return false;
        }
        logger_->info(TAG, "Schedule loaded: " + std::to_string(schedule_.entries().size()) + " entries");
        return true;
    }

    bool Robot::setSchedule(const nlohmann::json &arr)
    {
        if (!schedule_.fromJson(arr))
            return false;
        if (!schedulePath_.empty())
            schedule_.save(schedulePath_);
        return true;
    }

    nlohmann::json Robot::getSchedule() const
    {
        return schedule_.toJson();
    }

    // ── Diagnostic methods (C.10b) ─────────────────────────────────────────────────

    nlohmann::json Robot::diagRunMotor(const std::string &motor, float pwm,
                                       unsigned duration_ms, int revolutions)
    {
        if (activeOpName() != "Idle")
            return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
        if (motor != "left" && motor != "right" && motor != "mow")
            return {{"ok", false}, {"error", "Unbekannter Motor: " + motor}};

        const int ticksPerRev = config_->get<int>("ticks_per_revolution",
                                                  config_->get<int>("ticks_per_rev", 325));
        const int ticksTarget = (revolutions > 0)
                                    ? revolutions * ticksPerRev
                                    : 0;

        std::unique_lock<std::mutex> lk(diagMutex_);
        diagReq_ = DiagReq{};
        diagReq_.motor = motor;
        diagReq_.duration_ms = duration_ms;
        diagReq_.active = true;
        if (motor == "left")
        {
            diagReq_.leftPwm = pwm;
            diagReq_.leftTicksTarget = ticksTarget;
        }
        else if (motor == "right")
        {
            diagReq_.rightPwm = pwm;
            diagReq_.rightTicksTarget = ticksTarget;
        }
        else if (motor == "mow")
        {
            diagReq_.mowPwm = pwm;
            diagReq_.mowTicksTarget = ticksTarget;
        }
        const bool ok = diagCv_.wait_for(lk, std::chrono::seconds(15),
                                         [this]
                                         { return diagReq_.done; });
        if (!ok)
        {
            diagReq_.active = false;
            hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
            return {{"ok", false}, {"error", "Timeout"}};
        }
        const std::string result = diagReq_.resultJson;
        diagReq_ = {};
        try
        {
            return nlohmann::json::parse(result);
        }
        catch (const std::exception &e)
        {
            logger_->error(TAG, std::string("diagRunMotor result parse failed: ") + e.what());
            return {{"ok", false}, {"error", "Ungueltige Diagnose-Antwort"}};
        }
    }

    nlohmann::json Robot::diagImuCalib()
    {
        const std::string op = activeOpName();
        if (op != "Idle" && op != "Charge")
            return {{"ok", false}, {"error", "Nur im Idle- oder Charge-Zustand erlaubt"}};

        hw_->calibrateImu();
        logger_->info(TAG, "IMU-Kalibrierung gestartet — Roboter bitte nicht bewegen");
        imuCalibrationExpected_ = true;
        imuCalibrationActive_ = true;
        // The IMU driver completes calibration asynchronously in its 50 Hz update loop.
        // Returning immediately keeps the HTTP diag endpoint short-lived and avoids
        // tying the request lifetime to the full 5 s sensor calibration window.
        return {{"ok", true}, {"message", "IMU-Kalibrierung gestartet"}};
    }

    nlohmann::json Robot::diagMowMotor(bool on)
    {
        if (activeOpName() != "Idle")
            return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
        hw_->setMotorPwm(0, 0, on ? 255 : 0);
        return {{"ok", true}, {"on", on}};
    }

    nlohmann::json Robot::diagDriveStraight(float distance_m, float pwm)
    {
        if (activeOpName() != "Idle")
            return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
        if (distance_m <= 0.0f)
            return {{"ok", false}, {"error", "Strecke muss groesser als 0 sein"}};

        const int ticksPerRev = config_->get<int>("ticks_per_revolution",
                                                  config_->get<int>("ticks_per_rev", 325));
        const float wheelDiameter = config_->get<float>("wheel_diameter_m", 0.205f);
        const float wheelCircumference = std::max(0.001f, wheelDiameter * static_cast<float>(M_PI));
        const int ticksTarget = std::max(1, static_cast<int>(std::lround(distance_m / wheelCircumference * ticksPerRev)));
        const unsigned duration_ms = static_cast<unsigned>(
            std::clamp(distance_m / std::max(0.05f, std::abs(pwm)) * 4000.0f, 1000.0f, 90000.0f));

        std::unique_lock<std::mutex> lk(diagMutex_);
        diagReq_ = DiagReq{};
        diagReq_.motor = "both";
        diagReq_.leftPwm = pwm;
        diagReq_.rightPwm = pwm;
        diagReq_.duration_ms = duration_ms;
        diagReq_.leftTicksTarget = ticksTarget;
        diagReq_.rightTicksTarget = ticksTarget;
        diagReq_.targetDistance_m = distance_m;
        diagReq_.active = true;
        const unsigned waitSec = (duration_ms / 1000u) + 10u;
        const bool ok = diagCv_.wait_for(lk, std::chrono::seconds(waitSec),
                                         [this]
                                         { return diagReq_.done; });
        if (!ok)
        {
            diagReq_.active = false;
            hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
            return {{"ok", false}, {"error", "Timeout"}};
        }
        const std::string result = diagReq_.resultJson;
        diagReq_ = {};
        try
        {
            return nlohmann::json::parse(result);
        }
        catch (const std::exception &e)
        {
            logger_->error(TAG, std::string("diagDriveStraight result parse failed: ") + e.what());
            return {{"ok", false}, {"error", "Ungueltige Diagnose-Antwort"}};
        }
    }

    nlohmann::json Robot::diagTurnInPlace(float angle_deg, float pwm)
    {
        if (activeOpName() != "Idle")
            return {{"ok", false}, {"error", "Nur im Idle-Zustand erlaubt"}};
        if (std::abs(angle_deg) < 1.0f)
            return {{"ok", false}, {"error", "Winkel muss mindestens 1 Grad betragen"}};

        const int ticksPerRev = config_->get<int>("ticks_per_revolution",
                                                  config_->get<int>("ticks_per_rev", 325));
        const float wheelDiameter = config_->get<float>("wheel_diameter_m", 0.205f);
        const float wheelBase = config_->get<float>("wheel_base_m", 0.390f);
        const float wheelCircumference = std::max(0.001f, wheelDiameter * static_cast<float>(M_PI));
        const float arcLength = static_cast<float>(M_PI) * wheelBase * (std::abs(angle_deg) / 360.0f);
        const int ticksTarget = std::max(1, static_cast<int>(std::lround(arcLength / wheelCircumference * ticksPerRev)));
        const float absPwm = std::max(0.05f, std::abs(pwm));
        const float direction = angle_deg >= 0.0f ? 1.0f : -1.0f;
        const unsigned duration_ms = static_cast<unsigned>(
            std::clamp(std::abs(angle_deg) * 35.0f / absPwm, 1000.0f, 15000.0f));

        std::unique_lock<std::mutex> lk(diagMutex_);
        diagReq_ = DiagReq{};
        diagReq_.motor = "turn";
        diagReq_.leftPwm = -absPwm * direction;
        diagReq_.rightPwm = absPwm * direction;
        diagReq_.duration_ms = duration_ms;
        diagReq_.leftTicksTarget = ticksTarget;
        diagReq_.rightTicksTarget = ticksTarget;
        diagReq_.targetAngle_deg = angle_deg;
        diagReq_.active = true;
        const bool ok = diagCv_.wait_for(lk, std::chrono::seconds(30),
                                         [this]
                                         { return diagReq_.done; });
        if (!ok)
        {
            diagReq_.active = false;
            hw_->setMotorPwm(0.0f, 0.0f, 0.0f);
            return {{"ok", false}, {"error", "Timeout"}};
        }
        const std::string result = diagReq_.resultJson;
        diagReq_ = {};
        try
        {
            return nlohmann::json::parse(result);
        }
        catch (const std::exception &e)
        {
            logger_->error(TAG, std::string("diagTurnInPlace result parse failed: ") + e.what());
            return {{"ok", false}, {"error", "Ungueltige Diagnose-Antwort"}};
        }
    }

} // namespace sunray
