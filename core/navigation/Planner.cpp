#include "core/navigation/Planner.h"

#include "core/navigation/GridMap.h"
#include "core/navigation/Map.h"

#include <algorithm>
#include <cmath>

namespace sunray
{
    namespace nav
    {

        namespace
        {

            bool lineCircleIntersects(const Point &src, const Point &dst,
                                      const Point &center, float radius)
            {
                const float dx = dst.x - src.x;
                const float dy = dst.y - src.y;
                const float len2 = dx * dx + dy * dy;
                if (len2 < 1e-8f)
                    return src.distanceTo(center) < radius;
                float t = ((center.x - src.x) * dx + (center.y - src.y) * dy) / len2;
                t = std::clamp(t, 0.0f, 1.0f);
                const float cx = src.x + t * dx - center.x;
                const float cy = src.y + t * dy - center.y;
                return (cx * cx + cy * cy) < radius * radius;
            }

        } // namespace

        bool Planner::segmentNeedsReplan(const Map &map, const PlannerContext &context)
        {
            const Point segmentStart =
                (map.lastTargetPoint.distanceTo(context.robotPose) < 0.25f) ? map.lastTargetPoint : context.robotPose;
            const Point segmentEnd = context.destination;

            for (const auto &obs : map.obstacles_)
            {
                const float radius = obs.radius_m + map.planner_.obstacleInflation_m;
                if (lineCircleIntersects(segmentStart, segmentEnd, obs.center, radius))
                {
                    return true;
                }
            }
            return false;
        }

        RoutePlan Planner::planPath(const Map &map, const PlannerContext &context)
        {
            const float cellSize = std::max(0.05f, map.planner_.gridCellSize_m);
            const float margin = std::max(2.0f, context.clearance_m * 4.0f);
            const float originX = (context.source.x + context.destination.x) * 0.5f;
            const float originY = (context.source.y + context.destination.y) * 0.5f;

            // Base grid covers source→destination span plus a fixed margin.
            float spanX = std::fabs(context.destination.x - context.source.x) + margin * 2.0f;
            float spanY = std::fabs(context.destination.y - context.source.y) + margin * 2.0f;

            // If a constraint zone is provided, extend the grid to cover its full
            // bounding box so A* can find detour paths around the zone boundary.
            // Without this, the grid would be too narrow to route around obstacles
            // when source and destination share a similar coordinate.
            if (!context.constraintZone.empty())
            {
                for (const auto &p : context.constraintZone)
                {
                    spanX = std::max(spanX, std::fabs(p.x - originX) * 2.0f + margin * 2.0f);
                    spanY = std::max(spanY, std::fabs(p.y - originY) * 2.0f + margin * 2.0f);
                }
            }

            // Cap grid size to keep planning time bounded (200×200 = 40 000 cells max).
            const int cols = std::min(200, std::max(GridMap::DEFAULT_COLS,
                                                    static_cast<int>(std::ceil(spanX / cellSize))));
            const int rows = std::min(200, std::max(GridMap::DEFAULT_ROWS,
                                                    static_cast<int>(std::ceil(spanY / cellSize))));

            GridMap gridMap;
            gridMap.build(map, originX, originY, cellSize, cols, rows,
                          context.robotRadius_m,
                          context.planningMode,
                          context.constraintZone);
            if (!gridMap.isBuilt())
                return {};

            auto path = gridMap.planPath(context.source, context.destination);
            if (path.empty())
                return {};
            path = gridMap.smoothPath(path);

            std::vector<Point> candidatePath;
            candidatePath.reserve(path.size() + 1);
            candidatePath.push_back(context.source);
            candidatePath.insert(candidatePath.end(), path.begin(), path.end());
            if (!map.pathAllowed(candidatePath))
                return {};

            RoutePlan route;
            route.sourceMode = context.missionMode;
            route.active = true;
            route.points.reserve(path.size());

            bool useReverse = false;
            if (context.reverseAllowed && !path.empty() && context.hasHeadingReference)
            {
                const float forwardHeading = std::atan2(path.front().y - context.source.y,
                                                        path.front().x - context.source.x);
                const float reverseHeading = Map::scalePI(forwardHeading + static_cast<float>(M_PI));
                const float forwardError = std::fabs(Map::distancePI(context.headingReferenceRad, forwardHeading));
                const float reverseError = std::fabs(Map::distancePI(context.headingReferenceRad, reverseHeading));
                const float switchingMargin = 10.0f / 180.0f * static_cast<float>(M_PI);
                useReverse = (reverseError + switchingMargin) < forwardError;
            }

            for (const auto &waypoint : path)
            {
                RoutePoint routePoint;
                routePoint.p = waypoint;
                routePoint.reverse = useReverse;
                routePoint.reverseAllowed = context.reverseAllowed;
                routePoint.clearance_m = context.clearance_m;
                routePoint.slow = (context.planningMode == WayType::DOCK) && (waypoint.distanceTo(context.destination) <= map.dockMeta_.slowZoneRadius_m);
                routePoint.sourceMode = context.missionMode;
                routePoint.semantic = context.semantic;
                routePoint.zoneId = context.zoneId;
                routePoint.componentId = context.componentId;
                route.points.push_back(routePoint);
            }

            return route;
        }

    } // namespace nav
} // namespace sunray
