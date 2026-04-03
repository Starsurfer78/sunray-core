#pragma once

#include "core/navigation/Map.h"

#include <nlohmann/json.hpp>

namespace sunray {
namespace nav {

/// Build a full mowing preview route for a mission.
/// The route contains edge rounds, infill stripes and planner-generated
/// transitions between disconnected parts.
RoutePlan buildMissionMowRoutePreview(const Map& map, const nlohmann::json& missionJson);

} // namespace nav
} // namespace sunray
