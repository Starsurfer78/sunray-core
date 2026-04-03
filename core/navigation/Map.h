#pragma once
// Map.h — Waypoint management, perimeter polygon, A* path finding.
//
// Ported from sunray/map.h/.cpp — Arduino/SD replaced by:
//   - std::vector instead of fixed-size arrays (no heap fragmentation risk on Pi)
//   - nlohmann/json for map file serialization (replaces SD binary)
//   - std::filesystem for file I/O
//   - float coordinates throughout (no Arduino short/float duality)
//
// Coordinate system: local metres (east = +x, north = +y), origin set by AT+P.
//
// Key workflow:
//   load(path)               — load map from JSON file (set by Mission Service)
//   startMowing(x, y)        — begin mowing mission from current robot position
//   startDocking(x, y)       — begin dock approach
//   nextPoint(x, y)          — advance to next waypoint; returns false when done
//   isInsideAllowedArea(x,y) — perimeter in, exclusions out
//   addObstacle(x,y,t)       — mark a virtual obstacle (C.7: OnTheFlyObstacle)

#include "core/Config.h"
#include "core/navigation/PlannerContext.h"
#include "core/navigation/Route.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace sunray {
namespace nav {

// ── Per-waypoint mow metadata (C.4c — K-turn support) ────────────────────────

/// A single mowing waypoint with optional motion flags.
/// JSON format (new):  { "p": [x,y], "rev": true, "slow": true }
/// JSON format (legacy, read-only compat): [x, y]
struct MowPoint {
    Point p;
    bool  rev  = false;  ///< drive in reverse to reach this point
    bool  slow = false;  ///< reduce speed (turn transition)
};

// ── Mow-zone data (from WebUI MapEditor C.4b) ─────────────────────────────────

enum class ZonePattern { STRIPE, SPIRAL };
enum class ExclusionType { HARD, SOFT };
enum class DockApproachMode { FORWARD_ONLY, REVERSE_ALLOWED };

// ── On-The-Fly Obstacle (C.7) ─────────────────────────────────────────────────

/// A virtual obstacle detected at runtime (e.g. bumper hit).
/// Stored in Map and respected by findPath() for future routing.
struct OnTheFlyObstacle {
    Point    center;
    float    radius_m      = 0.3f;   ///< avoidance radius in metres
    unsigned detectedAt_ms = 0;      ///< now_ms when first detected
    int      hitCount      = 1;      ///< number of bumper hits at this location
    bool     persistent    = false;  ///< if false → auto-cleaned after expiry
};

struct ZoneSettings {
    std::string name        = "Zone";
    float       stripWidth  = 0.18f;   ///< mowing strip width in metres
    float       angle       = 0.0f;    ///< local preview stripe angle in degrees
    bool        edgeMowing  = true;    ///< local preview headland enabled
    int         edgeRounds  = 1;       ///< local preview headland rounds
    float       speed       = 1.0f;    ///< target speed in m/s
    ZonePattern pattern     = ZonePattern::STRIPE;
    bool        reverseAllowed = false; ///< planner may use reverse segments in this zone
    float       clearance   = 0.25f;   ///< planner clearance in metres
};

struct Zone {
    std::string  id;
    int          order = 1;  ///< legacy editor ordering until mission sequencing replaces it
    PolygonPoints polygon;
    ZoneSettings  settings;
};

struct PlannerSettings {
    float         defaultClearance_m    = 0.25f;
    float         perimeterSoftMargin_m = 0.15f;
    float         perimeterHardMargin_m = 0.05f;
    float         obstacleInflation_m   = 0.35f;
    float         softNoGoCostScale     = 0.6f;
    unsigned long replanPeriod_ms       = 250;
    float         gridCellSize_m        = 0.10f;
};

struct ExclusionMeta {
    ExclusionType type        = ExclusionType::HARD;
    float         clearance_m = 0.25f;
    float         costScale   = 1.0f;
};

struct DockMeta {
    DockApproachMode approachMode        = DockApproachMode::FORWARD_ONLY;
    PolygonPoints    corridor;
    float            finalAlignHeading_deg = 0.0f;
    bool             hasFinalAlignHeading  = false;
    float            slowZoneRadius_m      = 0.6f;
};

// ── Map ───────────────────────────────────────────────────────────────────────

class Map {
public:
    friend class Planner;

    explicit Map(std::shared_ptr<Config> config = nullptr);

    // ── Persistence ───────────────────────────────────────────────────────────

    /// Load map data from a JSON file (written by Mission Service).
    /// Returns true on success. On failure, map is cleared (empty).
    bool load(const std::filesystem::path& path);

    /// Load map data directly from a JSON object.
    /// Mirrors load(path) but avoids touching the filesystem.
    bool loadJson(const nlohmann::json& j);

    /// Save current map to JSON file.
    bool save(const std::filesystem::path& path) const;
    nlohmann::json exportMapJson() const;

    /// Reset all waypoint lists and polygon data.
    void clear();

    /// True if map has at least a perimeter polygon loaded.
    bool isLoaded() const { return !perimeterPoints_.empty(); }

