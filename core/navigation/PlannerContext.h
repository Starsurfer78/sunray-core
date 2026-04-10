#pragma once

#include "core/navigation/Route.h"

namespace sunray
{
    namespace nav
    {

        struct PlannerContext
        {
            Point robotPose;
            Point source;
            Point lastTarget;
            Point destination;
            WayType missionMode = WayType::FREE;
            WayType planningMode = WayType::FREE;
            float headingReferenceRad = 0.0f;
            bool hasHeadingReference = false;
            bool reverseAllowed = false;
            float clearance_m = 0.25f;
            float robotRadius_m = 0.0f;
            RouteSemantic semantic = RouteSemantic::UNKNOWN;
            std::string zoneId;
            std::string componentId;

            /// Optional zone polygon for zone-aware MOW planning.
            /// When non-empty: the grid is extended to cover the full zone bounding box
            /// and cells outside this polygon are blocked, so A* stays within the zone.
            PolygonPoints constraintZone;
        };

    } // namespace nav
} // namespace sunray
