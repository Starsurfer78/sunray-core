#pragma once

#include <optional>
#include <string>

namespace sunray {

struct EventRecord {
    unsigned long          ts_ms       = 0;
    std::string            level       = "info";
    std::string            module;
    std::string            eventType;
    std::string            statePhase;
    std::string            eventReason;
    std::string            errorCode;
    std::string            message;
    std::optional<float>   batteryV;
    std::optional<int>     gpsSol;
    std::optional<float>   x;
    std::optional<float>   y;
};

} // namespace sunray
