#pragma once

#include <cmath>
#include <vector>

namespace sunray {
namespace nav {

struct Point {
    float x = 0.0f;
    float y = 0.0f;

    Point() = default;
    Point(float ax, float ay) : x(ax), y(ay) {}

    float distanceTo(const Point& o) const {
        float dx = x - o.x;
        float dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

using PolygonPoints = std::vector<Point>;

enum class WayType { PERIMETER, EXCLUSION, DOCK, MOW, FREE };

struct RoutePoint {
    Point   p;
    bool    reverse    = false;
    bool    slow       = false;
    bool    reverseAllowed = false;
    float   clearance_m = 0.25f;
    WayType sourceMode = WayType::FREE;
};

struct RouteSegment {
    Point   lastTarget;
    Point   target;
    bool    reverse            = false;
    bool    slow               = false;
    bool    reverseAllowed     = false;
    WayType sourceMode         = WayType::FREE;
    float   clearance_m        = 0.25f;
    bool    hasTargetHeading   = false;
    float   targetHeadingRad   = 0.0f;
    float   distanceToTarget_m = 0.0f;
    float   distanceToLastTarget_m = 0.0f;
    bool    nextSegmentStraight = true;
    bool    targetReached      = false;
    float   targetReachedTolerance_m = 0.2f;
    float   kidnapTolerance_m  = 3.0f;
};

struct RoutePlan {
    std::vector<RoutePoint> points;
    WayType                 sourceMode = WayType::FREE;
    bool                    active = false;
};

} // namespace nav
} // namespace sunray
