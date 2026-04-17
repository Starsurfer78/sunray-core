<script lang="ts">
  import { createEventDispatcher } from "svelte";
  import { onDestroy } from "svelte";
  import { telemetry } from "../../stores/telemetry";
  import type { AddPointResult, Point } from "../../stores/map";
  import { mapStore } from "../../stores/map";
  import {
    getActivePlan,
    getLiveOverlay,
    type ActivePlanResponse,
    type PlannerPreviewRoutePoint,
    type RouteSemantic,
    type WaypointEntry,
    type LiveOverlayResponse,
    type ObstacleEntry,
    type DetourEntry,
  } from "../../api/rest";

  export let interactive = true;
  export let showHeader = true;
  export let showViewportActions = true;
  export let showRobot = true;
  export let showZoomControls = false;
  export let allowPointAdd = true;
  export let pointAddBlockedReason = "";
  /** N7.1: When false, suppress all runtime overlays (active plan, detour, obstacles,
   *  target crosshair). Use on pages that edit static geometry (Map page). */
  export let showRuntimeLayers = true;

  type CoordMode = "local" | "wgs84";
  let coordMode: CoordMode = "local";

  function detectCoordMode(
    perimeter: Point[],
    gpsLat: number,
    gpsLon: number,
  ): CoordMode {
    if (perimeter.length < 3) return "local";
    if (gpsLat === 0 || gpsLon === 0) return "local";

    let sumX = 0;
    let sumY = 0;
    for (const p of perimeter) {
      sumX += p.x;
      sumY += p.y;
    }
    const avgX = sumX / perimeter.length;
    const avgY = sumY / perimeter.length;

    const closeToGps =
      Math.abs(avgY - gpsLat) < 1 && Math.abs(avgX - gpsLon) < 1;
    if (!closeToGps) return "local";

    const minX = Math.min(...perimeter.map((p) => p.x));
    const maxX = Math.max(...perimeter.map((p) => p.x));
    const minY = Math.min(...perimeter.map((p) => p.y));
    const maxY = Math.max(...perimeter.map((p) => p.y));
    const spanX = maxX - minX;
    const spanY = maxY - minY;

    if (
      avgY >= -90 &&
      avgY <= 90 &&
      avgX >= -180 &&
      avgX <= 180 &&
      spanX > 0 &&
      spanX < 0.5 &&
      spanY > 0 &&
      spanY < 0.5
    ) {
      return "wgs84";
    }
    return "local";
  }

  const dispatch = createEventDispatcher<{
    pointrejected: { tool: string; reason?: string };
  }>();

  // Actual container dimensions — kept in sync with CSS layout via bind:clientWidth/Height.
  // Defaults are used only before the first layout measurement.
  let width = 900;
  let height = 620;

  const RAD_TO_DEG = 180 / Math.PI;
  const baseScale = 20;
  const grid = 1;
  const pointHitRadius = 10;
  const minZoom = 0.2;
  const maxZoom = 6;
  const robotOuterRadiusPx = 8.4;
  const robotInnerRadiusPx = 3.2;
  const robotNoseStartPx = 8.4;
  const robotNoseEndPx = 16.4;
  const robotArrowTipPx = 19.2;
  const robotArrowSidePx = 13.2;
  const robotArrowHalfWidthPx = 3.6;

  let zoom = 1;
  let offsetX = 0;
  let offsetY = 0;

  $: currentScale = baseScale * zoom;
  let isPanning = false;
  let panStartX = 0;
  let panStartY = 0;
  let panOriginX = 0;
  let panOriginY = 0;
  let suppressClick = false;
  let pointerCaptured = false;
  let hasAutoFit = false;

  // Import clamp helper, but define a scoped version to keep MapCanvas geometry utilities isolated.
  // This avoids importing mapHelpers just for one function when there are better local patterns.
  // If it's used frequently, we can refactor later.

  function clamp(value: number, min: number, max: number) {
    return Math.max(min, Math.min(max, value));
  }

  type DragTarget =
    | { kind: "perimeter"; index: number }
    | { kind: "dock"; index: number }
    | { kind: "dock-corridor"; index: number }
    | { kind: "zone"; zoneId: string; index: number }
    | { kind: "exclusion"; exclusionIndex: number; index: number };

  let dragTarget: DragTarget | null = null;
  let selectedTarget: DragTarget | null = null;

  function canEditTarget(target: DragTarget) {
    const tool = $mapStore.selectedTool;
    if (tool === "move") return false;
    if (tool === "perimeter") return target.kind === "perimeter";
    if (tool === "dock") return target.kind === "dock";
    if (tool === "dock-corridor") return target.kind === "dock-corridor";
    if (tool === "nogo") {
      return (
        target.kind === "exclusion" &&
        target.exclusionIndex === $mapStore.selectedExclusionIndex
      );
    }
    if (tool === "zone") {
      return (
        target.kind === "zone" && target.zoneId === $mapStore.selectedZoneId
      );
    }
    return false;
  }

  function worldToScreen(
    point: Point,
    scaleValue = currentScale,
    xOffset = offsetX,
    yOffset = offsetY,
  ) {
    return {
      x: width / 2 + xOffset + point.x * scaleValue,
      y: height / 2 + yOffset - point.y * scaleValue,
    };
  }

  function clientToSvgCoordinates(
    clientX: number,
    clientY: number,
    target: SVGSVGElement,
  ) {
    const point = target.createSVGPoint();
    point.x = clientX;
    point.y = clientY;
    const ctm = target.getScreenCTM();

    if (!ctm) {
      const rect = target.getBoundingClientRect();
      const scaleX = width / rect.width;
      const scaleY = height / rect.height;
      return {
        x: (clientX - rect.left) * scaleX,
        y: (clientY - rect.top) * scaleY,
      };
    }

    const svgPoint = point.matrixTransform(ctm.inverse());
    return {
      x: svgPoint.x,
      y: svgPoint.y,
    };
  }

  function screenToWorldFromCoordinates(
    clientX: number,
    clientY: number,
    target: SVGSVGElement,
  ) {
    const { x: sx, y: sy } = clientToSvgCoordinates(clientX, clientY, target);
    return {
      x: Number(((sx - width / 2 - offsetX) / currentScale).toFixed(2)),
      y: Number(((height / 2 + offsetY - sy) / currentScale).toFixed(2)),
    };
  }

  function screenToWorld(event: MouseEvent, target: SVGSVGElement) {
    return screenToWorldFromCoordinates(event.clientX, event.clientY, target);
  }

  function distanceSquared(
    a: { x: number; y: number },
    b: { x: number; y: number },
  ) {
    const dx = a.x - b.x;
    const dy = a.y - b.y;
    return dx * dx + dy * dy;
  }

  function findDragTarget(
    clientX: number,
    clientY: number,
    target: SVGSVGElement,
  ) {
    const cursor = clientToSvgCoordinates(clientX, clientY, target);

    const tool = $mapStore.selectedTool;

    if (tool === "dock") {
      for (let index = 0; index < $mapStore.map.dock.length; index += 1) {
        const screen = worldToScreen(
          $mapStore.map.dock[index],
          currentScale,
          offsetX,
          offsetY,
        );
        if (
          distanceSquared(cursor, screen) <=
          pointHitRadius * pointHitRadius
        ) {
          return { kind: "dock", index } as DragTarget;
        }
      }
      return null;
    }

    if (tool === "dock-corridor") {
      for (
        let index = 0;
        index < ($mapStore.map.dockMeta?.corridor.length ?? 0);
        index += 1
      ) {
        const point = $mapStore.map.dockMeta?.corridor[index];
        if (!point) continue;
        const screen = worldToScreen(point, currentScale, offsetX, offsetY);
        if (
          distanceSquared(cursor, screen) <=
          pointHitRadius * pointHitRadius
        ) {
          return { kind: "dock-corridor", index } as DragTarget;
        }
      }
      return null;
    }

    if (tool === "perimeter") {
      for (let index = 0; index < $mapStore.map.perimeter.length; index += 1) {
        const screen = worldToScreen(
          $mapStore.map.perimeter[index],
          currentScale,
          offsetX,
          offsetY,
        );
        if (
          distanceSquared(cursor, screen) <=
          pointHitRadius * pointHitRadius
        ) {
          return { kind: "perimeter", index } as DragTarget;
        }
      }
      return null;
    }

    if (tool === "nogo") {
      if ($mapStore.selectedExclusionIndex === null) return null;
      const exclusion =
        $mapStore.map.exclusions[$mapStore.selectedExclusionIndex] ?? [];
      for (let index = 0; index < exclusion.length; index += 1) {
        const screen = worldToScreen(
          exclusion[index],
          currentScale,
          offsetX,
          offsetY,
        );
        if (
          distanceSquared(cursor, screen) <=
          pointHitRadius * pointHitRadius
        ) {
          return {
            kind: "exclusion",
            exclusionIndex: $mapStore.selectedExclusionIndex,
            index,
          } as DragTarget;
        }
      }
      return null;
    }

    if (tool === "zone") {
      const activeZone = $mapStore.map.zones.find(
        (zone) => zone.id === $mapStore.selectedZoneId,
      );
      if (!activeZone) return null;
      for (let index = 0; index < activeZone.polygon.length; index += 1) {
        const screen = worldToScreen(
          activeZone.polygon[index],
          currentScale,
          offsetX,
          offsetY,
        );
        if (
          distanceSquared(cursor, screen) <=
          pointHitRadius * pointHitRadius
        ) {
          return { kind: "zone", zoneId: activeZone.id, index } as DragTarget;
        }
      }
      return null;
    }

    if (tool === "move") return null;

    return null;
  }

  function applyDraggedPoint(target: DragTarget, point: Point) {
    if (!canEditTarget(target)) return;
    if (target.kind === "perimeter") {
      mapStore.movePerimeterPoint(target.index, point);
    } else if (target.kind === "dock") {
      mapStore.moveDockPoint(target.index, point);
    } else if (target.kind === "dock-corridor") {
      mapStore.moveDockCorridorPoint(target.index, point);
    } else if (target.kind === "zone") {
      mapStore.moveZonePoint(target.zoneId, target.index, point);
    } else if (target.kind === "exclusion") {
      mapStore.moveExclusionPoint(target.exclusionIndex, target.index, point);
    }
  }

  function deleteTarget(target: DragTarget) {
    if (!canEditTarget(target)) return;
    if (target.kind === "perimeter") {
      mapStore.deletePerimeterPoint(target.index);
    } else if (target.kind === "dock") {
      mapStore.deleteDockPoint(target.index);
    } else if (target.kind === "dock-corridor") {
      mapStore.deleteDockCorridorPoint(target.index);
    } else if (target.kind === "zone") {
      mapStore.deleteZonePoint(target.zoneId, target.index);
    } else if (target.kind === "exclusion") {
      mapStore.deleteExclusionPoint(target.exclusionIndex, target.index);
    }
  }

  function isSelected(target: DragTarget) {
    if (!selectedTarget || selectedTarget.kind !== target.kind) return false;
    if (
      target.kind === "perimeter" ||
      target.kind === "dock" ||
      target.kind === "dock-corridor"
    ) {
      return selectedTarget.index === target.index;
    }
    if (target.kind === "zone" && selectedTarget.kind === "zone") {
      return (
        selectedTarget.zoneId === target.zoneId &&
        selectedTarget.index === target.index
      );
    }
    if (target.kind === "exclusion" && selectedTarget.kind === "exclusion") {
      return (
        selectedTarget.exclusionIndex === target.exclusionIndex &&
        selectedTarget.index === target.index
      );
    }
    return false;
  }

  export function zoomIn() {
    zoom = clamp(Number((zoom * 1.2).toFixed(3)), minZoom, maxZoom);
  }

  export function zoomOut() {
    zoom = clamp(Number((zoom * 0.8).toFixed(3)), minZoom, maxZoom);
  }

  function handlePointerDown(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement;
    suppressClick = false;

    if (interactive) {
      const drag = findDragTarget(event.clientX, event.clientY, target);
      if (drag && canEditTarget(drag)) {
        selectedTarget = drag;
        dragTarget = drag;
        pointerCaptured = true;
        target.setPointerCapture(event.pointerId);
        return;
      }
    }

    isPanning = true;
    pointerCaptured = true;
    panStartX = event.clientX;
    panStartY = event.clientY;
    panOriginX = offsetX;
    panOriginY = offsetY;
    target.setPointerCapture(event.pointerId);
  }

  let cursorWorld: { x: number; y: number } | null = null;

  function handlePointerMove(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement;
    cursorWorld = screenToWorldFromCoordinates(
      event.clientX,
      event.clientY,
      target,
    );

    if (dragTarget) {
      applyDraggedPoint(dragTarget, cursorWorld);
      suppressClick = true;
      return;
    }

    if (isPanning) {
      offsetX = panOriginX + (event.clientX - panStartX);
      offsetY = panOriginY + (event.clientY - panStartY);
      suppressClick = true;
    }
  }

  function releasePointer(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement;
    if (pointerCaptured) {
      try {
        target.releasePointerCapture(event.pointerId);
      } catch {
        // Ignore if pointer capture is already released.
      }
    }
    pointerCaptured = false;
    dragTarget = null;
    isPanning = false;
  }

  function handleCanvasClick(event: MouseEvent) {
    if (!interactive || $mapStore.selectedTool === "move") return;
    if (suppressClick) {
      suppressClick = false;
      return;
    }
    if (!allowPointAdd) {
      dispatch("pointrejected", {
        tool: $mapStore.selectedTool,
        reason: pointAddBlockedReason,
      });
      return;
    }
    const target = event.currentTarget as SVGSVGElement;
    const result: AddPointResult = mapStore.addPoint(
      screenToWorld(event, target),
    );
    if (!result.accepted) {
      dispatch("pointrejected", {
        tool: $mapStore.selectedTool,
        reason: result.reason,
      });
    }
  }

  function selectExclusionArea(exclusionIndex: number) {
    mapStore.selectExclusion(exclusionIndex);
    if ($mapStore.selectedTool !== "move") {
      mapStore.setTool("nogo");
    }
  }

  function handleWheel(event: WheelEvent) {
    event.preventDefault();
    const target = event.currentTarget as SVGSVGElement;
    const oldZoom = zoom;
    const delta = event.deltaY < 0 ? 1.1 : 0.9;
    const newZoom = clamp(Number((zoom * delta).toFixed(3)), minZoom, maxZoom);
    if (newZoom === oldZoom) return;

    // Zoom toward cursor: keep the world point under the cursor fixed in screen space.
    const cursor = clientToSvgCoordinates(event.clientX, event.clientY, target);
    const oldScale = baseScale * oldZoom;
    const worldX = (cursor.x - width / 2 - offsetX) / oldScale;
    const worldY = (height / 2 + offsetY - cursor.y) / oldScale;
    zoom = newZoom;
    const newScale = baseScale * newZoom;
    offsetX = cursor.x - width / 2 - worldX * newScale;
    offsetY = cursor.y - height / 2 + worldY * newScale;
  }

  function path(
    points: Point[],
    scaleValue = currentScale,
    xOffset = offsetX,
    yOffset = offsetY,
  ) {
    return points
      .map((point) => {
        const p = worldToScreen(point, scaleValue, xOffset, yOffset);
        return `${p.x},${p.y}`;
      })
      .join(" ");
  }

  function centroid(points: Point[]) {
    if (points.length === 0) return null;
    const sum = points.reduce(
      (acc, point) => ({
        x: acc.x + point.x,
        y: acc.y + point.y,
      }),
      { x: 0, y: 0 },
    );
    return {
      x: sum.x / points.length,
      y: sum.y / points.length,
    };
  }

  function zoneLabelBox(text: string) {
    return Math.max(50, Math.min(120, text.length * 5.4 + 12));
  }

  function labelWidth(text: string) {
    return Math.max(44, Math.min(112, text.length * 5.1 + 12));
  }

  function labelAnchor(point: Point, text: string) {
    const screen = worldToScreen(point, currentScale, offsetX, offsetY);
    return {
      x: Math.max(10, Math.min(width - 10, screen.x)),
      y: Math.max(18, Math.min(height - 12, screen.y)),
      width: labelWidth(text),
    };
  }

  function allMapPoints() {
    const points = [
      ...$mapStore.map.perimeter,
      ...$mapStore.map.dock,
      ...($mapStore.map.dockMeta?.corridor ?? []),
      ...$mapStore.map.zones.flatMap((zone) => zone.polygon),
      ...$mapStore.map.exclusions.flatMap((exclusion) => exclusion),
    ];
    if (showRobot) {
      if (coordMode === "wgs84") {
        if ($telemetry.gps_lat !== 0 && $telemetry.gps_lon !== 0) {
          points.push({ x: $telemetry.gps_lon, y: $telemetry.gps_lat });
        }
      } else {
        points.push({ x: $telemetry.x, y: $telemetry.y });
      }
    }
    return points;
  }

  function resetView() {
    zoom = 1;
    offsetX = 0;
    offsetY = 0;
  }

  export function fitToContent() {
    const points = allMapPoints();
    if (points.length === 0) {
      resetView();
      return;
    }

    const minX = Math.min(...points.map((point) => point.x));
    const maxX = Math.max(...points.map((point) => point.x));
    const minY = Math.min(...points.map((point) => point.y));
    const maxY = Math.max(...points.map((point) => point.y));

    const minSpan = coordMode === "wgs84" ? 0.00001 : 1;
    const spanX = Math.max(maxX - minX, minSpan);
    const spanY = Math.max(maxY - minY, minSpan);
    const padding = 48;

    const availableWidth = Math.max(width - padding * 2, 120);
    const availableHeight = Math.max(height - padding * 2, 120);

    const fitScale = Math.min(availableWidth / spanX, availableHeight / spanY);
    const nextZoom = clamp(
      Number((fitScale / baseScale).toFixed(3)),
      minZoom,
      maxZoom,
    );
    const nextScale = baseScale * nextZoom;
    zoom = nextZoom;

    const centerX = (minX + maxX) / 2;
    const centerY = (minY + maxY) / 2;
    offsetX = Number((-centerX * nextScale).toFixed(2));
    offsetY = Number((centerY * nextScale).toFixed(2));
  }

  export function deleteSelectedPoint() {
    if (!selectedTarget) return false;
    deleteTarget(selectedTarget);
    selectedTarget = null;
    return true;
  }

  $: robotScreen = worldToScreen(
    coordMode === "wgs84"
      ? { x: $telemetry.gps_lon, y: $telemetry.gps_lat }
      : { x: $telemetry.x, y: $telemetry.y },
    currentScale,
    offsetX,
    offsetY,
  );
  $: robotHeadingDeg = -(($telemetry.heading ?? 0) * RAD_TO_DEG);

  // N4.2 / N4.3 — active mission plan layers (only when showRuntimeLayers) ----
  let activePlanWaypoints: WaypointEntry[] = [];
  let activePlanRoutePoints: PlannerPreviewRoutePoint[] = [];
  let activePlanWaypointIndex = -1;
  let lastFetchedMissionId = "";

  type ActivePlanStatus = "done" | "active" | "remaining";

  interface ActivePlanRun {
    semantic: RouteSemantic;
    status: ActivePlanStatus;
    points: Point[];
  }

  const remainingSemanticColor: Partial<Record<RouteSemantic, string>> = {
    coverage_edge: "#4ade80",
    coverage_infill: "#22c55e",
    transit_within_zone: "#f59e0b",
    transit_between_components: "#a78bfa",
    transit_inter_zone: "#38bdf8",
    dock_approach: "#fbbf24",
    recovery: "#94a3b8",
    unknown: "#94a3b8",
  };

  const doneSemanticColor: Partial<Record<RouteSemantic, string>> = {
    coverage_edge: "#166534",
    coverage_infill: "#15803d",
    transit_within_zone: "#78716c",
    transit_between_components: "#6d28d9",
    transit_inter_zone: "#0369a1",
    dock_approach: "#a16207",
    recovery: "#475569",
    unknown: "#475569",
  };

  const activeSemanticColor: Partial<Record<RouteSemantic, string>> = {
    coverage_edge: "#facc15",
    coverage_infill: "#fde047",
    transit_within_zone: "#fb923c",
    transit_between_components: "#e879f9",
    transit_inter_zone: "#22d3ee",
    dock_approach: "#facc15",
    recovery: "#cbd5e1",
    unknown: "#e2e8f0",
  };

  function isCoverageSemantic(semantic: RouteSemantic) {
    return semantic === "coverage_edge" || semantic === "coverage_infill";
  }

  function activePlanRuns(
    points: PlannerPreviewRoutePoint[],
    activeIndex: number,
  ): ActivePlanRun[] {
    if (points.length < 2) return [];

    const runs: ActivePlanRun[] = [];
    const statusForSegment = (segmentIndex: number): ActivePlanStatus => {
      if (activeIndex <= 0) return segmentIndex === 0 ? "active" : "remaining";
      if (segmentIndex < activeIndex - 1) return "done";
      if (segmentIndex === activeIndex - 1) return "active";
      return "remaining";
    };

    for (let index = 0; index < points.length - 1; index += 1) {
      const semantic: RouteSemantic =
        points[index + 1].semantic ?? points[index].semantic ?? "unknown";
      const status = statusForSegment(index);
      const a: Point = { x: points[index].p[0], y: points[index].p[1] };
      const b: Point = { x: points[index + 1].p[0], y: points[index + 1].p[1] };
      const lastRun = runs[runs.length - 1];
      if (
        lastRun &&
        lastRun.semantic === semantic &&
        lastRun.status === status
      ) {
        lastRun.points.push(b);
      } else {
        runs.push({ semantic, status, points: [a, b] });
      }
    }

    return runs;
  }

  function activeRunStroke(run: ActivePlanRun) {
    if (run.status === "done") {
      return doneSemanticColor[run.semantic] ?? "#475569";
    }
    if (run.status === "active") {
      return activeSemanticColor[run.semantic] ?? "#e2e8f0";
    }
    return remainingSemanticColor[run.semantic] ?? "#94a3b8";
  }

  function activeRunStrokeWidth(run: ActivePlanRun) {
    if (run.status === "active") return 1.8 / zoom;
    return isCoverageSemantic(run.semantic)
      ? run.status === "done"
        ? 1.15 / zoom
        : 1.3 / zoom
      : 0.95 / zoom;
  }

  function activeRunDasharray(run: ActivePlanRun) {
    if (run.status === "active") return undefined;
    if (isCoverageSemantic(run.semantic)) {
      return run.status === "done" ? undefined : `${2.5 / zoom} ${1.6 / zoom}`;
    }
    return `${3.2 / zoom} ${2.1 / zoom}`;
  }

  function activeRunOpacity(run: ActivePlanRun) {
    if (run.status === "active") return 0.98;
    if (run.status === "done")
      return isCoverageSemantic(run.semantic) ? 0.72 : 0.52;
    return isCoverageSemantic(run.semantic) ? 0.88 : 0.74;
  }

  async function fetchActivePlan() {
    if (!$telemetry.mission_id) {
      activePlanWaypoints = [];
      activePlanRoutePoints = [];
      activePlanWaypointIndex = -1;
      return;
    }

    try {
      const response: ActivePlanResponse = await getActivePlan();
      activePlanWaypoints = response.waypoints ?? [];
      activePlanRoutePoints = response.route?.points ?? [];
      activePlanWaypointIndex =
        response.activePointIndex ?? response.waypointIndex ?? -1;
    } catch {
      activePlanWaypoints = [];
      activePlanRoutePoints = [];
      activePlanWaypointIndex = -1;
    }
  }

  $: if (showRuntimeLayers && $telemetry.mission_id !== lastFetchedMissionId) {
    lastFetchedMissionId = $telemetry.mission_id;
    if ($telemetry.mission_id) {
      void fetchActivePlan();
    } else {
      activePlanWaypoints = [];
      activePlanRoutePoints = [];
      activePlanWaypointIndex = -1;
    }
  }

  const activePlanTimer = showRuntimeLayers
    ? setInterval(() => {
        void fetchActivePlan();
      }, 2000)
    : undefined;

  // N7.3: These are display-only representations derived from backend data — not
  // a second progress model. waypoint_index is owned by the backend; the slicing
  // here only splits the already-fetched route for rendering purposes.
  $: currentActivePlanIndex =
    $telemetry.waypoint_index >= 0
      ? $telemetry.waypoint_index
      : activePlanWaypointIndex;
  $: activePlanDriven =
    $telemetry.waypoint_index > 0
      ? activePlanWaypoints.slice(0, $telemetry.waypoint_index)
      : [];
  $: activePlanRemaining =
    $telemetry.waypoint_index >= 0
      ? activePlanWaypoints.slice($telemetry.waypoint_index)
      : activePlanWaypoints;
  $: activePlanSemanticRuns = activePlanRuns(
    activePlanRoutePoints,
    currentActivePlanIndex,
  );

  $: targetScreen = $telemetry.has_target
    ? worldToScreen(
        { x: $telemetry.target_x, y: $telemetry.target_y },
        currentScale,
        offsetX,
        offsetY,
      )
    : null;
  // -------------------------------------------------------------------------

  // N5.1/N5.2 — obstacles + active detour live overlay (only when showRuntimeLayers)
  let liveObstacles: ObstacleEntry[] = [];
  let liveDetour: DetourEntry = { active: false, waypoints: [] };

  function fetchLiveOverlay() {
    getLiveOverlay()
      .then((r: LiveOverlayResponse) => {
        liveObstacles = r.obstacles ?? [];
        liveDetour = r.detour ?? { active: false, waypoints: [] };
      })
      .catch(() => {
        // Keep stale data on error
      });
  }

  const liveOverlayTimer = showRuntimeLayers
    ? setInterval(fetchLiveOverlay, 2000)
    : undefined;
  onDestroy(() => {
    if (activePlanTimer !== undefined) clearInterval(activePlanTimer);
    if (liveOverlayTimer !== undefined) clearInterval(liveOverlayTimer);
  });
  if (showRuntimeLayers) void fetchActivePlan();
  if (showRuntimeLayers) fetchLiveOverlay();
  // -------------------------------------------------------------------------

  $: coordMode = detectCoordMode(
    $mapStore.map.perimeter,
    $telemetry.gps_lat,
    $telemetry.gps_lon,
  );

  $: robotScale = Number(zoom.toFixed(3));
  $: pointRadius = Number(clamp(3.1 * zoom, 2.1, 4.3).toFixed(2));
  $: selectionRadius = Number((pointRadius + 3.6).toFixed(2));
  $: dockHalfSize = Number(clamp(8 * zoom, 4.5, 8).toFixed(2));
  $: strokeWidth = Number(clamp(1.15 / zoom, 0.5, 1.55).toFixed(2));
  $: corridorCenter = centroid($mapStore.map.dockMeta?.corridor ?? []);

  function scaleBarMeters(scale: number): number {
    const raw = 80 / scale;
    for (const c of [0.5, 1, 2, 5, 10, 20, 50, 100]) if (c >= raw) return c;
    return 100;
  }
  $: scaleMeters = scaleBarMeters(currentScale);
  $: scaleBarPx = Number((scaleMeters * currentScale).toFixed(1));
  $: mapPointCount = allMapPoints().length;
  $: if (!hasAutoFit && mapPointCount > 1) {
    fitToContent();
    hasAutoFit = true;
  }

  onDestroy(() => {
    dragTarget = null;
    isPanning = false;
  });
