#pragma once

namespace sunray::control {

class PidController {
public:
    void reset();

    float update(float error, float dt_s, float kp, float ki, float kd);

private:
    float integral_ = 0.0f;
    float previousError_ = 0.0f;
    bool initialized_ = false;
};

} // namespace sunray::control
