// GridMap.cpp — Local occupancy grid + A* path planning (E.2).
//
// Grid layout: row-major cells_[gy * cols_ + gx].
// World ↔ grid: cell (gx, gy) covers
//   [ originX + (gx - cols_/2)*cellSize,  originX + (gx - cols_/2 + 1)*cellSize ]
//   [ originY + (gy - rows_/2)*cellSize,  originY + (gy - rows_/2 + 1)*cellSize ]
//
// A*: 8-directional movement, Euclidean heuristic, standard open/closed sets.
//     With a 40×40 grid this processes ≤ 1600 nodes — negligible runtime.

#include "core/navigation/GridMap.h"
#include "core/navigation/Map.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace sunray {
namespace nav {

// ── Grid helpers ──────────────────────────────────────────────────────────────

bool GridMap::worldToGrid(float wx, float wy, int& gx, int& gy) const {
    gx = static_cast<int>(std::floor((wx - originX_) / cellSize_)) + cols_ / 2;
    gy = static_cast<int>(std::floor((wy - originY_) / cellSize_)) + rows_ / 2;
    return gx >= 0 && gx < cols_ && gy >= 0 && gy < rows_;
}

Point GridMap::gridToWorld(int gx, int gy) const {
    return {
        originX_ + (static_cast<float>(gx) - cols_ * 0.5f + 0.5f) * cellSize_,
        originY_ + (static_cast<float>(gy) - rows_ * 0.5f + 0.5f) * cellSize_
    };
}

Costmap::Cell GridMap::cellAt(int gx, int gy) const {
    if (gx < 0 || gx >= cols_ || gy < 0 || gy >= rows_) return Costmap::Cell::OCCUPIED;
    return cells_[gy * cols_ + gx];
}

float GridMap::traversalCostAt(int gx, int gy) const {
    if (gx < 0 || gx >= cols_ || gy < 0 || gy >= rows_) return 1e6f;
    return costs_.empty() ? 0.0f : costs_[gy * cols_ + gx];
}

bool GridMap::lineIsClear(const Point& a, const Point& b) const {
    const float dx   = b.x - a.x;
    const float dy   = b.y - a.y;
    const float len  = std::sqrt(dx*dx + dy*dy);
    if (len < 1e-4f) return true;
    const float step = cellSize_ * 0.5f;
    const int   n    = static_cast<int>(std::ceil(len / step));
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const float px = a.x + t * dx;
        const float py = a.y + t * dy;
        int gx, gy;
        if (!worldToGrid(px, py, gx, gy)) return false;
        if (cellAt(gx, gy) == Costmap::Cell::OCCUPIED)   return false;
    }
    return true;
}

// ── Build ──────────────────────────────────────────────────────────────────────

void GridMap::build(const Map& map,
                    float originX, float originY,
                    float cellSize_m, int cols, int rows,
                    float robotRadius,
                    WayType planningMode,
                    const PolygonPoints& constraintZone) {
    Costmap costmap;
    costmap.buildFromMap(map, originX, originY, cellSize_m, cols, rows, robotRadius, planningMode, constraintZone);
    build(costmap);
}

void GridMap::build(const Costmap& costmap) {
    if (!costmap.isBuilt()) {
        originX_ = 0.0f;
        originY_ = 0.0f;
        cellSize_ = DEFAULT_CELL_M;
        cols_ = rows_ = 0;
        cells_.clear();
        costs_.clear();
        return;
    }

    originX_ = costmap.originX();
    originY_ = costmap.originY();
    cellSize_ = costmap.cellSize();
    cols_ = costmap.cols();
    rows_ = costmap.rows();
    cells_ = costmap.cells();
    costs_ = costmap.costs();
}

// ── A* ────────────────────────────────────────────────────────────────────────

