// GpsWaitFixOp.cpp — wait until GPS achieves RTK Fix/Float.
#include "core/op/Op.h"

namespace sunray {

// After this timeout without GPS fix, go to ErrorOp.
static constexpr unsigned long GPS_FIX_TIMEOUT_MS = 120000;  // 2 minutes

void GpsWaitFixOp::begin(OpContext& ctx) {
    ctx.logger.info("GpsWait", "waiting for GPS fix...");
    ctx.stopMotors();  // stops all motors including mow (setMotorPwm 0,0,0)
    waitStartTime_ms = ctx.now_ms;
}

void GpsWaitFixOp::end(OpContext&) {}

void GpsWaitFixOp::run(OpContext& ctx) {
    // GPS Float is sufficient to continue.
    if (ctx.gpsHasFloat || ctx.gpsHasFix) {
        ctx.logger.info("GpsWait", "GPS acquired => continue");
        if (nextOp != nullptr) {
            changeOp(ctx, *nextOp, false);
        } else {
            changeOp(ctx, ctx.opMgr.idle());
        }
        return;
    }

    // Timeout: GPS did not recover in time.
    if (ctx.now_ms - waitStartTime_ms > GPS_FIX_TIMEOUT_MS) {
        ctx.logger.error("GpsWait", "GPS fix timeout => ERROR");
        changeOp(ctx, ctx.opMgr.error());
    }
}

} // namespace sunray
