// GpsWaitFixOp.cpp — wait until GPS achieves RTK Fix/Float.
#include "core/op/Op.h"

namespace sunray {

void GpsWaitFixOp::begin(OpContext& ctx) {
    ctx.logger.info("GpsWait", "waiting for GPS fix...");
    ctx.stopMotors();  // stops all motors including mow (setMotorPwm 0,0,0)
    waitStartTime_ms = ctx.now_ms;
}

void GpsWaitFixOp::end(OpContext&) {}

void GpsWaitFixOp::run(OpContext& ctx) {
    const unsigned long gpsFixTimeoutMs = static_cast<unsigned long>(
        ctx.config.get<int>("gps_fix_timeout_ms", 120000));

    // GPS Float is sufficient to continue.
    if (ctx.gpsHasFloat || ctx.gpsHasFix) {
        ctx.logger.info("GpsWait", "GPS acquired => continue");
        if (ctx.resumeBlockedByMapChange) {
            ctx.logger.warn("GpsWait", "resume blocked by map change => IDLE");
            changeOp(ctx, ctx.opMgr.idle());
            return;
        }
        if (nextOp != nullptr) {
            changeOp(ctx, *nextOp, false);
        } else {
            changeOp(ctx, ctx.opMgr.idle());
        }
        return;
    }

    // Timeout: GPS did not recover in time.
    if (ctx.now_ms - waitStartTime_ms > gpsFixTimeoutMs) {
        ctx.logger.error("GpsWait", "GPS fix timeout => DOCK");
        changeOp(ctx, ctx.opMgr.dock());
    }
}

} // namespace sunray
