#pragma once

#include "core/navigation/Map.h"
#include "core/navigation/ZonePlanner.h"
#include "core/navigation/TransitionRouter.h"

#include <nlohmann/json.hpp>

namespace sunray
{
    namespace nav
    {

        /// Build a full preview plan for a single zone.
        /// This is the greenfield planning object that the Mission editor should render
        /// for the active zone and that `Jetzt maehen` should activate directly.
        ZonePlan buildZonePlanPreview(const Map &map,
                                      const Zone &zone,
                                      const nlohmann::json &overrides = nlohmann::json::object());

        /// Build the full preview plan for a mission.
        /// The mission plan is the single source of truth for mission preview, start,
        /// and dashboard rendering.
        MissionPlan buildMissionPlanPreview(const Map &map, const nlohmann::json &missionJson);

        /// Build a full mowing preview route for a mission.
        /// The route contains edge rounds, infill stripes and planner-generated
        /// transitions between disconnected parts.
        ///
        /// Legacy adapter: prefer buildMissionPlanPreview() for new code.
        RoutePlan buildMissionMowRoutePreview(const Map &map, const nlohmann::json &missionJson);

    } // namespace nav
} // namespace sunray
