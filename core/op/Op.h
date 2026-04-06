#pragma once
// Op.h — Op State Machine base class, OpContext, and all Op declarations.
//
// Design differences from original sunray/src/op/op.h:
//   - No Arduino includes, no global variables, no CONSOLE macro
//   - Every method receives OpContext& instead of reading globals
//   - Op instances owned by OpManager (not global singletons)
//   - millis() replaced by OpContext::now_ms (from steady_clock in Robot)
//   - motor.*  replaced by OpContext::setMotorPwm / setLinearAngularSpeed
//   - battery.* replaced by OpContext::battery (BatteryData snapshot)
//   - Navigation stubs (x, y, isDocking) — filled by A.5 StateEstimator
//
// Op transition mechanism (unchanged logic):
//   Op::requestOp()   — sets pending op with priority
//   Op::changeOp()    — requestOp at PRIO_NORMAL
//   Op::checkStop()   — if shouldStop: end() → activate pending → begin()
//   OpManager::tick() — calls checkStop() + run() each loop iteration

#include "hal/HardwareInterface.h"
#include "core/Config.h"
#include "core/control/DifferentialDriveController.h"
#include "core/control/OpenLoopDriveController.h"
#include "core/Logger.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace sunray {

// ── Forward declarations ───────────────────────────────────────────────────────

class Op;
class OpManager;

// Forward declare navigation types to avoid circular includes.
// Definitions live in core/navigation/.
namespace nav {
    class StateEstimator;
    class Map;
    class LineTracker;
} // namespace nav

// ── OpContext ─────────────────────────────────────────────────────────────────
//
// Snapshot of robot state passed to every Op method each loop iteration.
// Robot populates this from its last sensor reads before calling OpManager::tick().

struct OpContext {
    OpContext(HardwareInterface& hwRef, Config& configRef, Logger& loggerRef, OpManager& opMgrRef)
        : hw(hwRef), config(configRef), logger(loggerRef), opMgr(opMgrRef) {}

    // ── Dependencies (stable, set once at Robot construction) ─────────────────
    HardwareInterface& hw;
    Config&            config;
    Logger&            logger;
    OpManager&         opMgr;

    // ── Sensor snapshots (updated every run() cycle) ──────────────────────────
    SensorData   sensors;
    BatteryData  battery;
    OdometryData odometry;

    // ── Navigation state (A.5 — populated by Robot::run() from StateEstimator) ──
    float x           = 0.0f;  ///< local metres east of origin
    float y           = 0.0f;  ///< local metres north of origin
    float heading     = 0.0f;  ///< radians, 0=east (atan2 convention)
    bool  insidePerimeter   = true;   ///< false → stop and dock
    bool  isDockingRoute    = false;  ///< map.isDocking() equivalent
    bool  gpsHasFix         = false;  ///< RTK fixed
    bool  gpsHasFloat       = false;  ///< at least RTK float
    unsigned long gpsFixAge_ms = 9999999; ///< ms since last valid GPS
    bool  resumeBlockedByMapChange = false; ///< true when route changed and resume is unsafe
    float imuYaw     = 0.0f;  ///< IMU yaw in radians (from lastImu_.yaw) — 0 if not valid
    bool  imuValid   = false; ///< true when IMU is responding and calibrated

    // ── Navigation objects (A.5 — null until loaded) ──────────────────────────
    nav::StateEstimator* stateEst    = nullptr;  ///< pose estimator (owned by Robot)
    nav::Map*            map         = nullptr;  ///< active waypoint map
    nav::LineTracker*    lineTracker = nullptr;  ///< Stanley controller
    control::DifferentialDriveController* driveController = nullptr; ///< wheel speed controller

    // ── Timing ─────────────────────────────────────────────────────────────────
    unsigned long now_ms = 0;  ///< Robot::run() timestamp (ms since epoch)
    unsigned long dt_ms  = 0;  ///< loop delta used for speed controller

    // ── Motor helpers ──────────────────────────────────────────────────────────

    /// Stop all motors immediately.
    void stopMotors() {
        if (driveController) driveController->reset();
        hw.setMotorPwm(0, 0, 0);
    }

    /// Set mow motor (on=true: PWM 200, off: PWM 0).
    void setMowMotor(bool on) { hw.setMotorPwm(0, 0, on ? 200 : 0); }

