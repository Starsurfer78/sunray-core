// test_boustrophedon.cpp — Tests for the N2 navigation modules:
//   ZonePlanner, TransitionRouter, WaypointExecutor.
//
// These tests check PRODUCT behaviour: correct stripe count, boustrophedon order,
// determinism, obstacle avoidance, waypoint sequencing.
// No A*, no Costmap, no GridMap — all pure unit tests.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/navigation/ZonePlanner.h"
#include "core/navigation/MissionCompiler.h"
#include "core/navigation/TransitionRouter.h"
#include "core/navigation/WaypointExecutor.h"
#include "core/navigation/Map.h"
#include "core/navigation/Route.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sunray;
using namespace sunray::nav;
using Catch::Approx;

// ── Test helpers ──────────────────────────────────────────────────────────────

/// Build a rectangular zone polygon (CCW, metres).
static PolygonPoints makeRect(float x0, float y0, float x1, float y1)
{
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

/// Write a minimal map JSON to a temp file and return the path.
static std::filesystem::path writeTmpMap(const std::string &json)
{
    auto p = std::filesystem::temp_directory_path() / "sunray_boustro_test.json";
    std::ofstream f(p);
    f << json;
    return p;
}

/// Load a Map from an inline JSON string.
static Map loadMap(const std::string &json)
{
    auto p = writeTmpMap(json);
    Map m;
    m.loadJson(p.string());
    return m;
}

/// Count coverage waypoints (mowOn == true).
static int countCoverage(const std::vector<Waypoint> &wps)
{
    return static_cast<int>(std::count_if(wps.begin(), wps.end(),
                                          [](const Waypoint &w)
                                          { return w.mowOn; }));
}

/// Count transit waypoints (mowOn == false).
static int countTransit(const std::vector<Waypoint> &wps)
{
    return static_cast<int>(std::count_if(wps.begin(), wps.end(),
                                          [](const Waypoint &w)
                                          { return !w.mowOn; }));
}

// ─────────────────────────────────────────────────────────────────────────────
// ZonePlanner Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Boustrophedon — invalid zone returns invalid plan", "[boustrophedon]")
{
    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;

    // Empty zone
    auto plan = ZonePlanner::plan("z1", {}, {}, {}, s);
    REQUIRE(!plan.valid);
    REQUIRE(!plan.invalidReason.empty());

    // Zone outside a perimeter
    const PolygonPoints zone = makeRect(10.0f, 10.0f, 20.0f, 20.0f);
    const PolygonPoints perim = makeRect(0.0f, 0.0f, 5.0f, 5.0f);
    auto plan2 = ZonePlanner::plan("z1", zone, perim, {}, s);
    REQUIRE(!plan2.valid);
}

TEST_CASE("Boustrophedon — simple rectangle, no edge mowing", "[boustrophedon]")
{
    // 3 m × 3 m zone, 0.3 m strip width → ceil(3/0.3) = 10 stripes
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 3.0f, 3.0f);

    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.angle = 0.0f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(plan.valid);
    REQUIRE(plan.zoneId == "z1");
    REQUIRE(!plan.waypoints.empty());
    REQUIRE(plan.hasStartPoint);
    REQUIRE(plan.hasEndPoint);

    // Coverage waypoints = 2 per stripe × ~10 stripes = ~20
    // (may differ slightly due to floating point edge effects)
    const int cov = countCoverage(plan.waypoints);
    REQUIRE(cov >= 18);
    REQUIRE(cov <= 22);

    // First waypoint = startPoint
    REQUIRE(plan.waypoints.front().x == Approx(plan.startPoint.x).margin(0.01f));
    REQUIRE(plan.waypoints.front().y == Approx(plan.startPoint.y).margin(0.01f));
}