    long calcCRC() const;

    // ── Mission control ────────────────────────────────────────────────────────

    /// Begin mowing from current robot position.
    /// Finds nearest mowing point and sets it as first target.
    bool startMowing(float robotX, float robotY);

    /// Begin mowing only the points belonging to the given zone IDs (C.12).
    /// Points are matched via their zone polygon membership.
    /// Falls back to startMowing() if zoneIds is empty or no points match.
    bool startMowingZones(float robotX, float robotY, const std::vector<std::string>& zoneIds);

    /// Begin mowing from an already planned route.
    /// Used for mission-specific coverage plans so runtime follows exactly the
    /// same route that preview/save generated.
    bool startPlannedMowing(float robotX, float robotY, const RoutePlan& route);

    /// Begin dock approach from current robot position.
    /// Finds nearest free-path entry point toward dock.
    bool startDocking(float robotX, float robotY);

    /// Retry docking (drive back to first docking point).
    bool retryDocking(float robotX, float robotY, float lateralOffsetM = 0.0f);

    /// Mark the map as manually docked (robot placed on dock without driving).
    void setIsDocked(bool flag) { isDocked_ = flag; }

    /// True if robot is currently following docking waypoints.
    bool isDocking() const;

    /// True if robot is currently following undocking waypoints.
    bool isUndocking() const;

    // ── Waypoint navigation ───────────────────────────────────────────────────

    /// Advance to the next waypoint. Returns false if no further points.
    bool nextPoint(bool sim, float robotX, float robotY);

    /// Is the next upcoming segment a straight line (no sharp turn)?
    bool nextPointIsStraight() const;

    /// Distance from (rx,ry) to current target point.
    float distanceToTargetPoint(float rx, float ry) const;

    /// Distance from (rx,ry) to the previous (last) target point.
    float distanceToLastTargetPoint(float rx, float ry) const;

    /// Get position and heading of the n-th docking point (default: last).
    bool getDockingPos(float& x, float& y, float& delta, int idx = -1) const;

    /// True if mowing sequence is complete (all mow points visited).
    bool mowingCompleted() const;

    /// Jump to a percentage position in the mowing point list (0..100).
    void setMowingPointPercent(float pct);

    /// Skip the next mowing point.
    void skipNextMowingPoint();

    // ── Current target ────────────────────────────────────────────────────────

    Point targetPoint;      ///< current target waypoint
    Point lastTargetPoint;  ///< previous target (defines the line being tracked)
    bool  trackReverse = false; ///< drive in reverse to reach target
    bool  trackSlow    = false; ///< reduce speed (docking approach)
    WayType wayMode    = WayType::MOW;
    int  percentCompleted = 0;

    // ── Obstacle management ───────────────────────────────────────────────────

    /// Add a virtual obstacle at (x,y) detected at now_ms.
    /// radius_m taken from config "obstacle_diameter_m" / 2 (default 0.3 m radius).
    /// persistent = true → survives clearAutoObstacles() (user-placed).
    bool addObstacle(float x, float y, unsigned now_ms, bool persistent = false);

    /// Remove all virtual obstacles (auto + persistent).
    void clearObstacles();

    /// Remove only auto-detected (non-persistent) obstacles — called on ChargeOp entry.
    void clearAutoObstacles();

    /// Remove auto-detected obstacles older than expiry_ms — call once per second.
    void cleanupExpiredObstacles(unsigned now_ms, unsigned expiry_ms = 3600000u);

    // ── Path injection (E.2 — GridMap A*) ────────────────────────────────────

    /// Inject a pre-computed path as FREE waypoints.
    /// Sets wayMode = FREE, freePointsIdx = 0, targetPoint = first waypoint.
    /// The next call to nextFreePoint() will advance through the injected path,
    /// then restore the explicit free-route resume context.
    /// waypoints must NOT include the current robot position (start point).
    void injectFreePath(const std::vector<Point>& waypoints);

    /// Replan from the robot's current position to the current mission target and
    /// switch into FREE path following on success.
    bool replanToCurrentTarget(float robotX, float robotY);

    /// Periodic replanning gate using plannerSettings().replanPeriod_ms.
    bool maybeReplanToCurrentTarget(unsigned long now_ms, float robotX, float robotY);

    /// Compute a planner preview path without mutating map state.
    RoutePlan previewPath(const Point& src,
                          const Point& dst,
                          WayType missionMode = WayType::FREE,
                          WayType planningMode = WayType::FREE,
                          float headingReferenceRad = 0.0f,
                          bool hasHeadingReference = false,
                          bool reverseAllowed = false,
                          float clearance_m = 0.25f,
                          float robotRadius_m = 0.0f) const;

    // ── Boundary queries ──────────────────────────────────────────────────────

    /// True if (x,y) is inside the perimeter polygon and outside all exclusions.
    bool isInsideAllowedArea(float x, float y) const;

    // ── Raw data access (for telemetry / debug) ───────────────────────────────

