#pragma once

#include "core/navigation/Map.h"
#include "core/navigation/Route.h"

#include <string>
#include <vector>

namespace sunray {
namespace nav {

// ── RouteValidationError ──────────────────────────────────────────────────────

enum class RouteErrorCode : uint8_t {
    ROUTE_EMPTY,                ///< route has no waypoints
    ROUTE_INVALID_TRANSITION,   ///< planner could not find A* path for a transition
    POINT_OUTSIDE_PERIMETER,    ///< waypoint is outside the allowed area
    POINT_IN_EXCLUSION,         ///< waypoint is inside a hard exclusion zone
    TRANSIT_OUTSIDE_ZONE,       ///< TRANSIT_WITHIN_ZONE segment leaves zone polygon
    SEGMENT_CROSSES_EXCLUSION,  ///< segment line crosses a hard exclusion polygon
    UNKNOWN_SEMANTIC,           ///< coverage/transit semantic is UNKNOWN (unset)
};

struct RouteValidationError {
    RouteErrorCode  code;
    int             pointIndex = -1;  ///< index of offending RoutePoint (-1 = route-level)
    std::string     message;
    std::string     zoneId;
};

// ── RouteValidationResult ─────────────────────────────────────────────────────

struct RouteValidationResult {
    bool valid = false;
    std::vector<RouteValidationError> errors;

    /// True if there is at least one error with the given code.
    bool hasError(RouteErrorCode code) const {
        for (const auto& e : errors) {
            if (e.code == code) return true;
        }
        return false;
    }
};

// ── RouteValidator ────────────────────────────────────────────────────────────

/// Validates a compiled RoutePlan before it is activated for a mowing mission.
///
/// Checks performed:
///   - Route is not empty
///   - Every MOW/COVERAGE point is inside the global allowed area
///   - Every MOW point is outside hard exclusions
///   - TRANSIT_WITHIN_ZONE segments stay within their zone polygon
///   - No segment crosses a hard exclusion polygon boundary
///   - No UNKNOWN semantics on coverage or transit points
///
/// Usage:
///   const RouteValidationResult result = RouteValidator::validate(map, route);
///   if (!result.valid) { /* log result.errors, reject mission start */ }
class RouteValidator {
public:
    static RouteValidationResult validate(const Map& map, const RoutePlan& route);

private:
    static bool segmentCrossesPolygon(const Point& a, const Point& b,
                                      const PolygonPoints& poly);
    static bool pointInPolygon(const PolygonPoints& poly, float px, float py);
};

} // namespace nav
} // namespace sunray