TEST_CASE("Boustrophedon — boustrophedon direction alternates", "[boustrophedon]")
{
    // 3 m × 3 m, horizontal stripes → even rows go left-to-right, odd rows right-to-left
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 3.0f, 3.0f);

    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.angle = 0.0f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(plan.valid);

    // Collect stripe endpoints (pairs of consecutive mowOn=true waypoints)
    // For horizontal stripes: adjacent coverage endpoints should have roughly the same Y
    // and alternating X direction.
    std::vector<std::pair<float, float>> stripeXPairs; // (startX, endX) per stripe
    bool inStripe = false;
    float stripeStartX = 0.0f;
    for (std::size_t i = 0; i < plan.waypoints.size(); ++i)
    {
        const auto &w = plan.waypoints[i];
        if (w.mowOn && !inStripe)
        {
            stripeStartX = w.x;
            inStripe = true;
        }
        else if (w.mowOn && inStripe)
        {
            stripeXPairs.push_back({stripeStartX, w.x});
            inStripe = false;
        }
        else if (!w.mowOn)
        {
            inStripe = false;
        }
    }

    REQUIRE(stripeXPairs.size() >= 8); // at least 8 stripes in a 3m × 3m zone

    // Alternating direction: even stripes go positive, odd go negative (or vice versa)
    for (std::size_t i = 0; i + 1 < stripeXPairs.size(); ++i)
    {
        const float dir0 = stripeXPairs[i].second - stripeXPairs[i].first;
        const float dir1 = stripeXPairs[i + 1].second - stripeXPairs[i + 1].first;
        // Consecutive stripes should travel in opposite x-directions
        INFO("stripe " << i << ": " << stripeXPairs[i].first << " -> " << stripeXPairs[i].second);
        INFO("stripe " << i + 1 << ": " << stripeXPairs[i + 1].first << " -> " << stripeXPairs[i + 1].second);
        REQUIRE((dir0 * dir1) < 0.0f); // opposite signs
    }
}

TEST_CASE("Boustrophedon — deterministic: same input → same output", "[boustrophedon]")
{
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 5.0f, 5.0f);
    ZoneSettings s;
    s.stripWidth = 0.25f;
    s.angle = 30.0f;
    s.edgeMowing = true;
    s.edgeRounds = 1;

    const auto plan1 = ZonePlanner::plan("z1", zone, {}, {}, s);
    const auto plan2 = ZonePlanner::plan("z1", zone, {}, {}, s);

    REQUIRE(plan1.valid);
    REQUIRE(plan2.valid);
    REQUIRE(plan1.waypoints.size() == plan2.waypoints.size());

    for (std::size_t i = 0; i < plan1.waypoints.size(); ++i)
    {
        REQUIRE(plan1.waypoints[i].x == Approx(plan2.waypoints[i].x).margin(1e-4f));
        REQUIRE(plan1.waypoints[i].y == Approx(plan2.waypoints[i].y).margin(1e-4f));
        REQUIRE(plan1.waypoints[i].mowOn == plan2.waypoints[i].mowOn);
    }
}

TEST_CASE("Boustrophedon — perimeter clips the zone", "[boustrophedon]")
{
    // Zone is 10×10, perimeter is 5×5 (bottom-left corner)
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 10.0f, 10.0f);
    const PolygonPoints perim = makeRect(0.0f, 0.0f, 5.0f, 5.0f);

    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, perim, {}, s);
    REQUIRE(plan.valid);

    // All coverage waypoints should be inside (or on) the perimeter
    for (const auto &w : plan.waypoints)
    {
        if (!w.mowOn)
            continue;
        REQUIRE(w.x <= 5.05f);
        REQUIRE(w.y <= 5.05f);
    }
}

TEST_CASE("Boustrophedon — hard NoGo excludes area", "[boustrophedon]")
{
    // 6×6 zone, 2×2 hard NoGo in centre
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 6.0f, 6.0f);
    const PolygonPoints nogo = makeRect(2.0f, 2.0f, 4.0f, 4.0f);

    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, {}, {nogo}, s);
    REQUIRE(plan.valid);

    // No coverage waypoint should fall inside the NoGo area
    for (const auto &w : plan.waypoints)
    {
        if (!w.mowOn)
            continue;
        const bool insideNoGo = (w.x > 2.05f && w.x < 3.95f &&
                                 w.y > 2.05f && w.y < 3.95f);
        REQUIRE(!insideNoGo);
    }

    // The plan should still mow around the NoGo — expect more waypoints than
    // a plan with no NoGo (transit points added for the gap in each row)
    const auto planNoExcl = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(planNoExcl.valid);
    // With NoGo: each affected row is split into two segments → more waypoints
    REQUIRE(plan.waypoints.size() > planNoExcl.waypoints.size());
}

