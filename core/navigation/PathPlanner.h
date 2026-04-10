#pragma once

#include "core/navigation/PlannerContext.h"

#include <vector>

namespace sunray
{
    namespace nav
    {

        class Map;

        /// PathPlanner — dynamic obstacle-aware online path planner.
        /// Wraps GridMap for runtime replanning during mission execution.
        class PathPlanner
        {
        public:
            static bool segmentNeedsReplan(const Map &map, const PlannerContext &context);
            static RoutePlan planPath(const Map &map, const PlannerContext &context);
        };

    } // namespace nav
} // namespace sunray