    /// Differential-drive speed control (replaces motor.setLinearAngularSpeed).
    ///   v:     linear velocity  (m/s, + = forward)
    ///   omega: angular velocity (rad/s, + = turn left)
    /// Converts to left/right PWM via DifferentialDriveController with
    /// OpenLoop fallback if no controller is attached.
    void setLinearAngularSpeed(float v, float omega) {
        const auto pwm = driveController
            ? driveController->compute(config, v, omega, odometry, dt_ms)
            : control::OpenLoopDriveController::compute(config, v, omega);
        hw.setMotorPwm(pwm.left, pwm.right, 0);  // mow unchanged by speed calls
    }
};

// ── Op ────────────────────────────────────────────────────────────────────────

class Op {
public:
    Op();
    virtual ~Op() = default;

    /// Human-readable name used in telemetry (must match Mission Service op strings).
    virtual std::string name() const { return "Op"; }

    // ── Priority levels ────────────────────────────────────────────────────────

    enum TransitionPriority : uint8_t {
        PRIO_LOW      = 10,
        PRIO_NORMAL   = 50,
        PRIO_HIGH     = 80,
        PRIO_CRITICAL = 100,
    };

    // ── Transition state ───────────────────────────────────────────────────────

    bool          initiatedByOperator = false;
    bool          shouldStop          = false;
    unsigned long startTime_ms        = 0;   ///< now_ms at begin()
    Op*           previousOp          = nullptr;
    Op*           nextOp              = nullptr;

    // ── Transition methods ─────────────────────────────────────────────────────

    /// Request transition to anOp at priority. Ignored if same op or lower priority
    /// than an already-pending request.
    void requestOp(OpContext& ctx, Op& anOp, uint8_t priority, bool returnBackOnExit = false);

    /// Convenience wrapper: requestOp at PRIO_NORMAL.
    void changeOp(OpContext& ctx, Op& anOp, bool returnBackOnExit = false);

    /// Operator command: map OperationType enum → specific Op.
    virtual void changeOperationTypeByOperator(OpContext& ctx, const std::string& opType);

    // ── Lifecycle ──────────────────────────────────────────────────────────────

    virtual void begin(OpContext& ctx);
    virtual void run(OpContext& ctx);
    virtual void end(OpContext& ctx);

    /// If shouldStop: end() → activate pending → begin().
    /// Called by OpManager::tick() before run().
    void checkStop(OpContext& ctx);

    // ── Events (all default to no-op; override in subclasses) ─────────────────

    virtual void onGpsNoSignal(OpContext& ctx);
    virtual void onGpsFixTimeout(OpContext& ctx);
    virtual void onRainTriggered(OpContext& ctx);
    virtual void onLiftTriggered(OpContext& ctx);
    virtual void onMotorError(OpContext& ctx);
    virtual void onObstacle(OpContext& ctx);
    virtual void onStuck(OpContext& ctx);
    virtual void onObstacleRotation(OpContext& ctx);
    virtual void onNoFurtherWaypoints(OpContext& ctx);
    virtual void onTargetReached(OpContext& ctx);
    virtual void onBatteryUndervoltage(OpContext& ctx);
    virtual void onBatteryLowShouldDock(OpContext& ctx);
    virtual void onChargerConnected(OpContext& ctx);
    virtual void onChargerDisconnected(OpContext& ctx);
    virtual void onChargingCompleted(OpContext& ctx);
    virtual void onBadChargingContact(OpContext& ctx);
    virtual void onKidnapped(OpContext& ctx, bool state);
    virtual void onImuTilt(OpContext& ctx);
    virtual void onImuError(OpContext& ctx);
    virtual void onPerimeterViolated(OpContext& ctx);
    virtual void onMapChanged(OpContext& ctx);
    virtual void onWatchdogTimeout(OpContext& ctx);
    virtual void onTimetableStartMowing(OpContext& ctx);
    virtual void onTimetableStopMowing(OpContext& ctx);
    virtual unsigned long watchdogTimeoutMs(const OpContext& ctx) const;

    // ── Op chain helpers ───────────────────────────────────────────────────────

    /// Follow nextOp chain to find the terminal (goal) Op.
    Op* getGoalOp();

    /// Returns goal Op name for logging/telemetry.
    std::string getOpChain() const;
};

// ── OpManager ─────────────────────────────────────────────────────────────────
//
// Owns all Op instances (no global singletons).
// Robot creates one OpManager and calls tick() each loop iteration.