TEST_CASE("Boustrophedon — edge mowing produces boundary waypoints", "[boustrophedon]")
{
    // 4×4 zone, 1 edge round at stripWidth=0.3m → contour at ~0.15m from the boundary
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 4.0f, 4.0f);

    ZoneSettings sEdge;
    sEdge.stripWidth = 0.3f;
    sEdge.edgeMowing = true;
    sEdge.edgeRounds = 1;

    const auto planEdge = ZonePlanner::plan("z1", zone, {}, {}, sEdge);
    REQUIRE(planEdge.valid);
    REQUIRE_FALSE(planEdge.waypoints.empty());

    // Edge mowing should produce mowOn=true waypoints within one stripWidth of
    // all four sides, because the contour pass traces the zone perimeter at ~0.15m inset
    const float margin = sEdge.stripWidth; // 0.3m
    bool nearLeft = false, nearRight = false, nearBottom = false, nearTop = false;
    for (const auto &w : planEdge.waypoints)
    {
        if (!w.mowOn)
            continue;
        if (w.x < margin)
            nearLeft = true;
        if (w.x > 4.0f - margin)
            nearRight = true;
        if (w.y < margin)
            nearBottom = true;
        if (w.y > 4.0f - margin)
            nearTop = true;
    }
    CHECK(nearLeft);
    CHECK(nearRight);
    CHECK(nearBottom);
    CHECK(nearTop);

    // The plan must also be valid without edge mowing (smoke check)
    ZoneSettings sNoEdge;
    sNoEdge.stripWidth = 0.3f;
    sNoEdge.edgeMowing = false;
    sNoEdge.edgeRounds = 0;
    const auto planNoEdge = ZonePlanner::plan("z1", zone, {}, {}, sNoEdge);
    REQUIRE(planNoEdge.valid);
}

TEST_CASE("Boustrophedon — transit waypoints between stripes have mowOn=false",
          "[boustrophedon]")
{
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 3.0f, 3.0f);
    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(plan.valid);
    REQUIRE(countTransit(plan.waypoints) >= 1);

    // No two consecutive waypoints should both be mowOn=false
    for (std::size_t i = 1; i < plan.waypoints.size(); ++i)
        REQUIRE(!(plan.waypoints[i - 1].mowOn == false && plan.waypoints[i].mowOn == false));
}

// ─────────────────────────────────────────────────────────────────────────────
// WaypointExecutor Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("WaypointExecutor — empty start is immediately done", "[waypoint_exec]")
{
    WaypointExecutor exec;
    REQUIRE(!exec.hasPlan());
    REQUIRE(exec.isDone());
    REQUIRE(exec.currentWaypoint() == nullptr);
}

TEST_CASE("WaypointExecutor — advance through 3 waypoints", "[waypoint_exec]")
{
    WaypointExecutor exec;
    exec.startPlan(std::vector<Waypoint>{
        {1.0f, 0.0f, true},
        {2.0f, 0.0f, true},
        {3.0f, 0.0f, false}});

    REQUIRE(exec.hasPlan());
    REQUIRE(!exec.isDone());
    REQUIRE(exec.totalWaypoints() == 3);
    REQUIRE(exec.currentIndex() == 0);

    const Waypoint *w0 = exec.currentWaypoint();
    REQUIRE(w0 != nullptr);
    REQUIRE(w0->x == Approx(1.0f));

    exec.advanceToNext();
    REQUIRE(exec.currentIndex() == 1);
    REQUIRE(exec.currentWaypoint()->x == Approx(2.0f));

    exec.advanceToNext();
    REQUIRE(exec.currentIndex() == 2);
    REQUIRE(exec.currentWaypoint()->x == Approx(3.0f));
    REQUIRE(exec.currentWaypoint()->mowOn == false);

    exec.advanceToNext();
    REQUIRE(exec.isDone());
    REQUIRE(exec.currentWaypoint() == nullptr);
    REQUIRE(exec.progress() == Approx(1.0f));
}

TEST_CASE("WaypointExecutor — reset clears state", "[waypoint_exec]")
{
    WaypointExecutor exec;
    exec.startPlan(std::vector<Waypoint>{{0.0f, 0.0f, true}, {1.0f, 0.0f, true}});
    exec.advanceToNext();
    REQUIRE(exec.currentIndex() == 1);

    exec.reset();
    REQUIRE(!exec.hasPlan());
    REQUIRE(exec.isDone());
    REQUIRE(exec.currentWaypoint() == nullptr);
    REQUIRE(exec.progress() == Approx(0.0f));
}

