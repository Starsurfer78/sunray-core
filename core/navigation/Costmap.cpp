#include "core/navigation/Costmap.h"

#include <algorithm>
#include <cmath>

namespace sunray {
namespace nav {

const std::vector<Costmap::Cell>& Costmap::layerCells(LayerKind kind) const {
    switch (kind) {
        case LayerKind::STATIC_GEOMETRY: return staticGeometryCells_;
        case LayerKind::STATIC_EXCLUSION: return staticExclusionCells_;
        case LayerKind::DYNAMIC_OBSTACLE: return dynamicObstacleCells_;
        case LayerKind::DOCK_PREFERENCE: return dockPreferenceCells_;
    }
    return cells_;
}

const std::vector<float>& Costmap::layerCosts(LayerKind kind) const {
    switch (kind) {
        case LayerKind::STATIC_GEOMETRY: return staticGeometryCosts_;
        case LayerKind::STATIC_EXCLUSION: return staticExclusionCosts_;
        case LayerKind::DYNAMIC_OBSTACLE: return dynamicObstacleCosts_;
        case LayerKind::DOCK_PREFERENCE: return dockPreferenceCosts_;
    }
    return costs_;
}

std::vector<Costmap::Cell>& Costmap::layerCellsMutable(LayerKind kind) {
    switch (kind) {
        case LayerKind::STATIC_GEOMETRY: return staticGeometryCells_;
        case LayerKind::STATIC_EXCLUSION: return staticExclusionCells_;
        case LayerKind::DYNAMIC_OBSTACLE: return dynamicObstacleCells_;
        case LayerKind::DOCK_PREFERENCE: return dockPreferenceCells_;
    }
    return cells_;
}

std::vector<float>& Costmap::layerCostsMutable(LayerKind kind) {
    switch (kind) {
        case LayerKind::STATIC_GEOMETRY: return staticGeometryCosts_;
        case LayerKind::STATIC_EXCLUSION: return staticExclusionCosts_;
        case LayerKind::DYNAMIC_OBSTACLE: return dynamicObstacleCosts_;
        case LayerKind::DOCK_PREFERENCE: return dockPreferenceCosts_;
    }
    return costs_;
}

void Costmap::resetStorage() {
    const size_t total = static_cast<size_t>(cols_ * rows_);
    cells_.assign(total, Cell::EMPTY);
    costs_.assign(total, 0.0f);
    staticGeometryCells_.assign(total, Cell::EMPTY);
    staticGeometryCosts_.assign(total, 0.0f);
    staticExclusionCells_.assign(total, Cell::EMPTY);
    staticExclusionCosts_.assign(total, 0.0f);
    dynamicObstacleCells_.assign(total, Cell::EMPTY);
    dynamicObstacleCosts_.assign(total, 0.0f);
    dockPreferenceCells_.assign(total, Cell::EMPTY);
    dockPreferenceCosts_.assign(total, 0.0f);
}

void Costmap::composeLayers() {
    const size_t total = static_cast<size_t>(cols_ * rows_);
    cells_.assign(total, Cell::EMPTY);
    costs_.assign(total, 0.0f);

    const std::vector<LayerKind> order{
        LayerKind::STATIC_GEOMETRY,
        LayerKind::STATIC_EXCLUSION,
        LayerKind::DYNAMIC_OBSTACLE,
        LayerKind::DOCK_PREFERENCE,
    };

    for (LayerKind layer : order) {
        const auto& lc = layerCells(layer);
        const auto& lk = layerCosts(layer);
        for (size_t i = 0; i < total; ++i) {
            if (lc[i] == Cell::OCCUPIED) {
                cells_[i] = Cell::OCCUPIED;
            }
            costs_[i] += lk[i];
        }
    }
}

bool Costmap::worldToGrid(float wx, float wy, int& gx, int& gy) const {
    gx = static_cast<int>(std::floor((wx - originX_) / cellSize_)) + cols_ / 2;
    gy = static_cast<int>(std::floor((wy - originY_) / cellSize_)) + rows_ / 2;
    return gx >= 0 && gx < cols_ && gy >= 0 && gy < rows_;
}

Point Costmap::gridToWorld(int gx, int gy) const {
    return {
        originX_ + (static_cast<float>(gx) - cols_ * 0.5f + 0.5f) * cellSize_,
        originY_ + (static_cast<float>(gy) - rows_ * 0.5f + 0.5f) * cellSize_
    };
}

Costmap::Cell Costmap::cellAt(int gx, int gy) const {
    if (gx < 0 || gx >= cols_ || gy < 0 || gy >= rows_) return Cell::OCCUPIED;
    return cells_[gy * cols_ + gx];
}

float Costmap::traversalCostAt(int gx, int gy) const {
    if (gx < 0 || gx >= cols_ || gy < 0 || gy >= rows_) return 1e6f;
    return costs_.empty() ? 0.0f : costs_[gy * cols_ + gx];
}

bool Costmap::lineIsClear(const Point& a, const Point& b) const {
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
        if (cellAt(gx, gy) == Cell::OCCUPIED) return false;
    }
    return true;
}

void Costmap::markCircle(std::vector<Cell>& layerCells, float cx, float cy, float radius) {
    if (cellSize_ <= 0.0f || cols_ <= 0 || rows_ <= 0) return;

    const int gxMin = std::max(0, static_cast<int>(std::floor((cx - radius - originX_) / cellSize_)) + cols_ / 2 - 1);
    const int gxMax = std::min(cols_ - 1, static_cast<int>(std::floor((cx + radius - originX_) / cellSize_)) + cols_ / 2 + 1);
    const int gyMin = std::max(0, static_cast<int>(std::floor((cy - radius - originY_) / cellSize_)) + rows_ / 2 - 1);
    const int gyMax = std::min(rows_ - 1, static_cast<int>(std::floor((cy + radius - originY_) / cellSize_)) + rows_ / 2 + 1);

    for (int gy = gyMin; gy <= gyMax; ++gy) {
        for (int gx = gxMin; gx <= gxMax; ++gx) {
            const Point world = gridToWorld(gx, gy);
            const float ddx = world.x - cx;
            const float ddy = world.y - cy;
            if (ddx * ddx + ddy * ddy <= radius * radius) {
                layerCells[gy * cols_ + gx] = Cell::OCCUPIED;
            }
        }
    }
}

bool Costmap::pointInPolygon(const PolygonPoints& poly, float px, float py) {
    bool inside = false;
    const int n = static_cast<int>(poly.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const float xi = poly[i].x, yi = poly[i].y;
        const float xj = poly[j].x, yj = poly[j].y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

float Costmap::distancePointToSegment(float px, float py, const Point& a, const Point& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-12f) {
        const float ex = px - a.x;
        const float ey = py - a.y;
        return std::sqrt(ex * ex + ey * ey);
    }
    const float t = std::clamp(((px - a.x) * dx + (py - a.y) * dy) / len2, 0.0f, 1.0f);
    const float cx = a.x + t * dx;
    const float cy = a.y + t * dy;
    const float ex = px - cx;
    const float ey = py - cy;
    return std::sqrt(ex * ex + ey * ey);
}

float Costmap::distancePointToPolygonEdges(const PolygonPoints& poly, float px, float py) {
    if (poly.size() < 2) return 1e6f;
    float best = 1e6f;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        best = std::min(best, distancePointToSegment(px, py, poly[j], poly[i]));
    }
    return best;
}

void Costmap::markExterior(std::vector<Cell>& layerCells, const PolygonPoints& poly) {
    if (poly.empty()) return;
    for (int gy = 0; gy < rows_; ++gy) {
        for (int gx = 0; gx < cols_; ++gx) {
            const Point w = gridToWorld(gx, gy);
            if (!pointInPolygon(poly, w.x, w.y)) {
                layerCells[gy * cols_ + gx] = Cell::OCCUPIED;
            }
        }
    }
}

void Costmap::markInterior(std::vector<Cell>& layerCells, const PolygonPoints& poly) {
    if (poly.empty()) return;
    for (int gy = 0; gy < rows_; ++gy) {
        for (int gx = 0; gx < cols_; ++gx) {
            const Point w = gridToWorld(gx, gy);
            if (pointInPolygon(poly, w.x, w.y)) {
                layerCells[gy * cols_ + gx] = Cell::OCCUPIED;
            }
        }
    }
}

void Costmap::addInteriorSoftCost(const std::vector<Cell>& layerCells,
                                  std::vector<float>& layerCosts,
                                  const PolygonPoints& poly,
                                  float cost) {
    if (poly.empty() || cost <= 0.0f) return;
    for (int gy = 0; gy < rows_; ++gy) {
        for (int gx = 0; gx < cols_; ++gx) {
            const Point w = gridToWorld(gx, gy);
            const size_t idx = static_cast<size_t>(gy * cols_ + gx);
            if (pointInPolygon(poly, w.x, w.y) && layerCells[idx] != Cell::OCCUPIED) {
                layerCosts[idx] += cost;
            }
        }
    }
}

void Costmap::addPolygonEdgeMarginCost(std::vector<Cell>& layerCells,
                                       std::vector<float>& layerCosts,
                                       const PolygonPoints& poly,
                                       float hardMargin,
                                       float softMargin,
                                       float costScale) {
    if (poly.size() < 3) return;
    for (int gy = 0; gy < rows_; ++gy) {
        for (int gx = 0; gx < cols_; ++gx) {
            const Point w = gridToWorld(gx, gy);
            if (!pointInPolygon(poly, w.x, w.y)) continue;
            const float dist = distancePointToPolygonEdges(poly, w.x, w.y);
            const size_t idx = static_cast<size_t>(gy * cols_ + gx);

            if (hardMargin > 0.0f && dist < hardMargin) {
                layerCells[idx] = Cell::OCCUPIED;
                layerCosts[idx] = 0.0f;
                continue;
            }
            if (softMargin > hardMargin && dist < softMargin && layerCells[idx] != Cell::OCCUPIED) {
                const float t = 1.0f - ((dist - hardMargin) / std::max(0.001f, softMargin - hardMargin));
                layerCosts[idx] += std::max(0.0f, t) * std::max(0.0f, costScale);
            }
        }
    }
}

void Costmap::buildFromMap(const Map& map,
                           float originX, float originY,
                           float cellSize_m, int cols, int rows,
                           float robotRadius,
                           WayType planningMode) {
    if (cellSize_m <= 0.0f || cols <= 0 || rows <= 0) {
        originX_ = originX;
        originY_ = originY;
        cellSize_ = DEFAULT_CELL_M;
        cols_ = rows_ = 0;
        cells_.clear();
        costs_.clear();
        return;
    }

    originX_  = originX;
    originY_  = originY;
    cellSize_ = cellSize_m;
    cols_     = cols;
    rows_     = rows;
    resetStorage();

    auto& staticGeometryCells = layerCellsMutable(LayerKind::STATIC_GEOMETRY);
    auto& staticGeometryCosts = layerCostsMutable(LayerKind::STATIC_GEOMETRY);
    auto& staticExclusionCells = layerCellsMutable(LayerKind::STATIC_EXCLUSION);
    auto& staticExclusionCosts = layerCostsMutable(LayerKind::STATIC_EXCLUSION);
    auto& dynamicObstacleCells = layerCellsMutable(LayerKind::DYNAMIC_OBSTACLE);
    auto& dockPreferenceCosts = layerCostsMutable(LayerKind::DOCK_PREFERENCE);

    markExterior(staticGeometryCells, map.perimeterPoints());
    addPolygonEdgeMarginCost(staticGeometryCells,
                             staticGeometryCosts,
                             map.perimeterPoints(),
                             map.plannerSettings().perimeterHardMargin_m,
                             map.plannerSettings().perimeterSoftMargin_m,
                             4.0f);

    const auto& exclusions = map.exclusions();
    const auto& exclusionMeta = map.exclusionMeta();
    for (size_t i = 0; i < exclusions.size(); ++i) {
        const auto& ex = exclusions[i];
        const ExclusionMeta meta = (i < exclusionMeta.size()) ? exclusionMeta[i] : ExclusionMeta{};
        if (meta.type == ExclusionType::HARD) {
            markInterior(staticExclusionCells, ex);
            addPolygonEdgeMarginCost(staticExclusionCells,
                                     staticExclusionCosts,
                                     ex,
                                     meta.clearance_m,
                                     meta.clearance_m + map.plannerSettings().defaultClearance_m,
                                     6.0f);
        } else {
            addInteriorSoftCost(staticExclusionCells,
                                staticExclusionCosts,
                                ex,
                                std::max(0.5f, meta.costScale * 4.0f));
            addPolygonEdgeMarginCost(staticExclusionCells,
                                     staticExclusionCosts,
                                     ex,
                                     0.0f,
                                     std::max(meta.clearance_m, 0.1f),
                                     std::max(0.5f, meta.costScale * 3.0f));
        }
    }

    for (const auto& obs : map.obstacles()) {
        markCircle(dynamicObstacleCells, obs.center.x, obs.center.y, obs.radius_m + robotRadius);
    }

    if (planningMode == WayType::DOCK && !map.dockMeta().corridor.empty()) {
        for (int gy = 0; gy < rows_; ++gy) {
            for (int gx = 0; gx < cols_; ++gx) {
                const Point w = gridToWorld(gx, gy);
                if (!pointInPolygon(map.dockMeta().corridor, w.x, w.y) && cellAt(gx, gy) != Cell::OCCUPIED) {
                    dockPreferenceCosts[gy * cols_ + gx] += 3.0f;
                }
            }
        }
    }

    composeLayers();
}

} // namespace nav
} // namespace sunray