std::vector<Point> GridMap::planPath(const Point& src, const Point& dst) const {
    if (cells_.empty()) return {};

    // Convert to grid coords
    int sx, sy, dx, dy;
    if (!worldToGrid(src.x, src.y, sx, sy)) return {};

    // If destination is out-of-bounds or OCCUPIED, find nearest free cell
    bool dstOk = worldToGrid(dst.x, dst.y, dx, dy);
    if (!dstOk || cellAt(dx, dy) == Costmap::Cell::OCCUPIED) {
        float bestDist = 1e30f;
        bool foundFree = false;
        for (int gy = 0; gy < rows_; ++gy) {
            for (int gx = 0; gx < cols_; ++gx) {
                if (cellAt(gx, gy) == Costmap::Cell::OCCUPIED) continue;
                const Point w = gridToWorld(gx, gy);
                const float ddx = w.x - dst.x;
                const float ddy = w.y - dst.y;
                const float d = ddx*ddx + ddy*ddy;
                if (d < bestDist) {
                    bestDist = d;
                    dx = gx;
                    dy = gy;
                    foundFree = true;
                }
            }
        }
        if (!foundFree) return {};
    }

    // If source is OCCUPIED (robot inside obstacle), relax it
    if (cellAt(sx, sy) == Costmap::Cell::OCCUPIED) {
        // Allow starting from OCCUPIED cell — A* will route out
    }

    const int total = cols_ * rows_;
    const int goalIdx = dy * cols_ + dx;
    const int startIdx = sy * cols_ + sx;

    if (startIdx == goalIdx) {
        return { dst };
    }

    // A* with f = g + h (Euclidean heuristic)
    struct Node {
        float f;
        int   idx;
        bool operator>(const Node& o) const { return f > o.f; }
    };

    std::vector<float> gCost(total, 1e30f);
    std::vector<int>   parent(total, -1);
    std::vector<bool>  closed(total, false);

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;

    gCost[startIdx] = 0.0f;
    const Point goal = gridToWorld(dx, dy);
    const auto heuristic = [&](int idx) -> float {
        const int gx = idx % cols_;
        const int gy = idx / cols_;
        const Point w = gridToWorld(gx, gy);
        const float ddx = w.x - goal.x;
        const float ddy = w.y - goal.y;
        return std::sqrt(ddx*ddx + ddy*ddy);
    };

    openSet.push({ heuristic(startIdx), startIdx });

    // 8-directional offsets
    static constexpr int DDX[8] = {1, 1, 0,-1,-1,-1, 0, 1};
    static constexpr int DDY[8] = {0, 1, 1, 1, 0,-1,-1,-1};
    static constexpr float COST[8] = {
        1.0f, 1.4142f, 1.0f, 1.4142f, 1.0f, 1.4142f, 1.0f, 1.4142f
    };

    bool found = false;
    while (!openSet.empty()) {
        const auto [f, idx] = openSet.top();
        openSet.pop();

        if (closed[idx]) continue;
        closed[idx] = true;

        if (idx == goalIdx) { found = true; break; }

        const int cx = idx % cols_;
        const int cy = idx / cols_;

        for (int d = 0; d < 8; ++d) {
            const int nx = cx + DDX[d];
            const int ny = cy + DDY[d];
            if (nx < 0 || nx >= cols_ || ny < 0 || ny >= rows_) continue;
            const int nidx = ny * cols_ + nx;
            if (closed[nidx])                          continue;
            if (cellAt(nx, ny) == Costmap::Cell::OCCUPIED)      continue;
            if ((DDX[d] != 0) && (DDY[d] != 0)) {
                if (cellAt(cx + DDX[d], cy) == Costmap::Cell::OCCUPIED ||
                    cellAt(cx, cy + DDY[d]) == Costmap::Cell::OCCUPIED) {
                    continue;
                }
            }

            const float ng = gCost[idx] + COST[d] * cellSize_ + traversalCostAt(nx, ny) * cellSize_;
            if (ng < gCost[nidx]) {
                gCost[nidx]  = ng;
                parent[nidx] = idx;
                openSet.push({ ng + heuristic(nidx), nidx });
            }
        }
    }

    if (!found) return {};

    // Reconstruct path (reverse order)
    std::vector<Point> path;
    for (int cur = goalIdx; cur != startIdx; cur = parent[cur]) {
        if (cur == -1) return {};  // broken chain
        path.push_back(gridToWorld(cur % cols_, cur / cols_));
    }
    std::reverse(path.begin(), path.end());

    // Replace last point with exact dst (avoids grid quantisation at goal)
    if (!path.empty()) path.back() = dst;

    return path;
}

// ── Path smoothing (E.2-d) ────────────────────────────────────────────────────
//
// Greedy forward-visibility string-pull:
//   From each current point, find the furthest successor with clear line-of-sight.
//   Jump directly to it, discarding intermediate waypoints.

std::vector<Point> GridMap::smoothPath(const std::vector<Point>& path) const {
    if (path.size() <= 2) return path;

    std::vector<Point> result;
    result.push_back(path.front());

    size_t i = 0;
    while (i < path.size() - 1) {
        // Find furthest reachable point from path[i]
        size_t j = path.size() - 1;
        while (j > i + 1) {
            if (lineIsClear(path[i], path[j])) break;
            --j;
        }
        result.push_back(path[j]);
        i = j;
    }

    return result;
}

} // namespace nav
} // namespace sunray