class IdleOp;
class MowOp;
class DockOp;
class ChargeOp;
class UndockOp;
class NavToStartOp;
class WaitRainOp;
class EscapeReverseOp;
class EscapeForwardOp;
class GpsWaitFixOp;
class ErrorOp;

class OpManager {
public:
    OpManager();

    /// Called each loop iteration: checkStop() + run() on active op.
    void tick(OpContext& ctx);

    /// Direct access to the current active op.
    Op* activeOp() const { return activeOp_; }

    /// Request a transition (called from Op::requestOp).
    void setPending(Op& op, uint8_t priority, bool returnBackOnExit, Op& caller);

    /// Operator command shortcut.
    void changeOperationTypeByOperator(OpContext& ctx, const std::string& opType);

    // ── Op instances (owned here, not global) ─────────────────────────────────
    // Forward-declared above — definitions in each .cpp file
    IdleOp&          idle()    { return *idle_; }
    MowOp&           mow()     { return *mow_; }
    DockOp&          dock()    { return *dock_; }
    ChargeOp&        charge()  { return *charge_; }
    UndockOp&        undock()  { return *undock_; }
    NavToStartOp&    navToStart() { return *navToStart_; }
    WaitRainOp&      waitRain() { return *waitRain_; }
    EscapeReverseOp& escape()  { return *escape_; }
    GpsWaitFixOp&    gpsWait() { return *gpsWait_; }
    ErrorOp&         error()   { return *error_; }

private:
    // unique_ptr to avoid forward-declaration issues with inline constructors
    std::unique_ptr<IdleOp>          idle_;
    std::unique_ptr<MowOp>           mow_;
    std::unique_ptr<DockOp>          dock_;
    std::unique_ptr<ChargeOp>        charge_;
    std::unique_ptr<UndockOp>        undock_;
    std::unique_ptr<NavToStartOp>    navToStart_;
    std::unique_ptr<WaitRainOp>      waitRain_;
    std::unique_ptr<EscapeReverseOp> escape_;
    std::unique_ptr<EscapeForwardOp> escapeForward_;
    std::unique_ptr<GpsWaitFixOp>    gpsWait_;
    std::unique_ptr<ErrorOp>         error_;

    Op*     activeOp_      = nullptr;
    Op*     pendingOp_     = nullptr;
    uint8_t pendingPriority_ = 0;
};

// ── Individual Op declarations ────────────────────────────────────────────────

class IdleOp : public Op {
public:
    std::string name() const override { return "Idle"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
};

class MowOp : public Op {
public:
    std::string name() const override { return "Mow"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onGpsNoSignal(OpContext& ctx)         override;
    void onGpsFixTimeout(OpContext& ctx)       override;
    void onObstacle(OpContext& ctx)            override;
    void onStuck(OpContext& ctx)               override;
    void onLiftTriggered(OpContext& ctx)       override;
    void onMotorError(OpContext& ctx)          override;
    void onRainTriggered(OpContext& ctx)       override;
    void onBatteryLowShouldDock(OpContext& ctx)override;
    void onNoFurtherWaypoints(OpContext& ctx)  override;
    void onTimetableStopMowing(OpContext& ctx) override;
    void onKidnapped(OpContext& ctx, bool state) override;
    void onImuTilt(OpContext& ctx)             override;
    void onImuError(OpContext& ctx)            override;
    void onPerimeterViolated(OpContext& ctx)   override;
    void onMapChanged(OpContext& ctx)          override;
    void onBatteryUndervoltage(OpContext& ctx) override;
};

class DockOp : public Op {
public:
    int  mapRoutingFailedCounter = 0;
    int  nonProgressiveRetryCounter = 0;
    float retryStartX = 0.0f;
    float retryStartY = 0.0f;

    std::string name() const override { return "Dock"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onObstacle(OpContext& ctx)          override;
    void onStuck(OpContext& ctx)             override;
    void onLiftTriggered(OpContext& ctx)     override;
    void onMotorError(OpContext& ctx)        override;
    void onTargetReached(OpContext& ctx)     override;
    void onNoFurtherWaypoints(OpContext& ctx)override;
    void onGpsFixTimeout(OpContext& ctx)     override;
    void onGpsNoSignal(OpContext& ctx)       override;
    void onChargerConnected(OpContext& ctx)  override;
    void onKidnapped(OpContext& ctx, bool state) override;
    void onPerimeterViolated(OpContext& ctx) override;
    void onMapChanged(OpContext& ctx)        override;
    void onBatteryUndervoltage(OpContext& ctx) override;
    void onWatchdogTimeout(OpContext& ctx)   override;
    unsigned long watchdogTimeoutMs(const OpContext& ctx) const override;
};

class ChargeOp : public Op {
public:
    unsigned long retryTime_ms          = 0;
    unsigned long nextConsoleDetails_ms = 0;
    bool          retryTouchDock        = false;
    bool          chargingComplete_     = false;
    int           contactRetryCount     = 0;

