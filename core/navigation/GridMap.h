#pragma once
// GridMap.h — Local occupancy grid for A* obstacle-avoidance path planning.
//
// Workflow:
//   GridMap gm;
//   gm.build(map, robotX, robotY);     // rasterise map+obstacles into 40×40 grid
//   auto path = gm.planPath(src, dst); // A* in world coords, returns waypoints
//   path = gm.smoothPath(path);        // string-pull: remove visible intermediates
//
// Grid covers DEFAULT_COLS × DEFAULT_ROWS cells of DEFAULT_CELL_M metres each,
// centred at (originX, originY) passed to build().
// Cells outside the perimeter, inside exclusion zones, and within inflated
// obstacle radii are marked OCCUPIED.
//
// A*: 8-directional movement. Heuristic: Euclidean distance to goal.
// Smooth: greedy forward-visibility string-pull using the same occupancy grid.
//
// Thread safety: none — build() and planPath() must run on the same thread.

#include "core/navigation/Costmap.h"

#include <cstdint>
#include <vector>

namespace sunray {
namespace nav {

class GridMap {
public:
    static constexpr float DEFAULT_CELL_M      = Costmap::DEFAULT_CELL_M;
    static constexpr int   DEFAULT_COLS        = Costmap::DEFAULT_COLS;
    static constexpr int   DEFAULT_ROWS        = Costmap::DEFAULT_ROWS;
    static constexpr float DEFAULT_ROBOT_RAD_M = Costmap::DEFAULT_ROBOT_RAD_M;

    GridMap() = default;

    // ── Build ─────────────────────────────────────────────────────────────────

    /// Rasterise the given map into an occupancy grid centred at (originX, originY).
    /// Cells outside the perimeter, inside exclusion zones, and inside inflated
    /// obstacle circles are marked OCCUPIED.  All others are EMPTY.
    void build(const Map& map,
               float originX, float originY,
               float cellSize_m   = DEFAULT_CELL_M,
               int   cols         = DEFAULT_COLS,
               int   rows         = DEFAULT_ROWS,
               float robotRadius  = DEFAULT_ROBOT_RAD_M,
               WayType planningMode = WayType::FREE,
               const PolygonPoints& constraintZone = {});
    void build(const Costmap& costmap);

    bool isBuilt() const { return !cells_.empty(); }

    // ── Path planning ─────────────────────────────────────────────────────────

    /// A* from src to dst (world-coordinate metres).
    /// Returns waypoints NOT including src, in world coords.
    /// Returns empty vector if no path is reachable.
    std::vector<Point> planPath(const Point& src, const Point& dst) const;

    // ── Path smoothing (E.2-d) ────────────────────────────────────────────────

    /// String-pull smoothing: for each point in path, find the furthest
    /// directly-visible successor and skip all intermediate points.
    /// Uses this grid's occupancy data for line-of-sight checks.
    std::vector<Point> smoothPath(const std::vector<Point>& path) const;

private:
    float originX_  = 0.0f;
    float originY_  = 0.0f;
    float cellSize_ = DEFAULT_CELL_M;
    int   cols_     = 0;
    int   rows_     = 0;
    std::vector<Costmap::Cell> cells_;  ///< row-major: cells_[gy * cols_ + gx]
    std::vector<float> costs_; ///< additional traversal cost per cell

    // ── Grid ↔ World conversion ───────────────────────────────────────────────

    /// Convert world coords to grid cell indices. Returns false if out-of-bounds.
    bool  worldToGrid(float wx, float wy, int& gx, int& gy) const;

    /// Centre of grid cell (gx, gy) in world coords.
    Point gridToWorld(int gx, int gy) const;

    /// Cell value, or OCCUPIED when out-of-bounds.
    Costmap::Cell  cellAt(int gx, int gy) const;
    float traversalCostAt(int gx, int gy) const;

    /// True if the straight line from a to b passes through no OCCUPIED cell.
    /// Sampled at cellSize/2 intervals.
    bool  lineIsClear(const Point& a, const Point& b) const;

    // ── Grid marking ─────────────────────────────────────────────────────────

};

} // namespace nav
} // namespace sunray
