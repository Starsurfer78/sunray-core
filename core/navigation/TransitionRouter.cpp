/// TransitionRouter.cpp — binary-grid A* planner for zone/dock transitions.
///
/// Grid construction:
///   - Bounding box = perimeter (or from/to if no perimeter) + 3-cell margin.
///   - Cell free  if inside perimeter (or allowOutsidePerimeter) AND outside every hard exclusion.
///   - Cell blocked otherwise.
///
/// A*: 8-connected, g = accumulated Euclidean step cost, h = Euclidean distance to goal.
///     Path reconstruction walks cameFrom chain back to start.
///
/// Post-processing: collinear intermediate waypoints are removed (tolerance 0.05 m).

#include "core/navigation/TransitionRouter.h"
#include "core/navigation/Map.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace
{
    using namespace sunray::nav;

    // ── Point-in-polygon (ray casting) ────────────────────────────────────────

    static bool pointInPolygon(float x, float y, const PolygonPoints &poly)
    {
        bool inside = false;
        for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        {
            const auto &pi = poly[i];
            const auto &pj = poly[j];
            if (((pi.y > y) != (pj.y > y)) &&
                (x < (pj.x - pi.x) * (y - pi.y) / (pj.y - pi.y) + pi.x))
                inside = !inside;
        }
        return inside;
    }

    // ── Binary grid ───────────────────────────────────────────────────────────

    struct Grid
    {
        std::vector<bool> free; ///< true = traversable
        int rows = 0, cols = 0;
        float originX = 0.0f, originY = 0.0f;
        float cellSize = 0.15f;

        bool isFree(int r, int c) const
        {
            if (r < 0 || r >= rows || c < 0 || c >= cols)
                return false;
            return free[static_cast<std::size_t>(r * cols + c)];
        }

        /// World → grid cell (clamped to grid bounds).
        void worldToCell(float x, float y, int &outR, int &outC) const
        {
            outC = static_cast<int>((x - originX) / cellSize);
            outR = static_cast<int>((y - originY) / cellSize);
            outC = std::clamp(outC, 0, cols - 1);
            outR = std::clamp(outR, 0, rows - 1);
        }

        /// Grid cell centre → world Point.
        Point cellToWorld(int r, int c) const
        {
            return {originX + (static_cast<float>(c) + 0.5f) * cellSize,
                    originY + (static_cast<float>(r) + 0.5f) * cellSize};
        }
    };

    static Grid buildGrid(
        float minX, float maxX, float minY, float maxY,
        float cellSize,
        const PolygonPoints &perimeter,
        const std::vector<PolygonPoints> &hardExclusions,
        bool allowOutsidePerimeter)
    {
        Grid grid;
        grid.cellSize = cellSize;
        grid.originX = minX;
        grid.originY = minY;
        grid.cols = std::max(1, static_cast<int>(std::ceil((maxX - minX) / cellSize)));
        grid.rows = std::max(1, static_cast<int>(std::ceil((maxY - minY) / cellSize)));
        grid.free.assign(static_cast<std::size_t>(grid.rows * grid.cols), true);

        for (int r = 0; r < grid.rows; ++r)
        {
            for (int c = 0; c < grid.cols; ++c)
            {
                const float x = grid.originX + (static_cast<float>(c) + 0.5f) * cellSize;
                const float y = grid.originY + (static_cast<float>(r) + 0.5f) * cellSize;

                // Outside perimeter = blocked (unless the caller allows it)
                if (!allowOutsidePerimeter && perimeter.size() >= 3 &&
                    !pointInPolygon(x, y, perimeter))
                {
                    grid.free[static_cast<std::size_t>(r * grid.cols + c)] = false;
                    continue;
                }

                // Inside any hard exclusion = blocked
                for (const auto &excl : hardExclusions)
                {
                    if (excl.size() >= 3 && pointInPolygon(x, y, excl))
                    {
                        grid.free[static_cast<std::size_t>(r * grid.cols + c)] = false;
                        break;
                    }
                }
            }
        }
        return grid;
    }

    // ── A* ────────────────────────────────────────────────────────────────────

    // 8-connected neighbour offsets and their move costs (diagonal = √2)
    static constexpr int DR[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static constexpr int DC[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr float COST[8] = {1.41421f, 1.0f, 1.41421f, 1.0f, 1.0f, 1.41421f, 1.0f, 1.41421f};

    static std::vector<int> runAstar(const Grid &grid, int startR, int startC, int goalR, int goalC)
    {
        const int N = grid.rows * grid.cols;
        const int startEnc = startR * grid.cols + startC;
        const int goalEnc = goalR * grid.cols + goalC;

        if (startEnc == goalEnc)
            return {startEnc};

        std::vector<float> gScore(static_cast<std::size_t>(N), std::numeric_limits<float>::infinity());
        std::vector<int> cameFrom(static_cast<std::size_t>(N), -1);

        gScore[static_cast<std::size_t>(startEnc)] = 0.0f;

        using PQ = std::pair<float, int>; // (f, encoded)
        std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
        const float hStart = std::hypot(static_cast<float>(startR - goalR),
                                        static_cast<float>(startC - goalC));
        open.push({hStart, startEnc});

        while (!open.empty())
        {
            const auto [f, currEnc] = open.top();
            open.pop();

            if (currEnc == goalEnc)
            {
                // Reconstruct
                std::vector<int> path;
                int e = goalEnc;
                while (e != -1)
                {
                    path.push_back(e);
                    e = cameFrom[static_cast<std::size_t>(e)];
                }
                std::reverse(path.begin(), path.end());
                return path;
            }

            const int cr = currEnc / grid.cols;
            const int cc = currEnc % grid.cols;
            const float currG = gScore[static_cast<std::size_t>(currEnc)];

            // Prune stale entries
            if (currG > gScore[static_cast<std::size_t>(currEnc)] + 1e-6f)
                continue;

            for (int i = 0; i < 8; ++i)
            {
                const int nr = cr + DR[i];
                const int nc = cc + DC[i];
                if (!grid.isFree(nr, nc))
                    continue;

                const int nbrEnc = nr * grid.cols + nc;
                const float tentG = currG + COST[i];
                if (tentG < gScore[static_cast<std::size_t>(nbrEnc)])
                {
                    gScore[static_cast<std::size_t>(nbrEnc)] = tentG;
                    cameFrom[static_cast<std::size_t>(nbrEnc)] = currEnc;
                    const float h = std::hypot(static_cast<float>(nr - goalR),
                                               static_cast<float>(nc - goalC));
                    open.push({tentG + h, nbrEnc});
                }
            }
        }
        return {}; // no path found
    }

    // ── Path simplification ───────────────────────────────────────────────────

    /// Remove near-collinear intermediary points.
    static std::vector<Point> simplifyPath(const std::vector<Point> &pts, float tol = 0.05f)
    {
        if (pts.size() <= 2)
            return pts;
        std::vector<Point> out;
        out.push_back(pts.front());
        for (std::size_t i = 1; i + 1 < pts.size(); ++i)
        {
            const Point &a = out.back();
            const Point &b = pts[i];
            const Point &c = pts[i + 1];
            // Cross product magnitude / segment length = perpendicular distance
            const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            const float len = a.distanceTo(c);
            if (len > 1e-6f && std::fabs(cross) / len > tol)
                out.push_back(b);
        }
        out.push_back(pts.back());
        return out;
    }

    /// Try to free a blocked cell by searching in a small spiral neighbourhood.
    static bool freeNearby(const Grid &grid, int &r, int &c)
    {
        if (grid.isFree(r, c))
            return true;
        for (int radius = 1; radius <= 3; ++radius)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                for (int dc = -radius; dc <= radius; ++dc)
                {
                    if (std::abs(dr) != radius && std::abs(dc) != radius)
                        continue; // only the border of the square
                    const int nr = r + dr, nc = c + dc;
                    if (grid.isFree(nr, nc))
                    {
                        r = nr;
                        c = nc;
                        return true;
                    }
                }
            }
        }
        return false;
    }

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
namespace sunray
{
    namespace nav
    {

        std::vector<Point> TransitionRouter::plan(
            const Point &from,
            const Point &to,
            const Map &map,
            const Options &opts)
        {
            const auto &perimeter = map.perimeterPoints();
            const auto &allExcl = map.exclusions();
            const auto &allMeta = map.exclusionMeta();

            // Collect hard exclusions only
            std::vector<PolygonPoints> hardExcl;
            for (std::size_t i = 0; i < allExcl.size(); ++i)
            {
                if (i < allMeta.size() && allMeta[i].type == ExclusionType::HARD)
                    hardExcl.push_back(allExcl[i]);
                else if (i >= allMeta.size()) // no meta → treat as hard
                    hardExcl.push_back(allExcl[i]);
            }

            // Bounding box: perimeter (if present) and from/to points
            float minX = std::min(from.x, to.x), maxX = std::max(from.x, to.x);
            float minY = std::min(from.y, to.y), maxY = std::max(from.y, to.y);
            for (const auto &p : perimeter)
            {
                minX = std::min(minX, p.x);
                maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y);
                maxY = std::max(maxY, p.y);
            }

            const float margin = opts.cellSize_m * 4.0f;
            minX -= margin;
            minY -= margin;
            maxX += margin;
            maxY += margin;

            // Sanity: guard against unreasonably large grids
            const int cols = static_cast<int>(std::ceil((maxX - minX) / opts.cellSize_m));
            const int rows = static_cast<int>(std::ceil((maxY - minY) / opts.cellSize_m));
            if (cols <= 0 || rows <= 0 || cols > 5000 || rows > 5000)
                return {}; // area too large or degenerate

            const Grid grid = buildGrid(
                minX, maxX, minY, maxY,
                opts.cellSize_m,
                perimeter,
                hardExcl,
                opts.allowOutsidePerimeter);

            int startR, startC, goalR, goalC;
            grid.worldToCell(from.x, from.y, startR, startC);
            grid.worldToCell(to.x, to.y, goalR, goalC);

            // Ensure start/goal cells are not blocked (shift to nearest free cell)
            if (!freeNearby(grid, startR, startC) || !freeNearby(grid, goalR, goalC))
                return {}; // impossible to plan

            const auto cellPath = runAstar(grid, startR, startC, goalR, goalC);
            if (cellPath.empty())
                return {};

            // Convert cell path to world points
            std::vector<Point> worldPath;
            worldPath.reserve(cellPath.size() + 2);
            worldPath.push_back(from); // exact start
            for (const int enc : cellPath)
                worldPath.push_back(grid.cellToWorld(enc / grid.cols, enc % grid.cols));
            worldPath.push_back(to); // exact goal

            return simplifyPath(worldPath);
        }

    } // namespace nav
} // namespace sunray
