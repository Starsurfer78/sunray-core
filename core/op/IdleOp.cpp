// IdleOp.cpp
#include "core/op/Op.h"

namespace sunray {

void IdleOp::begin(OpContext& ctx) {
    ctx.logger.info("Idle", "OP_IDLE");
    ctx.stopMotors();
}

void IdleOp::end(OpContext&) {}

void IdleOp::run(OpContext& ctx) {
    // If charger connected while idle → go to ChargeOp.
    // Grace period: if we just entered idle (e.g. brief stop during docking),
    // wait 2 seconds before treating charger as "operator placed on dock".
    if (ctx.battery.chargerConnected) {
        if (ctx.now_ms - startTime_ms > 2000) {
            ctx.logger.info("Idle", "charger connected (>2s) => CHARGE");
            if (initiatedByOperator) ctx.opMgr.charge().initiatedByOperator = true;
            changeOp(ctx, ctx.opMgr.charge());
        }
    }
}

} // namespace sunray
