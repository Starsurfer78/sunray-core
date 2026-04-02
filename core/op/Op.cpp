// Op.cpp — Op base class + OpManager implementation.

#include "core/op/Op.h"

#include <algorithm>

namespace sunray {

// ── Op base ───────────────────────────────────────────────────────────────────

Op::Op() = default;

void Op::requestOp(OpContext& ctx, Op& anOp, uint8_t priority, bool returnBackOnExit) {
    if (&anOp == ctx.opMgr.activeOp()) return;
    ctx.opMgr.setPending(anOp, priority, returnBackOnExit, *this);
    shouldStop = true;
}

void Op::changeOp(OpContext& ctx, Op& anOp, bool returnBackOnExit) {
    requestOp(ctx, anOp, PRIO_NORMAL, returnBackOnExit);
}

void Op::changeOperationTypeByOperator(OpContext& ctx, const std::string& opType) {
    ctx.opMgr.changeOperationTypeByOperator(ctx, opType);
}

void Op::begin(OpContext&) {}
void Op::run(OpContext&)   {}
void Op::end(OpContext&)   {}

// ── Default event handlers ────────────────────────────────────────────────────

void Op::onGpsNoSignal(OpContext&)           {}
void Op::onGpsFixTimeout(OpContext&)         {}
void Op::onRainTriggered(OpContext&)         {}
void Op::onLiftTriggered(OpContext&)         {}
void Op::onMotorError(OpContext&)            {}
void Op::onObstacle(OpContext&)              {}
void Op::onObstacleRotation(OpContext&)      {}
void Op::onNoFurtherWaypoints(OpContext&)    {}
void Op::onTargetReached(OpContext&)         {}
void Op::onChargerConnected(OpContext&)      {}
void Op::onChargerDisconnected(OpContext&)   {}
void Op::onChargingCompleted(OpContext&)     {}
void Op::onBadChargingContact(OpContext&)    {}
void Op::onKidnapped(OpContext&, bool)       {}
void Op::onImuTilt(OpContext&)               {}
void Op::onImuError(OpContext&)              {}
void Op::onPerimeterViolated(OpContext&)     {}
void Op::onMapChanged(OpContext&)            {}
void Op::onWatchdogTimeout(OpContext& ctx)   { requestOp(ctx, ctx.opMgr.error(), PRIO_CRITICAL, false); }
void Op::onTimetableStartMowing(OpContext&)  {}
void Op::onTimetableStopMowing(OpContext&)   {}
unsigned long Op::watchdogTimeoutMs(const OpContext&) const { return 0; }

void Op::onBatteryUndervoltage(OpContext& ctx) {
    requestOp(ctx, ctx.opMgr.error(), PRIO_CRITICAL, false);
}
void Op::onBatteryLowShouldDock(OpContext&) {}

// ── Op chain helpers ──────────────────────────────────────────────────────────

Op* Op::getGoalOp() {
    Op* goal = this;
    int limit = 10;
    while (goal->nextOp != nullptr && limit-- > 0) {
        goal = goal->nextOp;
        if (goal == this) break;
    }
    return goal;
}

std::string Op::getOpChain() const {
    std::string chain = name();
    chain += "(operator=";
    chain += initiatedByOperator ? "1" : "0";
    chain += ")";
    const Op* cur = this;
    int limit = 10;
    while (cur->nextOp != nullptr && limit-- > 0) {
        cur = cur->nextOp;
        if (cur == this) break;
        chain += "->";
        chain += cur->name();
    }
    return chain;
}

// ── OpManager ─────────────────────────────────────────────────────────────────

OpManager::OpManager()
    : idle_         (std::make_unique<IdleOp>())
    , mow_          (std::make_unique<MowOp>())
    , dock_         (std::make_unique<DockOp>())
    , charge_       (std::make_unique<ChargeOp>())
    , undock_       (std::make_unique<UndockOp>())
    , navToStart_   (std::make_unique<NavToStartOp>())
    , waitRain_     (std::make_unique<WaitRainOp>())
    , escape_       (std::make_unique<EscapeReverseOp>())
    , escapeForward_(std::make_unique<EscapeForwardOp>())
    , gpsWait_      (std::make_unique<GpsWaitFixOp>())
    , error_        (std::make_unique<ErrorOp>())
    , activeOp_     (idle_.get())
{}

void OpManager::setPending(Op& op, uint8_t priority, bool returnBackOnExit, Op& caller) {
    if (&op == activeOp_) return;

    uint8_t minPrio = Op::PRIO_LOW;
    if (&op == error_.get()  || &op == charge_.get()) minPrio = Op::PRIO_HIGH;
    else if (&op == dock_.get() || &op == mow_.get() || &op == idle_.get()
          || &op == undock_.get() || &op == navToStart_.get()
          || &op == waitRain_.get()) minPrio = Op::PRIO_NORMAL;
    const uint8_t eff = std::max(priority, minPrio);

    if (pendingOp_ != nullptr && pendingPriority_ > eff) return;

    pendingOp_       = &op;
    pendingPriority_ = eff;
    op.previousOp    = &caller;
    op.nextOp        = returnBackOnExit ? &caller : nullptr;
}

void OpManager::tick(OpContext& ctx) {
    if (activeOp_ == nullptr) activeOp_ = idle_.get();

    // Flush pending transition
    if (pendingOp_ != nullptr) {
        activeOp_->shouldStop = false;
        activeOp_->end(ctx);

        activeOp_               = pendingOp_;
        activeOp_->startTime_ms = ctx.now_ms;
        activeOp_->shouldStop   = false;

        ctx.logger.info("OpManager", "==> " + activeOp_->getOpChain());

        pendingOp_       = nullptr;
        pendingPriority_ = 0;

        activeOp_->begin(ctx);
    }

    activeOp_->run(ctx);
}

void OpManager::changeOperationTypeByOperator(OpContext& ctx, const std::string& opType) {
    ctx.logger.info("OpManager", "operator cmd: " + opType);
    if (opType == "Idle" || opType == "stop") {
        activeOp_->requestOp(ctx, *idle_, Op::PRIO_CRITICAL, false);
        idle_->initiatedByOperator = true;
    } else if (opType == "Dock" || opType == "dock") {
        activeOp_->requestOp(ctx, *dock_, Op::PRIO_HIGH, false);
        dock_->initiatedByOperator = true;
    } else if (opType == "Mow" || opType == "start") {
        if (ctx.battery.chargerConnected) {
            activeOp_->requestOp(ctx, *undock_, Op::PRIO_HIGH, false);
            undock_->initiatedByOperator = true;
        } else {
            activeOp_->requestOp(ctx, *navToStart_, Op::PRIO_HIGH, false);
            navToStart_->initiatedByOperator = true;
        }
    } else if (opType == "Charge" || opType == "charge") {
        activeOp_->requestOp(ctx, *charge_, Op::PRIO_HIGH, false);
        charge_->initiatedByOperator = true;
    } else if (opType == "Undock" || opType == "undock") {
        activeOp_->requestOp(ctx, *undock_, Op::PRIO_HIGH, false);
        undock_->initiatedByOperator = true;
    } else if (opType == "NavToStart" || opType == "navtostart") {
        activeOp_->requestOp(ctx, *navToStart_, Op::PRIO_HIGH, false);
        navToStart_->initiatedByOperator = true;
    } else if (opType == "WaitRain" || opType == "waitrain") {
        activeOp_->requestOp(ctx, *waitRain_, Op::PRIO_HIGH, false);
        waitRain_->initiatedByOperator = true;
    } else if (opType == "EscapeReverse" || opType == "escapereverse") {
        activeOp_->requestOp(ctx, *escape_, Op::PRIO_HIGH, false);
        escape_->initiatedByOperator = true;
    } else if (opType == "EscapeForward" || opType == "escapeforward") {
        activeOp_->requestOp(ctx, *escapeForward_, Op::PRIO_HIGH, false);
        escapeForward_->initiatedByOperator = true;
    } else if (opType == "Error" || opType == "error") {
        activeOp_->requestOp(ctx, *error_, Op::PRIO_CRITICAL, false);
        error_->initiatedByOperator = true;
    } else {
        ctx.logger.warn("OpManager", "unknown op type: " + opType);
    }
}

} // namespace sunray