    std::string name() const override { return "Charge"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onChargerDisconnected(OpContext& ctx)  override;
    void onBadChargingContact(OpContext& ctx)   override;
    void onBatteryUndervoltage(OpContext& ctx)  override;
    void onRainTriggered(OpContext& ctx)        override;
    void onChargerConnected(OpContext& ctx)     override;
    void onChargingCompleted(OpContext& ctx)    override;
    void onTimetableStartMowing(OpContext& ctx) override;
    void onTimetableStopMowing(OpContext& ctx)  override;
};

class UndockOp : public Op {
public:
    float startX = 0.0f;
    float startY = 0.0f;
    bool  chargerDropped = false;

    std::string name() const override { return "Undock"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onObstacle(OpContext& ctx)      override;
    void onStuck(OpContext& ctx)         override;
    void onLiftTriggered(OpContext& ctx) override;
    void onMotorError(OpContext& ctx)    override;
};

class NavToStartOp : public Op {
public:
    std::string name() const override { return "NavToStart"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onTargetReached(OpContext& ctx) override;
    void onNoFurtherWaypoints(OpContext& ctx) override;
    void onGpsNoSignal(OpContext& ctx) override;
    void onGpsFixTimeout(OpContext& ctx) override;
    void onObstacle(OpContext& ctx) override;
    void onLiftTriggered(OpContext& ctx) override;
    void onMotorError(OpContext& ctx) override;
    void onKidnapped(OpContext& ctx, bool state) override;
    void onImuTilt(OpContext& ctx) override;
    void onImuError(OpContext& ctx) override;
    void onPerimeterViolated(OpContext& ctx) override;
    void onMapChanged(OpContext& ctx) override;
    void onBatteryUndervoltage(OpContext& ctx) override;
};

class WaitRainOp : public Op {
public:
    std::string name() const override { return "WaitRain"; }
    void begin(OpContext& ctx) override;
    void run(OpContext& ctx) override;
    void end(OpContext& ctx) override;
    void onRainTriggered(OpContext& ctx) override;
    void onBatteryLowShouldDock(OpContext& ctx) override;

private:
    bool dockingRequested = false;
};

class EscapeReverseOp : public Op {
public:
    unsigned long driveReverseStopTime_ms    = 0;
    bool          hitLeft                    = false;
    bool          hitRight                   = false;
    bool          recoveringToPerimeter_     = false;  ///< true while driving forward back into perimeter
    unsigned long perimeterRecoveryStart_ms_ = 0;

    std::string name() const override { return "EscapeReverse"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onObstacle(OpContext& ctx) override;
    void onLiftTriggered(OpContext& ctx) override;
    void onBatteryUndervoltage(OpContext& ctx) override;
    void onKidnapped(OpContext& ctx, bool state) override;
    void onImuTilt(OpContext& ctx)  override;
    void onImuError(OpContext& ctx) override;
    void onPerimeterViolated(OpContext& ctx) override;
};

class EscapeForwardOp : public Op {
public:
    unsigned long driveForwardStopTime_ms = 0;

    std::string name() const override { return "EscapeForward"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
    void onObstacle(OpContext& ctx) override;
    void onLiftTriggered(OpContext& ctx) override;
    void onBatteryUndervoltage(OpContext& ctx) override;
    void onKidnapped(OpContext& ctx, bool state) override;
    void onImuTilt(OpContext& ctx)  override;
    void onImuError(OpContext& ctx) override;
    void onPerimeterViolated(OpContext& ctx) override;
};

class GpsWaitFixOp : public Op {
public:
    unsigned long waitStartTime_ms  = 0;
    bool          resetTriggered_   = false;  ///< true once mid-timeout reset has fired

    std::string name() const override { return "GpsWait"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
};

class ErrorOp : public Op {
public:
    unsigned long nextBuzzTime_ms = 0;

    std::string name() const override { return "Error"; }
    void begin(OpContext& ctx) override;
    void end(OpContext& ctx)   override;
    void run(OpContext& ctx)   override;
};

} // namespace sunray