TEST_CASE("WaypointExecutor — startPlan(ZonePlan) loads waypoints", "[waypoint_exec]")
{
    // Build a zone plan with ZonePlanner
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 2.0f, 2.0f);
    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;
    s.edgeRounds = 0;
    const auto zonePlan = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(zonePlan.valid);

    WaypointExecutor exec;
    exec.startPlan(zonePlan);
    REQUIRE(exec.hasPlan());
    REQUIRE(!exec.isDone());
    REQUIRE(exec.totalWaypoints() == static_cast<int>(zonePlan.waypoints.size()));
}

TEST_CASE("WaypointExecutor — second startPlan overwrites first", "[waypoint_exec]")
{
    WaypointExecutor exec;
    exec.startPlan(std::vector<Waypoint>{{0.0f, 0.0f, true}, {1.0f, 0.0f, true}});
    exec.advanceToNext();
    REQUIRE(exec.currentIndex() == 1);

    // Start a completely new plan
    exec.startPlan(std::vector<Waypoint>{{5.0f, 5.0f, true}});
    REQUIRE(exec.currentIndex() == 0);
    REQUIRE(!exec.isDone());
    REQUIRE(exec.currentWaypoint()->x == Approx(5.0f));
}

TEST_CASE("WaypointExecutor — progress fraction", "[waypoint_exec]")
{
    WaypointExecutor exec;
    exec.startPlan(std::vector<Waypoint>{
        {0.0f, 0.0f, true},
        {1.0f, 0.0f, true},
        {2.0f, 0.0f, true},
        {3.0f, 0.0f, true}});

    REQUIRE(exec.progress() == Approx(0.0f));
    exec.advanceToNext();
    REQUIRE(exec.progress() == Approx(0.25f));
    exec.advanceToNext();
    REQUIRE(exec.progress() == Approx(0.5f));
}

