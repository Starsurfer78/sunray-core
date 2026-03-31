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

#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace sunray {
namespace nav {

// ── Geometry primitives ───────────────────────────────────────────────────────

struct Point {
    float x = 0.0f;  ///< metres east
    float y = 0.0f;  ///< metres north

    Point() = default;
    Point(float ax, float ay) : x(ax), y(ay) {}

    float distanceTo(const Point& o) const {
        float dx = x - o.x, dy = y - o.y;
        return std::sqrt(dx*dx + dy*dy);
    }
};

using PolygonPoints = std::vector<Point>;

// ── WayType ───────────────────────────────────────────────────────────────────

enum class WayType { PERIMETER, EXCLUSION, DOCK, MOW, FREE };

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
};

struct Zone {
    std::string  id;
    int          order = 1;  ///< legacy editor ordering until mission sequencing replaces it
    PolygonPoints polygon;
    ZoneSettings  settings;
};

// ── Map ───────────────────────────────────────────────────────────────────────

class Map {
public:
    explicit Map(std::shared_ptr<Config> config = nullptr);

    // ── Persistence ───────────────────────────────────────────────────────────

    /// Load map data from a JSON file (written by Mission Service).
    /// Returns true on success. On failure, map is cleared (empty).
    bool load(const std::filesystem::path& path);

    /// Save current map to JSON file.
    bool save(const std::filesystem::path& path) const;

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

    /// Begin dock approach from current robot position.
    /// Finds nearest free-path entry point toward dock.
    bool startDocking(float robotX, float robotY);

    /// Retry docking (drive back to first docking point).
    bool retryDocking(float robotX, float robotY);

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
    WayType previousWayMode = WayType::MOW;  ///< stores mode before switching to FREE
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
    /// then restore previousWayMode.
    /// waypoints must NOT include the current robot position (start point).
    void injectFreePath(const std::vector<Point>& waypoints);

    // ── Boundary queries ──────────────────────────────────────────────────────

    /// True if (x,y) is inside the perimeter polygon and outside all exclusions.
    bool isInsideAllowedArea(float x, float y) const;

    // ── Raw data access (for telemetry / debug) ───────────────────────────────

    const PolygonPoints&                   perimeterPoints() const { return perimeterPoints_; }
    const std::vector<MowPoint>&          mowPoints()       const { return mowPoints_; }
    const PolygonPoints&                   dockPoints()      const { return dockPoints_; }
    const std::vector<PolygonPoints>&      exclusions()      const { return exclusions_; }
    const std::vector<Zone>&               zones()           const { return zones_; }
    const std::vector<OnTheFlyObstacle>&   obstacles()       const { return obstacles_; }  ///< C.7
    const nlohmann::json&                  captureMeta()     const { return captureMeta_; }
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
    std::vector<MowPoint>      mowPoints_;
    std::vector<MowPoint>      allMowPoints_;  ///< immutable source set loaded from map.json
    PolygonPoints              dockPoints_;
    PolygonPoints              freePoints_;
    std::vector<PolygonPoints> exclusions_;
    std::vector<OnTheFlyObstacle> obstacles_;  ///< virtual obstacles from addObstacle() (C.7)
    std::vector<Zone>          zones_;       ///< mow zones (C.4b — ordered by zone.order)
    nlohmann::json             captureMeta_ = nlohmann::json::object();  ///< optional map capture metadata (C.14-g)

    bool isDocked_   = false;
    bool shouldDock_ = false;
    bool shouldMow_  = false;

    std::shared_ptr<Config> config_;

    long mapCRC_ = 0;
};

} // namespace nav
} // namespace sunray
