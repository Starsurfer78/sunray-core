#pragma once

#include "core/navigation/Route.h"

namespace sunray {
namespace nav {

struct PlannerContext {
    Point   robotPose;
    Point   source;
    Point   destination;
    WayType missionMode  = WayType::FREE;
    WayType planningMode = WayType::FREE;
    float   headingReferenceRad = 0.0f;
    bool    hasHeadingReference = false;
    bool    reverseAllowed = false;
    float   clearance_m = 0.25f;
    float   robotRadius_m = 0.0f;
};

} // namespace nav
} // namespace sunray
