// ManualDriveOp.cpp — active while the robot is driven via app joystick.
//
// Lifecycle managed by Robot::tickManualDrive:
//   Idle + fresh drive command  →  changeOp("Manual")
//   Manual + stale (>2 s)       →  changeOp("Idle")
//   Lift persists               →  onLiftTriggered → Idle + motors off
//
// Deliberate omissions:
//   - LineTracker not ticked  (no route, no kidnap detection)
//   - Perimeter check disabled (user intentionally moves anywhere)
//   - Watchdog timer disabled  (timeout handled in tickManualDrive)

#include "Op.h"
#include "../navigation/LineTracker.h"

namespace sunray {

void ManualDriveOp::begin(OpContext &ctx)
{
    ctx.logger.info("Manual", "joystick control active");
    // Clear any stale kidnap flag left over from a previous mow session.
    if (ctx.lineTracker)
        ctx.lineTracker->reset();
    ctx.stopMotors();
}

void ManualDriveOp::run(OpContext &ctx)
{
    // Drive commands and the 2-second watchdog are handled entirely in
    // Robot::tickManualDrive so that the motor PWM path stays in one place.
    (void)ctx;
}

void ManualDriveOp::end(OpContext &ctx)
{
    ctx.logger.info("Manual", "joystick control ended");
    ctx.stopMotors();
}

void ManualDriveOp::onLiftTriggered(OpContext &ctx)
{
    ctx.logger.warn("Manual", "lift sensor active — stopping manual drive");
    ctx.stopMotors();
    changeOp(ctx, ctx.opMgr.idle());
}

} // namespace sunray