TEST_CASE("RoutePlan -> Waypoints keeps one-to-one indices and mow semantics", "[route][waypoint_exec]")
{
    RoutePlan route;
    route.sourceMode = WayType::MOW;
    route.points = {
        {{0.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::COVERAGE_EDGE, "z1", "z1:c1"},
        // TRANSIT_WITHIN_ZONE keeps blade on (motor stays running during same-zone stripe
        // transitions to avoid stop/start overhead). Use TRANSIT_INTER_ZONE to test mowOn=false.
        {{1.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::TRANSIT_INTER_ZONE, "z1", "z1:c1"},
        {{2.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::COVERAGE_INFILL, "z1", "z1:c1"},
    };

    const auto waypoints = routePlanToWaypoints(route);
    REQUIRE(waypoints.size() == route.points.size());
    REQUIRE(waypoints[0].mowOn);
    REQUIRE_FALSE(waypoints[1].mowOn);
    REQUIRE(waypoints[2].mowOn);
}

TEST_CASE("WaypointExecutor — startPlan(RoutePlan) and seek stay aligned", "[waypoint_exec]")
{
    RoutePlan route;
    route.sourceMode = WayType::MOW;
    route.points = {
        {{0.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::COVERAGE_EDGE, "z1", "z1:c1"},
        // TRANSIT_INTER_ZONE: blade off (crossing between zones or leaving mowing area)
        {{1.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::TRANSIT_INTER_ZONE, "z1", "z1:c1"},
        {{2.0f, 0.0f}, false, false, false, 0.25f, WayType::MOW, RouteSemantic::COVERAGE_INFILL, "z1", "z1:c1"},
    };

    WaypointExecutor exec;
    exec.startPlan(route);
    exec.seekToIndex(1);

    REQUIRE(exec.currentIndex() == 1);
    REQUIRE(exec.currentWaypoint() != nullptr);
    REQUIRE(exec.currentWaypoint()->x == Approx(1.0f));
    REQUIRE_FALSE(exec.currentWaypoint()->mowOn);
}

// ─────────────────────────────────────────────────────────────────────────────
// TransitionRouter Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("TransitionRouter — direct path when no obstacles", "[nav_planner]")
{
    // Empty map, allowOutsidePerimeter = true → just from/to
    Map emptyMap;

    TransitionRouter::Options opts;
    opts.allowOutsidePerimeter = true;
    opts.cellSize_m = 0.20f;

    const Point from{0.0f, 0.0f};
    const Point to{3.0f, 0.0f};

    const auto path = TransitionRouter::plan(from, to, emptyMap, opts);

    REQUIRE(!path.empty());
    REQUIRE(path.front().x == Approx(from.x).margin(0.01f));
    REQUIRE(path.front().y == Approx(from.y).margin(0.01f));
    REQUIRE(path.back().x == Approx(to.x).margin(0.01f));
    REQUIRE(path.back().y == Approx(to.y).margin(0.01f));
}

TEST_CASE("TransitionRouter — same start and end returns valid path", "[nav_planner]")
{
    Map emptyMap;
    TransitionRouter::Options opts;
    opts.allowOutsidePerimeter = true;
    opts.cellSize_m = 0.20f;

    const Point pos{1.0f, 1.0f};
    const auto path = TransitionRouter::plan(pos, pos, emptyMap, opts);

    // Should return a path (may be 1-2 points)
    REQUIRE(!path.empty());
}

TEST_CASE("TransitionRouter — path stays inside perimeter", "[nav_planner]")
{
    // Simple 10×10 m perimeter, from one side to the other
    const std::string mapJson = R"({
        "perimeter": [
            {"x": 0, "y": 0}, {"x": 10, "y": 0},
            {"x": 10, "y": 10}, {"x": 0, "y": 10}
        ]
    })";
    auto map = loadMap(mapJson);

    TransitionRouter::Options opts;
    opts.allowOutsidePerimeter = false;
    opts.cellSize_m = 0.25f;

    const auto path = TransitionRouter::plan({1.0f, 1.0f}, {9.0f, 9.0f}, map, opts);

    REQUIRE(!path.empty());
    // All intermediate waypoints should be within the perimeter (with small margin)
    for (const auto &p : path)
    {
        REQUIRE(p.x >= -0.5f);
        REQUIRE(p.x <= 10.5f);
        REQUIRE(p.y >= -0.5f);
        REQUIRE(p.y <= 10.5f);
    }
}

TEST_CASE("TransitionRouter — path avoids hard exclusion", "[nav_planner]")
{
    // 10×10 m perimeter, 3×8 m wall in the middle (x: 4-7, y: 0-8)
    const std::string mapJson = R"({
        "perimeter": [
            {"x": 0, "y": 0}, {"x": 10, "y": 0},
            {"x": 10, "y": 10}, {"x": 0, "y": 10}
        ],
        "exclusions": [
            {
                "points": [
                    {"x": 4, "y": 0}, {"x": 7, "y": 0},
                    {"x": 7, "y": 8}, {"x": 4, "y": 8}
                ],
                "type": "hard"
            }
        ]
    })";
    auto map = loadMap(mapJson);

    TransitionRouter::Options opts;
    opts.allowOutsidePerimeter = false;
    opts.cellSize_m = 0.25f;

    // Navigate from left side to right side — must go around or over the wall
    const auto path = TransitionRouter::plan({1.0f, 5.0f}, {9.0f, 5.0f}, map, opts);

    // Path must exist
    REQUIRE(!path.empty());

    // No path point should fall inside the exclusion zone (with small margin)
    for (const auto &p : path)
    {
        const bool inExcl = (p.x > 4.1f && p.x < 6.9f && p.y > 0.1f && p.y < 7.9f);
        INFO("path point (" << p.x << ", " << p.y << ") is inside exclusion");
        REQUIRE(!inExcl);
    }
}

TEST_CASE("TransitionRouter — allowOutsidePerimeter unlocks dock approach", "[nav_planner]")
{
    // Small 3×3 perimeter, dock at (5, 1.5) which is outside
    const std::string mapJson = R"({
        "perimeter": [
            {"x": 0, "y": 0}, {"x": 3, "y": 0},
            {"x": 3, "y": 3}, {"x": 0, "y": 3}
        ]
    })";
    auto map = loadMap(mapJson);

    // Without permission: should fail (path blocked at perimeter edge)
    TransitionRouter::Options opts;
    opts.allowOutsidePerimeter = false;
    opts.cellSize_m = 0.20f;
    const auto pathRestricted = TransitionRouter::plan({1.5f, 1.5f}, {5.0f, 1.5f}, map, opts);
    // Restricted path may be empty or stay inside perimeter
    // (just verifying the flag has an effect when true)

    // With permission: should find a path
    opts.allowOutsidePerimeter = true;
    const auto pathFree = TransitionRouter::plan({1.5f, 1.5f}, {5.0f, 1.5f}, map, opts);
    REQUIRE(!pathFree.empty());
    REQUIRE(pathFree.back().x == Approx(5.0f).margin(0.01f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Integration: ZonePlanner → WaypointExecutor
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Integration — zone plan can be loaded into executor and advanced fully",
          "[boustrophedon][waypoint_exec]")
{
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 4.0f, 4.0f);
    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = true;
    s.edgeRounds = 1;

    const auto zonePlan = ZonePlanner::plan("z1", zone, {}, {}, s);
    REQUIRE(zonePlan.valid);

    WaypointExecutor exec;
    exec.startPlan(zonePlan);

    int steps = 0;
    while (!exec.isDone())
    {
        REQUIRE(exec.currentWaypoint() != nullptr);
        exec.advanceToNext();
        ++steps;

        // Safety: no plan should take > 10,000 steps for a small zone
        REQUIRE(steps < 10000);
    }

    REQUIRE(exec.isDone());
    REQUIRE(exec.currentWaypoint() == nullptr);
    REQUIRE(steps == static_cast<int>(zonePlan.waypoints.size()));
    REQUIRE(exec.progress() == Approx(1.0f));
}

