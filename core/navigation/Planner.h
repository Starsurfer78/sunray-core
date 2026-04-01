#pragma once

#include "core/navigation/PlannerContext.h"

#include <vector>

namespace sunray {
namespace nav {

class Map;

class Planner {
public:
    static bool segmentNeedsReplan(const Map& map, const PlannerContext& context);
    static RoutePlan planPath(const Map& map, const PlannerContext& context);
};

} // namespace nav
} // namespace sunray
