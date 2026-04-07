#pragma once

#include <cmath>
#include <cstdint>
#include <string>
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

/// Semantic classification of a route waypoint.
/// Assigned by MowRoutePlanner; used for debug export and WebUI colour coding.
enum class RouteSemantic : uint8_t {
    COVERAGE_EDGE,        ///< headland / perimeter mowing pass
    COVERAGE_INFILL,      ///< interior stripe (boustrophedon)
    TRANSIT_WITHIN_ZONE,  ///< A* transition inside one zone
    TRANSIT_INTER_ZONE,   ///< A* transition between two zones
    DOCK_APPROACH,        ///< final approach to charging station
    RECOVERY,             ///< replanned recovery segment
    UNKNOWN,              ///< default / legacy
};

struct RoutePoint {
    Point          p;
    bool           reverse       = false;
    bool           slow          = false;
    bool           reverseAllowed = false;
    float          clearance_m   = 0.25f;
    WayType        sourceMode    = WayType::FREE;
    RouteSemantic  semantic      = RouteSemantic::UNKNOWN;
    std::string    zoneId;        ///< ID of the active zone at this waypoint
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
    bool                    valid  = true;   ///< false if any transition could not be planned
    std::string             invalidReason;   ///< human-readable reason when valid == false
};

} // namespace nav
} // namespace sunray