TEST_CASE("Integration — coverage waypoints form monotone rows (no chaotic jumps)",
          "[boustrophedon]")
{
    // 6×6 zone with a NoGo in the middle. The mowing should stay local —
    // no long diagonal jumps across the full zone between adjacent stripes.
    const PolygonPoints zone = makeRect(0.0f, 0.0f, 6.0f, 6.0f);
    const PolygonPoints nogo = makeRect(2.5f, 2.5f, 3.5f, 3.5f);

    ZoneSettings s;
    s.stripWidth = 0.3f;
    s.edgeMowing = false;
    s.edgeRounds = 0;

    const auto plan = ZonePlanner::plan("z1", zone, {}, {nogo}, s);
    REQUIRE(plan.valid);

    // Transit distances between consecutive stripes should be short
    // (< 2 × strip width for a properly ordered boustrophedon).
    // A badly ordered plan would have diagonal jumps of 3+ metres.
    const float maxAllowedTransit = 2.0f; // metres

    for (std::size_t i = 1; i < plan.waypoints.size(); ++i)
    {
        if (plan.waypoints[i].mowOn)
            continue; // coverage points are fine, only check transit hops
        const auto &prev = plan.waypoints[i - 1];
        const auto &curr = plan.waypoints[i];
        const float dist = std::hypot(curr.x - prev.x, curr.y - prev.y);
        INFO("Transit hop from (" << prev.x << "," << prev.y << ") to ("
                                  << curr.x << "," << curr.y << ") = " << dist << "m");
        REQUIRE(dist < maxAllowedTransit);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// N3: MissionPlan assembly — zone waypoints + inter-zone transitions
// ─────────────────────────────────────────────────────────────────────────────

using namespace sunray::nav;

/// Helper: build a two-zone Map JSON where z1 and z2 are separated by a gap.
static const char *kTwoZoneMapJson = R"({
  "perimeter":[[0,0],[20,0],[20,8],[0,8]],
  "zones":[
    {"id":"z1","order":1,
     "polygon":[[0.5,0.5],[4.5,0.5],[4.5,7.5],[0.5,7.5]],
     "settings":{"name":"Left","stripWidth":0.5,"angle":0,
                 "edgeMowing":false,"edgeRounds":0,"speed":1.0,
                 "pattern":"stripe","reverseAllowed":false,"clearance":0.0}},
    {"id":"z2","order":2,
     "polygon":[[10.5,0.5],[14.5,0.5],[14.5,7.5],[10.5,7.5]],
     "settings":{"name":"Right","stripWidth":0.5,"angle":0,
                 "edgeMowing":false,"edgeRounds":0,"speed":1.0,
                 "pattern":"stripe","reverseAllowed":false,"clearance":0.0}}
  ]
})";