    const PolygonPoints&                   perimeterPoints() const { return perimeterPoints_; }
    const RoutePlan&                       mowRoutePlan()    const { return mowRoute_; }
    const RoutePlan&                       dockRoutePlan()   const { return dockRoute_; }
    const RoutePlan&                       freeRoutePlan()   const { return localRoute_; }
    const std::vector<PolygonPoints>&      exclusions()      const { return exclusions_; }
    const std::vector<Zone>&               zones()           const { return zones_; }
    const std::vector<OnTheFlyObstacle>&   obstacles()       const { return obstacles_; }  ///< C.7
    const PlannerSettings&                 plannerSettings() const { return planner_; }
    const std::vector<ExclusionMeta>&      exclusionMeta()   const { return exclusionMeta_; }
    const DockMeta&                        dockMeta()        const { return dockMeta_; }
    const nlohmann::json&                  captureMeta()     const { return captureMeta_; }
    RouteSegment currentTrackingSegment(float robotX, float robotY, float robotHeadingRad) const;
    bool refreshTracking(unsigned long now_ms, float robotX, float robotY);
    bool advanceTracking(float robotX, float robotY);
    bool dockingFinalAlignActive(float robotX, float robotY) const;
    float dockingFinalAlignHeadingRad() const;
    bool dockingReverseAllowed() const;
    bool dockingShouldReverseOnFinalApproach(float robotX, float robotY, float robotHeadingRad) const;
    std::string zoneIdForPoint(float x, float y,
                               const std::vector<std::string>& preferredOrder = {}) const;

    int mowPointsIdx  = 0;
    int dockPointsIdx = 0;
    int freePointsIdx = 0;

private:
    // ── Geometry helpers ──────────────────────────────────────────────────────

    static float scalePI(float v);
    static float distancePI(float x, float w);
    static float scalePIangles(float setAngle, float currAngle);
    static float pointsAngle(float x1, float y1, float x2, float y2);
    static bool  pointInPolygon(const PolygonPoints& poly, float px, float py);
    static float polygonArea(const PolygonPoints& poly);

    bool lineIntersects(const Point& p0, const Point& p1,
                        const Point& p2, const Point& p3) const;
    bool linePolygonIntersection(const Point& src, const Point& dst,
                                 const PolygonPoints& poly) const;
    bool segmentAllowed(const Point& src, const Point& dst) const;
    bool pathAllowed(const std::vector<Point>& path) const;
    bool currentSegmentNeedsReplan(float robotX, float robotY) const;
    const ZoneSettings* zoneSettingsForPoint(float x, float y) const;
    float routeClearanceForPoint(const Point& point, WayType mode) const;
    bool routeReverseAllowedForPoint(const Point& point, WayType mode) const;
    void applyConfigDefaults();
    static RoutePlan buildMowRoutePlan(const std::vector<MowPoint>& points);
    static RoutePlan buildDockRoutePlan(const PolygonPoints& points);
    void activateMowRoute(const RoutePlan& route);
    void beginFreeRoute(WayType resumeMode,
                        const Point& resumeTarget,
                        bool resumeTrackReverse,
                        bool resumeTrackSlow,
                        int resumeMowIdx,
                        int resumeDockIdx,
                        const RoutePlan& route);
    Point dockApproachTarget() const;
    float kidnapToleranceForMode(WayType mode) const;

    // ── A* pathfinding (simplified for Phase 1) ───────────────────────────────

    /// Insert free-path points between current position and target using A*.
    bool findPath(const Point& src, const Point& dst);

    // ── Waypoint state helpers ────────────────────────────────────────────────

    bool nextMowPoint(bool sim);
    bool nextDockPoint(bool sim);
    bool nextFreePoint(bool sim);
    void setLastTargetPoint(float x, float y);

    // ── Data ──────────────────────────────────────────────────────────────────

    PolygonPoints              perimeterPoints_;
    PolygonPoints              freePoints_;
    RoutePlan                  allMowRoute_;
    RoutePlan                  mowRoute_;
    RoutePlan                  dockRoute_;
    RoutePlan                  localRoute_;
    std::vector<PolygonPoints> exclusions_;
    std::vector<ExclusionMeta> exclusionMeta_;
    std::vector<OnTheFlyObstacle> obstacles_;  ///< virtual obstacles from addObstacle() (C.7)
    std::vector<Zone>          zones_;       ///< mow zones (C.4b — ordered by zone.order)
    PlannerSettings            planner_;
    DockMeta                   dockMeta_;
    nlohmann::json             captureMeta_ = nlohmann::json::object();  ///< optional map capture metadata (C.14-g)

    struct FreeRouteContext {
        bool    active             = false;
        WayType resumeMode         = WayType::MOW;
        Point   resumeTarget;
        bool    resumeTrackReverse = false;
        bool    resumeTrackSlow    = false;
        int     resumeMowIdx       = 0;
        int     resumeDockIdx      = 0;
    } freeRoute_;
    unsigned long lastReplanAttempt_ms_ = 0;

    bool isDocked_   = false;
    bool shouldDock_ = false;
    bool shouldMow_  = false;

    std::shared_ptr<Config> config_;

    long mapCRC_ = 0;
};

} // namespace nav
} // namespace sunray
