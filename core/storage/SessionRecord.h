#pragma once

#include <string>

namespace sunray {

struct SessionRecord {
    std::string id;
    long long   startedAtMs       = 0;
    long long   endedAtMs         = 0;
    long long   durationMs        = 0;
    float       distanceM         = 0.0f;
    float       batteryStartV     = 0.0f;
    float       batteryEndV       = 0.0f;
    std::string endReason;
    float       meanGpsAccuracyM  = 0.0f;
    float       maxGpsAccuracyM   = 0.0f;
    std::string metadataJson      = "{}";
};

} // namespace sunray