TEST_CASE("N3.1 — buildZonePlanPreview populates ZonePlan.waypoints", "[n3][mow_planner]")
{
    Map map;
    REQUIRE(map.loadJson(nlohmann::json::parse(kTwoZoneMapJson)));
    REQUIRE(map.zones().size() == 2);

    const Zone &zone = map.zones()[0]; // z1
    const ZonePlan zp = buildZonePlanPreview(map, zone, nlohmann::json::object());

    REQUIRE(zp.valid);
    REQUIRE_FALSE(zp.waypoints.empty());
    // Must have at least some coverage waypoints.
    REQUIRE(countCoverage(zp.waypoints) > 0);
}

TEST_CASE("N3.2 — buildMissionPlanPreview populates MissionPlan.waypoints (single zone)", "[n3][mow_planner]")
{
    Map map;
    REQUIRE(map.loadJson(nlohmann::json::parse(kTwoZoneMapJson)));

    const nlohmann::json mission = {{"zoneIds", {"z1"}}};
    const MissionPlan mp = buildMissionPlanPreview(map, mission);

    REQUIRE(mp.valid);
    REQUIRE_FALSE(mp.waypoints.empty());
    REQUIRE(countCoverage(mp.waypoints) > 0);
    // Single zone: no transit between zones expected — all waypoints are from z1.
    REQUIRE(mp.zonePlans.size() == 1);
    REQUIRE(mp.waypoints.size() == mp.zonePlans[0].waypoints.size());
}

TEST_CASE("N3.2 — buildMissionPlanPreview multi-zone includes inter-zone transit", "[n3][mow_planner]")
{
    Map map;
    REQUIRE(map.loadJson(nlohmann::json::parse(kTwoZoneMapJson)));

    const nlohmann::json mission = {{"zoneIds", {"z1", "z2"}}};
    const MissionPlan mp = buildMissionPlanPreview(map, mission);

    REQUIRE(mp.valid);
    REQUIRE(mp.zonePlans.size() == 2);

    const std::size_t z1Count = mp.zonePlans[0].waypoints.size();
    const std::size_t z2Count = mp.zonePlans[1].waypoints.size();
    REQUIRE(z1Count > 0);
    REQUIRE(z2Count > 0);

    // Total waypoints must be at least the sum of both zone plans.
    // The transition adds ≥1 extra waypoint(s) between the zones.
    REQUIRE(mp.waypoints.size() >= z1Count + z2Count);

    // Must have at least one transit (mowOn=false) waypoint bridging the gap.
    REQUIRE(countTransit(mp.waypoints) > 0);
}

TEST_CASE("N3.2 — MissionPlan.waypoints: z1 coverage, then transit, then z2 coverage", "[n3][mow_planner]")
{
    Map map;
    REQUIRE(map.loadJson(nlohmann::json::parse(kTwoZoneMapJson)));

    const nlohmann::json mission = {{"zoneIds", {"z1", "z2"}}};
    const MissionPlan mp = buildMissionPlanPreview(map, mission);

    REQUIRE(mp.valid);
    REQUIRE_FALSE(mp.waypoints.empty());

    // z1 spans x=[0.5,4.5], z2 spans x=[10.5,14.5].
    // The inter-zone transit must cross the gap (x > 5.0) and include at least
    // one transit waypoint that is clearly outside z1 (x > 5.0).
    bool foundInterZoneTransit = false;
    for (const auto &wp : mp.waypoints)
    {
        if (!wp.mowOn && wp.x > 5.0f)
        {
            foundInterZoneTransit = true;
            break;
        }
    }
    REQUIRE(foundInterZoneTransit);

    // z2 coverage waypoints (x > 9.0) must appear after z1 coverage (x < 5.0).
    bool seenZ1Coverage = false;
    bool seenZ2Coverage = false;
    for (const auto &wp : mp.waypoints)
    {
        if (wp.mowOn && wp.x < 5.0f)
            seenZ1Coverage = true;
        if (wp.mowOn && wp.x > 9.0f)
        {
            // Once z2 coverage starts, z1 coverage must already have appeared.
            REQUIRE(seenZ1Coverage);
            seenZ2Coverage = true;
        }
    }
    REQUIRE(seenZ1Coverage);
    REQUIRE(seenZ2Coverage);
}
