#pragma once

/// TransitionRouter — simple binary-grid A* path planner for zone/dock transitions.
///
/// Input : from, to, Map (for perimeter + hard exclusions).
/// Output: sequence of Points forming a collision-free path.
///
/// Grid: binary (free / blocked). No cost layers. No inflation layers.
/// Cell size: ~15 cm. 8-connected A* with Euclidean heuristic.
/// Path is simplified by removing near-collinear intermediate points.
///
/// Three intended use-cases:
///   1. NavToZone    — navigate to the start point of a ZonePlan.
///   2. NavToDock    — navigate back to the charging station.
///   3. ObstacleDetour (N5) — temporary detour around a runtime obstacle.

#include "core/navigation/Route.h"

#include <vector>

namespace sunray
{
    // Forward declarations
    namespace nav
    {
        class Map;
    }

    namespace nav
    {

        struct TransitionRouterOptions
        {
            /// Allow path to leave the perimeter polygon (e.g. for dock approach).
            bool allowOutsidePerimeter = false;
            /// Grid cell size in metres (smaller = more accurate, slower to build).
            float cellSize_m = 0.15f;
        };

        class TransitionRouter
        {
        public:
            using Options = TransitionRouterOptions;

            /// Plan a path from @p from to @p to using the map geometry.
            /// @returns waypoint list including exact from/to endpoints;
            ///          empty vector if no path exists or inputs are invalid.
            static std::vector<Point> plan(
                const Point &from,
                const Point &to,
                const Map &map,
                const Options &opts = Options{});
        };

    } // namespace nav
} // namespace sunray
