// GpsWaitFixOp.cpp — wait until GPS achieves RTK Fix/Float.
#include "core/op/Op.h"
#include <cstdlib>

namespace sunray {

void GpsWaitFixOp::begin(OpContext& ctx) {
    ctx.logger.info("GpsWait", "waiting for GPS fix...");
    ctx.stopMotors();  // stops all motors including mow (setMotorPwm 0,0,0)
    waitStartTime_ms = ctx.now_ms;
    resetTriggered_  = false;
}

void GpsWaitFixOp::end(OpContext&) {}

void GpsWaitFixOp::run(OpContext& ctx) {
    const unsigned long gpsFixTimeoutMs = static_cast<unsigned long>(
        ctx.config.get<int>("gps_fix_timeout_ms", 120000));
    const bool dockCriticalResume =
        nextOp != nullptr && nextOp->getGoalOp() != nullptr && nextOp->getGoalOp()->name() == "Dock";
    const bool gpsRecovered = dockCriticalResume ? ctx.gpsHasFix : (ctx.gpsHasFloat || ctx.gpsHasFix);

    if (gpsRecovered) {
        ctx.logger.info("GpsWait",
                        dockCriticalResume ? "GPS RTK Fix acquired for dock => continue"
                                           : "GPS acquired => continue");
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

    if (dockCriticalResume && ctx.gpsHasFloat) {
        ctx.logger.info("GpsWait", "GPS Float present but dock resume waits for RTK Fix");
    }

    // GPS_RESET_WAIT_FIX: fire a configurable shell command once at the
    // mid-point of the timeout to attempt GPS recovery before giving up.
    // Disabled by default — enable via config: "gps_reset_command": "systemctl restart ublox-gps"
    if (!resetTriggered_) {
        const std::string resetCmd = ctx.config.get<std::string>("gps_reset_command", "");
        if (!resetCmd.empty()) {
            const unsigned long resetMs = static_cast<unsigned long>(
                ctx.config.get<int>("gps_reset_wait_ms",
                    static_cast<int>(gpsFixTimeoutMs / 2)));
            if (ctx.now_ms - waitStartTime_ms >= resetMs) {
                resetTriggered_ = true;
                ctx.logger.warn("GpsWait", "GPS fix timeout midpoint — executing reset: " + resetCmd);
                std::system(resetCmd.c_str());
            }
        }
    }

    // Timeout: GPS did not recover in time.
    if (ctx.now_ms - waitStartTime_ms > gpsFixTimeoutMs) {
        ctx.logger.error("GpsWait", "GPS fix timeout => DOCK");
        changeOp(ctx, ctx.opMgr.dock());
    }
}

} // namespace sunray