</script>

<section class:embedded={!showHeader && !showViewportActions} class="panel">
  {#if showHeader}
    <header>
      <h2>Arbeitsraum</h2>
      <p>Lokales Raster in Meterkoordinaten.</p>
    </header>
  {/if}

  {#if showViewportActions}
    <div class="viewport-actions">
      <button type="button" on:click={fitToContent}>Auf Karte zoomen</button>
      <button type="button" class="secondary" on:click={resetView}
        >Ansicht zentrieren</button
      >
      <span class="meta">Zoom {(zoom * 100).toFixed(0)}%</span>
    </div>
  {/if}

  <div class="canvas-wrap" bind:clientWidth={width} bind:clientHeight={height}>
    {#if cursorWorld}
      <div class="cursor-coords">
        {#if coordMode === "wgs84"}
          lon {cursorWorld.x.toFixed(6)} &nbsp; lat {cursorWorld.y.toFixed(6)}
        {:else}
          x {cursorWorld.x.toFixed(2)} m &nbsp; y {cursorWorld.y.toFixed(2)} m
        {/if}
      </div>
    {/if}

    {#if showZoomControls}
      <div class="zoom-controls">
        <button type="button" title="Heranzoomen" on:click={zoomIn}>+</button>
        <button type="button" title="Herauszoomen" on:click={zoomOut}>−</button>
        <button type="button" title="Auf Inhalt zoomen" on:click={fitToContent}
          >◎</button
        >
      </div>
    {/if}
    <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_noninteractive_tabindex a11y_no_noninteractive_element_interactions -->
    <svg
      viewBox={`0 0 ${width} ${height}`}
      role="img"
      aria-label="Map canvas"
      tabindex={interactive ? 0 : -1}
      data-readonly={!interactive}
      on:click={handleCanvasClick}
      on:pointerdown={handlePointerDown}
      on:pointermove={handlePointerMove}
      on:pointerup={releasePointer}
      on:pointerleave={(e) => {
        releasePointer(e);
        cursorWorld = null;
      }}
      on:wheel|nonpassive={handleWheel}
    >
      <defs>
        <pattern
          id="grid"
          width={currentScale * grid}
          height={currentScale * grid}
          patternUnits="userSpaceOnUse"
          x={offsetX}
          y={offsetY}
        >
          <path
            d={`M ${currentScale * grid} 0 L 0 0 0 ${currentScale * grid}`}
            fill="none"
            stroke="rgba(30,58,95,0.45)"
            stroke-width="1"
          />
        </pattern>
      </defs>

      <rect x="0" y="0" {width} {height} fill="url(#grid)" />
      <line
        x1="0"
        y1={height / 2}
        x2={width}
        y2={height / 2}
        stroke="rgba(148,163,184,0.18)"
        stroke-width="1"
      />
      <line
        x1={width / 2}
        y1="0"
        x2={width / 2}
        y2={height}
        stroke="rgba(148,163,184,0.18)"
        stroke-width="1"
      />

      {#if $mapStore.map.perimeter.length === 2}
        <polyline
          points={path($mapStore.map.perimeter, currentScale, offsetX, offsetY)}
          fill="none"
          stroke="#7aa2ff"
          stroke-width={strokeWidth}
          stroke-dasharray="5 4"
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {:else if $mapStore.map.perimeter.length > 2}
        <polygon
          points={path($mapStore.map.perimeter, currentScale, offsetX, offsetY)}
          fill="rgba(122, 162, 255, 0.10)"
          stroke="#7aa2ff"
          stroke-width={strokeWidth}
          stroke-dasharray={$mapStore.selectedTool === "perimeter"
            ? "5 4"
            : "none"}
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {/if}

      {#each $mapStore.map.perimeter as point, index}
        {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
        <circle cx={p.x} cy={p.y} r={pointRadius} fill="#7aa2ff" />
        {#if isSelected({ kind: "perimeter", index })}
          <circle
            cx={p.x}
            cy={p.y}
            r={selectionRadius}
            class="selection-ring"
          />
        {/if}
      {/each}

      {#if $mapStore.map.dock.length === 2}
        <polyline
          points={path($mapStore.map.dock, currentScale, offsetX, offsetY)}
          fill="none"
          stroke="#f4b860"
          stroke-width={strokeWidth}
          stroke-dasharray="5 4"
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {:else if $mapStore.map.dock.length > 2}
        <polyline
          points={path($mapStore.map.dock, currentScale, offsetX, offsetY)}
          fill="none"
          stroke="#f4b860"
          stroke-width={strokeWidth}
          stroke-dasharray="5 4"
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {/if}

      {#each $mapStore.map.dock as point, index}
        {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
        <circle cx={p.x} cy={p.y} r={pointRadius} fill="#f4b860" />
        {#if index === $mapStore.map.dock.length - 1}
          <circle
            cx={p.x}
            cy={p.y}
            r={selectionRadius + 1}
            class="dock-entry-ring"
          />
          {@const dockLabel = labelAnchor(point, "Dock")}
          <g>
            <rect
              x={dockLabel.x - dockLabel.width / 2}
              y={dockLabel.y - 16}
              width={dockLabel.width}
              height="15"
              rx="7.5"
              fill="rgba(7, 13, 24, 0.72)"
              stroke="rgba(244, 184, 96, 0.45)"
              stroke-width="1"
            />
            <text
              x={dockLabel.x}
              y={dockLabel.y - 5}
              text-anchor="middle"
              class="label dock-label">Dock</text
            >
          </g>
        {:else}
          {@const dockPointLabel = labelAnchor(point, `${index + 1}`)}
          <g>
            <rect
              x={dockPointLabel.x - 12}
              y={dockPointLabel.y - 17}
              width="20"
              height="18"
              rx="9"
              fill="rgba(7, 13, 24, 0.85)"
              stroke="rgba(244, 184, 96, 0.6)"
              stroke-width="1.5"
            />
            <text
              x={dockPointLabel.x}
              y={dockPointLabel.y - 4}
              text-anchor="middle"
              class="label dock-point-label">{index + 1}</text
            >
          </g>
        {/if}
        {#if isSelected({ kind: "dock", index })}
          <circle
            cx={p.x}
            cy={p.y}
            r={selectionRadius + 2}
            class="selection-ring dock-selection"
          />
        {/if}
      {/each}

      {#if ($mapStore.map.dockMeta?.corridor.length ?? 0) === 2}
        <polyline
          points={path(
            $mapStore.map.dockMeta?.corridor ?? [],
            currentScale,
            offsetX,
            offsetY,
          )}
          fill="rgba(250, 204, 21, 0.08)"
          stroke="#fde68a"
          stroke-width={strokeWidth}
          stroke-dasharray="4 4"
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {:else if ($mapStore.map.dockMeta?.corridor.length ?? 0) > 2}
        <polygon
          points={path(
            $mapStore.map.dockMeta?.corridor ?? [],
            currentScale,
            offsetX,
            offsetY,
          )}
          fill="rgba(250, 204, 21, 0.12)"
          stroke="#fde68a"
          stroke-width={strokeWidth}
          stroke-dasharray={$mapStore.selectedTool === "dock-corridor"
            ? "4 4"
            : "none"}
          stroke-linecap="round"
          stroke-linejoin="round"
        />
      {/if}

      {#each $mapStore.map.dockMeta?.corridor ?? [] as point, index}
        {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
        <rect
          x={p.x - pointRadius * 0.85}
          y={p.y - pointRadius * 0.85}
          width={pointRadius * 1.7}
          height={pointRadius * 1.7}
          fill="#fde68a"
          rx="2.4"
        />
        {#if isSelected({ kind: "dock-corridor", index })}
          <circle
            cx={p.x}
            cy={p.y}
            r={selectionRadius + 1}
            class="selection-ring corridor-selection"
          />
        {/if}
      {/each}
      {#if corridorCenter}
        {@const center = worldToScreen(
          corridorCenter,
          currentScale,
          offsetX,
          offsetY,
        )}
        <g>
          <rect
            x={center.x - 58}
            y={center.y - 22}
            width="90"
            height="15"
            rx="7.5"
            fill="rgba(7, 13, 24, 0.72)"
            stroke="rgba(253, 230, 138, 0.35)"
            stroke-width="1"
          />
          <text
            x={center.x}
            y={center.y - 11}
            text-anchor="middle"
            class="label corridor-label">Korridor</text
          >
        </g>
      {/if}

      {#each $mapStore.map.exclusions as exclusion, exclusionIndex}
        {#if exclusion.length === 2}
          <polyline
            points={path(exclusion, currentScale, offsetX, offsetY)}
            fill="none"
            stroke={$mapStore.selectedExclusionIndex === exclusionIndex
              ? "#fca5a5"
              : "#fb7185"}
            stroke-width={$mapStore.selectedExclusionIndex === exclusionIndex
              ? strokeWidth * 1.6
              : strokeWidth}
            stroke-dasharray="4 4"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        {:else if exclusion.length > 2}
          <polygon
            points={path(exclusion, currentScale, offsetX, offsetY)}
            fill={$mapStore.selectedExclusionIndex === exclusionIndex
              ? "rgba(248, 113, 113, 0.16)"
              : "rgba(248, 113, 113, 0.08)"}
            stroke={$mapStore.selectedExclusionIndex === exclusionIndex
              ? "#fca5a5"
              : "#fb7185"}
            stroke-width={$mapStore.selectedExclusionIndex === exclusionIndex
              ? strokeWidth * 1.6
              : strokeWidth}
            stroke-dasharray="4 4"
            stroke-linecap="round"
            stroke-linejoin="round"
            role="button"
            tabindex="-1"
            aria-label={`NoGo ${exclusionIndex + 1} auswählen`}
            on:click|stopPropagation={() => selectExclusionArea(exclusionIndex)}
          />
        {/if}
        {#each exclusion as point, index}
          {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
          <circle
            cx={p.x}
            cy={p.y}
            r={pointRadius}
            fill={$mapStore.selectedExclusionIndex === exclusionIndex
              ? "#fca5a5"
              : "#fb7185"}
          />
          {#if isSelected({ kind: "exclusion", exclusionIndex, index })}
            <circle
              cx={p.x}
              cy={p.y}
              r={selectionRadius}
              class="selection-ring nogo-selection"
            />
          {/if}
        {/each}
        {@const exclusionCenter = centroid(exclusion)}
        {#if exclusionCenter}
          {@const center = worldToScreen(
            exclusionCenter,
            currentScale,
            offsetX,
            offsetY,
          )}
          <g
            role="button"
            tabindex="-1"
            aria-label={`NoGo ${exclusionIndex + 1} auswählen`}
            on:click|stopPropagation={() => selectExclusionArea(exclusionIndex)}
          >
            <rect
              x={center.x - 31}
              y={center.y - 22}
              width="62"
              height="15"
              rx="7.5"
              fill="rgba(7, 13, 24, 0.72)"
              stroke="rgba(248, 113, 113, 0.36)"
              stroke-width="1"
            />
            <text
              x={center.x}
              y={center.y - 11}
              text-anchor="middle"
              class="label exclusion-label">NoGo {exclusionIndex + 1}</text
            >
          </g>
        {/if}
      {/each}

      {#each $mapStore.map.zones as zone}
        {#if zone.polygon.length === 2}
          <polyline
            points={path(zone.polygon, currentScale, offsetX, offsetY)}
            fill="none"
            stroke="#67e8f9"
            stroke-width={strokeWidth}
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        {:else if zone.polygon.length > 2}
          <polygon
            points={path(zone.polygon, currentScale, offsetX, offsetY)}
            fill="rgba(103, 232, 249, 0.09)"
            stroke="#67e8f9"
            stroke-width={strokeWidth}
            stroke-linejoin="round"
          />
        {/if}
        {#each zone.polygon as point, index}
          {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
          <circle cx={p.x} cy={p.y} r={pointRadius} fill="#67e8f9" />
          {#if isSelected({ kind: "zone", zoneId: zone.id, index })}
            <circle
              cx={p.x}
              cy={p.y}
              r={selectionRadius}
              class="selection-ring zone-selection"
            />
          {/if}
        {/each}
        {@const zoneCenter = centroid(zone.polygon)}
        {#if zoneCenter}
          {@const center = worldToScreen(
            zoneCenter,
            currentScale,
            offsetX,
            offsetY,
          )}
          {@const labelWidth = zoneLabelBox(zone.name)}
          <g>
            <rect
              x={Math.max(10, Math.min(width - 10, center.x)) - labelWidth / 2}
              y={center.y - 22}
              width={labelWidth}
              height="15"
              rx="7.5"
              fill="rgba(7, 13, 24, 0.85)"
              stroke="rgba(103, 232, 249, 0.55)"
              stroke-width="1.2"
            />
            <text
              x={center.x}
              y={center.y - 11}
              text-anchor="middle"
              class="label zone-label">{zone.name}</text
            >
          </g>
        {/if}
      {/each}

      <!-- Scale bar -->
      <g
        transform={`translate(${width - scaleBarPx - 16}, 20)`}
        class="scale-bar"
      >
        <line x1="0" y1="0" x2={scaleBarPx} y2="0" stroke-linecap="round" />
        <line x1="0" y1="-3" x2="0" y2="5" />
        <line x1={scaleBarPx} y1="-3" x2={scaleBarPx} y2="5" />
        <text x={scaleBarPx / 2} y="14" text-anchor="middle"
          >{scaleMeters >= 1
            ? `${scaleMeters} m`
            : `${scaleMeters * 100} cm`}</text
        >
      </g>

      {#if showRobot}
        {#if showRuntimeLayers}
          <!-- N5.1: obstacle circles (red, semi-transparent) -->
          {#each liveObstacles as obs}
            {@const os = worldToScreen(
              { x: obs.x, y: obs.y },
              currentScale,
              offsetX,
              offsetY,
            )}
            {@const rPx = obs.r * currentScale}
            <circle
              cx={os.x}
              cy={os.y}
              r={Math.max(rPx, 3)}
              fill="rgba(239,68,68,0.18)"
              stroke="#ef4444"
              stroke-width={1.2 / zoom}
            />
          {/each}
          <!-- N5.2: active detour path (dashed orange) + re-entry marker -->
          {#if liveDetour.active && liveDetour.waypoints.length > 1}
            <polyline
              points={path(liveDetour.waypoints)}
              fill="none"
              stroke="#f97316"
              stroke-width={1.4 / zoom}
              stroke-dasharray={`${3.5 / zoom} ${2 / zoom}`}
              stroke-linecap="round"
              stroke-linejoin="round"
            />
          {/if}
          {#if liveDetour.active && liveDetour.reentryX !== undefined && liveDetour.reentryY !== undefined}
            {@const rs = worldToScreen(
              { x: liveDetour.reentryX, y: liveDetour.reentryY },
              currentScale,
              offsetX,
              offsetY,
            )}
            {@const cs = 4 / zoom}
            <circle
              cx={rs.x}
              cy={rs.y}
              r={cs}
              fill="#f97316"
              fill-opacity="0.7"
              stroke="#f97316"
              stroke-width={0.8 / zoom}
            />
          {/if}
          <!-- N4.2: active mission route with semantic + progress rendering -->
          {#if activePlanSemanticRuns.length > 0}
            {#each activePlanSemanticRuns as run}
              {#if run.points.length > 1}
                <polyline
                  points={path(run.points)}
                  fill="none"
                  stroke={activeRunStroke(run)}
                  stroke-opacity={activeRunOpacity(run)}
                  stroke-width={activeRunStrokeWidth(run)}
                  stroke-dasharray={activeRunDasharray(run)}
                  stroke-linecap="round"
                  stroke-linejoin="round"
                />
              {/if}
            {/each}
          {:else}
            {#if activePlanDriven.length > 1}
              <polyline
                points={path(activePlanDriven)}
                fill="none"
                stroke="#22c55e"
                stroke-width={1.4 / zoom}
                stroke-linecap="round"
                stroke-linejoin="round"
              />
            {/if}
            {#if activePlanRemaining.length > 1}
              <polyline
                points={path(activePlanRemaining)}
                fill="none"
                stroke="#4ade80"
                stroke-opacity="0.55"
                stroke-width={0.9 / zoom}
                stroke-dasharray={`${3 / zoom} ${2 / zoom}`}
                stroke-linecap="round"
                stroke-linejoin="round"
              />
            {/if}
          {/if}
          <!-- N4.3: current navigation target crosshair (orange) -->
          {#if targetScreen}
            {@const cs = 5 / zoom}
            <line
              x1={targetScreen.x - cs}
              y1={targetScreen.y}
              x2={targetScreen.x + cs}
              y2={targetScreen.y}
              stroke="#f97316"
              stroke-width={1.2 / zoom}
              stroke-linecap="round"
            />
            <line
              x1={targetScreen.x}
              y1={targetScreen.y - cs}
              x2={targetScreen.x}
              y2={targetScreen.y + cs}
              stroke="#f97316"
              stroke-width={1.2 / zoom}
              stroke-linecap="round"
            />
          {/if}
        {/if}
        <g
          class="robot-marker"
          transform={`translate(${robotScreen.x} ${robotScreen.y}) rotate(${robotHeadingDeg}) scale(${robotScale})`}
        >
          <circle r={robotOuterRadiusPx} class="robot-shell" />
          <circle r={robotInnerRadiusPx} class="robot-core" />
          <line
            x1="0"
            y1={-robotNoseStartPx}
            x2="0"
            y2={-robotNoseEndPx}
            class="robot-nose"
            stroke-width="2.5"
            stroke-linecap="round"
          />
          <polygon
            points={`0,${-robotArrowTipPx} ${-robotArrowHalfWidthPx},${-robotArrowSidePx} ${robotArrowHalfWidthPx},${-robotArrowSidePx}`}
            class="robot-arrow"
          />
        </g>
      {/if}
    </svg>
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1rem;
    border-radius: 0.9rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .panel.embedded {
    gap: 0;
    padding: 0;
    background: transparent;
    border: 0;
    height: 100%;
  }

  header {
    display: grid;
    gap: 0.25rem;
  }

  .viewport-actions {
    display: flex;
    gap: 0.7rem;
    flex-wrap: wrap;
    align-items: center;
  }

  .viewport-actions button {
    padding: 0.65rem 0.9rem;
    border: 1px solid #1e3a5f;
    border-radius: 0.45rem;
    background: #0a1020;
    color: #60a5fa;
    font-weight: 700;
    cursor: pointer;
  }

  .viewport-actions button.secondary {
    background: #0f1829;
    color: #94a3b8;
    border: 1px solid #1e3a5f;
  }

  .meta {
    color: #64748b;
    font-size: 0.8rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #64748b;
    font-size: 0.84rem;
  }

  .canvas-wrap {
    position: relative;
    overflow: hidden;
    height: 100%;
    background: #070d18;
  }

  .zoom-controls {
    position: absolute;
    left: 0.75rem;
    bottom: 0.75rem;
    z-index: 2;
    display: flex;
    gap: 0.35rem;
  }

  .zoom-controls button {
    width: 1.8rem;
    height: 1.8rem;
    border-radius: 0.45rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 24, 41, 0.96);
    color: #60a5fa;
    font-weight: 700;
    font-size: 0.9rem;
    cursor: pointer;
  }

  .zoom-controls button:hover {
    background: rgba(30, 58, 95, 0.96);
    color: #93c5fd;
  }

  svg {
    display: block;
    width: 100%;
    height: 100%;
    cursor: grab;
    touch-action: none;
  }

  svg[tabindex="0"] {
    outline: none;
  }

  svg[data-readonly="true"] {
    cursor: default;
  }

  .label {
    font-size: 9px;
    font-weight: 700;
    paint-order: stroke;
    stroke: rgba(7, 13, 24, 0.88);
    stroke-width: 1px;
    stroke-linejoin: round;
    user-select: none;
    pointer-events: none;
  }

  .zone-label {
    fill: #67e8f9;
  }

  .exclusion-label {
    fill: #fecdd3;
  }

  .dock-label {
    fill: #fdba74;
  }

  .dock-point-label {
    fill: #fdba74;
  }

  .corridor-label {
    fill: #fde047;
  }

  .selection-ring {
    fill: none;
    stroke: #f8fafc;
    stroke-width: 2;
    stroke-dasharray: 3 2;
    pointer-events: none;
  }

  .zone-selection {
    stroke: #67e8f9;
  }

  .nogo-selection {
    stroke: #fecdd3;
  }

  .dock-selection {
    stroke: #fbbf24;
  }

  .corridor-selection {
    stroke: #fde047;
  }

  .dock-entry-ring {
    fill: none;
    stroke: rgba(251, 191, 36, 0.85);
    stroke-width: 1.6;
    stroke-dasharray: 4 3;
    pointer-events: none;
  }

  .robot-marker {
    pointer-events: none;
  }

  .robot-shell {
    fill: rgba(15, 24, 41, 0.22);
    stroke: rgba(147, 197, 253, 0.9);
    stroke-width: 2;
  }

  .robot-core {
    fill: rgba(96, 165, 250, 0.16);
    stroke: rgba(191, 219, 254, 0.75);
    stroke-width: 1.5;
  }

  .robot-nose,
  .robot-arrow {
    stroke: rgba(96, 165, 250, 0.92);
    fill: rgba(96, 165, 250, 0.92);
  }

  .cursor-coords {
    position: absolute;
    right: 0.6rem;
    bottom: 0.6rem;
    z-index: 2;
    font-size: 10px;
    font-family: monospace;
    color: rgba(148, 163, 184, 0.75);
    background: rgba(7, 13, 24, 0.65);
    padding: 2px 7px;
    border-radius: 4px;
    pointer-events: none;
    white-space: nowrap;
  }

  .scale-bar {
    pointer-events: none;
  }
  .scale-bar line {
    stroke: rgba(148, 163, 184, 0.7);
    stroke-width: 1.5;
  }
  .scale-bar text {
    font-size: 10px;
    fill: rgba(148, 163, 184, 0.7);
  }
</style>
