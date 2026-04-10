#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sunray
{
    namespace nav
    {

        struct Point
        {
            float x = 0.0f;
            float y = 0.0f;

            Point() = default;
            Point(float ax, float ay) : x(ax), y(ay) {}

            float distanceTo(const Point &o) const
            {
                float dx = x - o.x;
                float dy = y - o.y;
                return std::sqrt(dx * dx + dy * dy);
            }
        };

        using PolygonPoints = std::vector<Point>;

        /// A single position in a mowing plan.
        /// mowOn: when navigating TO this waypoint, the mow motor should be active.
        struct Waypoint
        {
            float x = 0.0f;
            float y = 0.0f;
            bool mowOn = true;
        };

        enum class WayType
        {
            PERIMETER,
            EXCLUSION,
            DOCK,
            MOW,
            FREE
        };

        /// Semantic classification of a route waypoint.
        /// Assigned by MissionCompiler; used for debug export and WebUI colour coding.
        enum class RouteSemantic : uint8_t
        {
            COVERAGE_EDGE,              ///< headland / perimeter mowing pass
            COVERAGE_INFILL,            ///< interior stripe (boustrophedon)
            TRANSIT_WITHIN_ZONE,        ///< A* transition inside one zone
            TRANSIT_BETWEEN_COMPONENTS, ///< transition between disconnected components of one zone
            TRANSIT_INTER_ZONE,         ///< A* transition between two zones
            DOCK_APPROACH,              ///< final approach to charging station
            RECOVERY,                   ///< replanned recovery segment
            UNKNOWN,                    ///< default / legacy
        };

        struct RoutePoint
        {
            Point p;
            bool reverse = false;
            bool slow = false;
            bool reverseAllowed = false;
            float clearance_m = 0.25f;
            WayType sourceMode = WayType::FREE;
            RouteSemantic semantic = RouteSemantic::UNKNOWN;
            std::string zoneId;      ///< ID of the active zone at this waypoint
            std::string componentId; ///< Connected working-area component inside the active zone
        };

        struct RouteSegment
        {
            Point lastTarget;
            Point target;
            bool reverse = false;
            bool slow = false;
            bool reverseAllowed = false;
            WayType sourceMode = WayType::FREE;
            float clearance_m = 0.25f;
            bool hasTargetHeading = false;
            float targetHeadingRad = 0.0f;
            float distanceToTarget_m = 0.0f;
            float distanceToLastTarget_m = 0.0f;
            bool nextSegmentStraight = true;
            bool targetReached = false;
            float targetReachedTolerance_m = 0.2f;
            float kidnapTolerance_m = 3.0f;
        };

        struct RoutePlan
        {
            std::vector<RoutePoint> points;
            WayType sourceMode = WayType::FREE;
            bool active = false;
            bool valid = true;         ///< false if any transition could not be planned
            std::string invalidReason; ///< human-readable reason when valid == false
        };

        struct ZonePlanSettingsSnapshot
        {
            float stripWidth_m = 0.18f;
            float angle_deg = 0.0f;
            bool edgeMowing = true;
            int edgeRounds = 1;
            float speed_mps = 1.0f;
            std::string pattern = "stripe";
            bool reverseAllowed = false;
            float clearance_m = 0.25f;
        };

        struct ZonePlan
        {
            std::string zoneId;
            std::string zoneName;
            ZonePlanSettingsSnapshot settings;
            RoutePlan route;
            Point startPoint;
            Point endPoint;
            bool hasStartPoint = false;
            bool hasEndPoint = false;
            bool valid = true;
            std::string invalidReason;

            /// Convenience view derived from RoutePlan for renderers/executors.
            std::vector<Waypoint> waypoints;
        };

        struct MissionPlan
        {
            std::string missionId;
            std::vector<std::string> zoneOrder;
            std::vector<ZonePlan> zonePlans;
            RoutePlan route;
            /// Renderer/executor view derived from RoutePlan.
            /// Kept as a convenience payload for WebUI/runtime adapters.
            std::vector<Waypoint> waypoints;
            bool valid = true;
            std::string invalidReason;
        };

        struct LocalDetour
        {
            RoutePlan route;
            Point reentryPoint;
            std::string resumeZoneId;
            int resumePointIndex = -1;
            bool hasReentryPoint = false;
            bool active = false;
        };

        struct RuntimeSnapshot
        {
            std::string missionId;
            std::string activeZoneId;
            int activePointIndex = -1;
            Point currentTarget;
            bool hasCurrentTarget = false;
            RoutePlan completedRoute;
            LocalDetour activeDetour;
            bool hasActiveMissionPlan = false;
            bool hasActiveZonePlan = false;
        };

        inline bool routePointMowOn(const RoutePoint &point)
        {
            switch (point.semantic)
            {
            case RouteSemantic::COVERAGE_EDGE:
            case RouteSemantic::COVERAGE_INFILL:
                return true;
            case RouteSemantic::TRANSIT_WITHIN_ZONE:
            case RouteSemantic::TRANSIT_BETWEEN_COMPONENTS:
                return true; // motor stays on during same-zone stripe transitions
            case RouteSemantic::TRANSIT_INTER_ZONE:
            case RouteSemantic::DOCK_APPROACH:
            case RouteSemantic::RECOVERY:
                return false;
            case RouteSemantic::UNKNOWN:
            default:
                return point.sourceMode == WayType::MOW;
            }
        }

        inline std::vector<Waypoint> routePlanToWaypoints(const RoutePlan &route)
        {
            std::vector<Waypoint> waypoints;
            waypoints.reserve(route.points.size());
            for (const auto &point : route.points)
            {
                waypoints.push_back({point.p.x, point.p.y, routePointMowOn(point)});
            }
            return waypoints;
        }

    } // namespace nav
} // namespace sunray
