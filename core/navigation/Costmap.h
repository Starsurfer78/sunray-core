#pragma once

#include "core/navigation/Route.h"
#include "core/navigation/Map.h"

#include <cstdint>
#include <vector>

namespace sunray {
namespace nav {

class Costmap {
public:
    enum class Cell : uint8_t { EMPTY = 0, OCCUPIED = 1 };
    enum class LayerKind : uint8_t {
        STATIC_GEOMETRY = 0,
        STATIC_EXCLUSION = 1,
        DYNAMIC_OBSTACLE = 2,
        DOCK_PREFERENCE = 3,
    };

    static constexpr float DEFAULT_CELL_M      = 0.25f;
    static constexpr int   DEFAULT_COLS        = 40;
    static constexpr int   DEFAULT_ROWS        = 40;
    static constexpr float DEFAULT_ROBOT_RAD_M = 0.0f;

    void buildFromMap(const Map& map,
                      float originX, float originY,
                      float cellSize_m   = DEFAULT_CELL_M,
                      int   cols         = DEFAULT_COLS,
                      int   rows         = DEFAULT_ROWS,
                      float robotRadius  = DEFAULT_ROBOT_RAD_M,
                      WayType planningMode = WayType::FREE,
                      const PolygonPoints& constraintZone = {});

    bool isBuilt() const { return !cells_.empty(); }

    float originX() const { return originX_; }
    float originY() const { return originY_; }
    float cellSize() const { return cellSize_; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    const std::vector<Cell>& cells() const { return cells_; }
    const std::vector<float>& costs() const { return costs_; }
    const std::vector<Cell>& layerCells(LayerKind kind) const;
    const std::vector<float>& layerCosts(LayerKind kind) const;

    bool  worldToGrid(float wx, float wy, int& gx, int& gy) const;
    Point gridToWorld(int gx, int gy) const;
    Cell  cellAt(int gx, int gy) const;
    float traversalCostAt(int gx, int gy) const;
    bool  lineIsClear(const Point& a, const Point& b) const;

private:
    float originX_  = 0.0f;
    float originY_  = 0.0f;
    float cellSize_ = DEFAULT_CELL_M;
    int   cols_     = 0;
    int   rows_     = 0;
    std::vector<Cell> cells_;
    std::vector<float> costs_;
    std::vector<Cell> staticGeometryCells_;
    std::vector<float> staticGeometryCosts_;
    std::vector<Cell> staticExclusionCells_;
    std::vector<float> staticExclusionCosts_;
    std::vector<Cell> dynamicObstacleCells_;
    std::vector<float> dynamicObstacleCosts_;
    std::vector<Cell> dockPreferenceCells_;
    std::vector<float> dockPreferenceCosts_;

    void resetStorage();
    void composeLayers();
    std::vector<Cell>& layerCellsMutable(LayerKind kind);
    std::vector<float>& layerCostsMutable(LayerKind kind);
    void markCircle(std::vector<Cell>& layerCells, float cx, float cy, float radius);
    void markExterior(std::vector<Cell>& layerCells, const PolygonPoints& poly);
    void markInterior(std::vector<Cell>& layerCells, const PolygonPoints& poly);
    void addInteriorSoftCost(const std::vector<Cell>& layerCells, std::vector<float>& layerCosts, const PolygonPoints& poly, float cost);
    void addPolygonEdgeMarginCost(std::vector<Cell>& layerCells,
                                  std::vector<float>& layerCosts,
                                  const PolygonPoints& poly,
                                  float hardMargin,
                                  float softMargin,
                                  float costScale);

    static bool pointInPolygon(const PolygonPoints& poly, float px, float py);
    static float distancePointToSegment(float px, float py, const Point& a, const Point& b);
    static float distancePointToPolygonEdges(const PolygonPoints& poly, float px, float py);
};

} // namespace nav
} // namespace sunray
