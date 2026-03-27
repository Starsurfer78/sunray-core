// ErrorOp.cpp — permanent error state, motors stopped, buzzer alert.
#include "core/op/Op.h"

namespace sunray {

static constexpr unsigned long BUZZ_INTERVAL_MS = 5000;

void ErrorOp::begin(OpContext& ctx) {
    ctx.logger.error("Error", "OP_ERROR — robot stopped");
    ctx.stopMotors();
    ctx.hw.setLed(LedId::LED_2, LedState::RED);
    nextBuzzTime_ms = ctx.now_ms + 1000;
}

void ErrorOp::end(OpContext& ctx) {
    ctx.hw.setLed(LedId::LED_2, LedState::GREEN);
    ctx.hw.setBuzzer(false);
}

void ErrorOp::run(OpContext& ctx) {
    // Motors stay off.
    ctx.stopMotors();

    // Periodic buzzer alert.
    if (ctx.now_ms >= nextBuzzTime_ms) {
        ctx.hw.setBuzzer(true);
        nextBuzzTime_ms = ctx.now_ms + BUZZ_INTERVAL_MS;
    } else if (ctx.now_ms >= nextBuzzTime_ms - BUZZ_INTERVAL_MS + 500) {
        // Short beep (500 ms on, rest off).
        ctx.hw.setBuzzer(false);
    }

    // Operator can manually clear via changeOperationTypeByOperator("Idle").
}

} // namespace sunray
