<script lang="ts">
  import { createEventDispatcher } from "svelte";
  import type { Mission } from "../../stores/missions";
  import type { Zone, Point, RobotMap } from "../../stores/map";
  import {
    previewPlannerRoutes,
    type PlannerPreviewRequest,
    type PlannerPreviewRoute,
    type RouteSemantic,
    type RouteValidationError,
    type WaypointEntry,
  } from "../../api/rest";
  import { missionStore } from "../../stores/missions";
  import { sendCmd } from "../../api/websocket";

  export let zones: Zone[] = [];
  export let perimeter: Point[] = [];
  export let exclusions: Point[][] = [];
  export let dock: Point[] = [];
  export let mission: Mission | null = null;
  export let selectedZoneId: string | null = null;
  export let map: RobotMap | null = null;

  const dispatch = createEventDispatcher<{ selectzone: { zoneId: string } }>();

  let zoom = 1;
  let offsetX = 0;
  let offsetY = 0;
  let dragging = false;
  let dragStartX = 0;
  let dragStartY = 0;

  // Layer-Toggles
  let showPerimeter = true;
  let showZones = true;
  let showPaths = true;

  // Debug-Modus: zeigt Übergänge, Fehler-Segmente, Validator-Details
  // User-Modus (Standard): nur Coverage-Bahnen, kein technischer Ballast
  let debugMode = false;

  const width = 820;
  const height = 520;
  export const zoneColors = [
    "#a855f7",
    "#22d3ee",
    "#22c55e",
    "#f59e0b",
    "#f97316",
    "#38bdf8",
  ];

  type Bounds = { minX: number; minY: number; maxX: number; maxY: number };

  function boundsOf(pts: Point[]): Bounds {
    if (pts.length === 0) return { minX: 0, minY: 0, maxX: 10, maxY: 10 };
    return {
      minX: Math.min(...pts.map((p) => p.x)),
      minY: Math.min(...pts.map((p) => p.y)),
      maxX: Math.max(...pts.map((p) => p.x)),
      maxY: Math.max(...pts.map((p) => p.y)),
    };
  }

  function project(point: Point, bounds: Bounds) {
    const centerX = width / 2 + offsetX;
    const centerY = height / 2 + offsetY;
    return {
      x: centerX + (point.x - boundsMidX) * currentScale,
      y: centerY - (point.y - boundsMidY) * currentScale,
    };
  }

  function polyPoints(pts: Point[], bounds: Bounds): string {
    return pts
      .map((p) => {
        const { x, y } = project(p, bounds);
        return `${x.toFixed(2)},${y.toFixed(2)}`;
      })
      .join(" ");
  }

  function worldPoints(pts: Point[]): string {
    return pts.map((p) => `${p.x.toFixed(4)},${p.y.toFixed(4)}`).join(" ");
  }

  function toPathD(pts: Point[], bounds: Bounds): string {
    if (pts.length < 3) return "";
    const projected = pts.map((p) => project(p, bounds));
    return (
      "M " +
      projected.map((p) => `${p.x.toFixed(2)},${p.y.toFixed(2)}`).join(" L ") +
      " Z"
    );
  }

  /** N3.3: Group consecutive same-mowOn waypoints into polyline segments. */
  function groupWaypointRuns(
    wps: WaypointEntry[],
  ): Array<{ mowOn: boolean; pts: WaypointEntry[] }> {
    if (wps.length === 0) return [];
    const runs: Array<{ mowOn: boolean; pts: WaypointEntry[] }> = [];
    let current = { mowOn: wps[0].mowOn, pts: [wps[0]] };
    for (let i = 1; i < wps.length; i++) {
      if (wps[i].mowOn === current.mowOn) {
        current.pts.push(wps[i]);
      } else {
        runs.push(current);
        // Bridge: last point of prior run is first point of new run for continuity.
        current = { mowOn: wps[i].mowOn, pts: [wps[i - 1], wps[i]] };
      }
    }
    runs.push(current);
    return runs;
  }

  function toWorldPathD(pts: Point[]): string {
    if (pts.length < 3) return "";
    return (
      "M " +
      pts.map((p) => `${p.x.toFixed(4)},${p.y.toFixed(4)}`).join(" L ") +
      " Z"
    );
  }

  function polygonCenter(zone: Zone): Point {
    const total = zone.polygon.reduce(
      (acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }),
      { x: 0, y: 0 },
    );
    return {
      x: total.x / Math.max(1, zone.polygon.length),
      y: total.y / Math.max(1, zone.polygon.length),
    };
  }

  function labelWidth(text: string) {
    return Math.max(44, Math.min(120, text.length * 5.2 + 10));
  }

  function labelPosition(point: Point, bounds: Bounds, text: string) {
    const anchor = project(point, bounds);
    const w = labelWidth(text);
    return {
      x: Math.max(8, Math.min(width - 8, anchor.x)),
      y: Math.max(12, Math.min(height - 10, anchor.y)),
      width: w,
    };
  }

  // ── Zoom / Pan ────────────────────────────────────────────────────────────

  function startDrag(event: PointerEvent) {
    if (event.button !== 0) return;
    dragging = true;
    dragStartX = event.clientX - offsetX;
    dragStartY = event.clientY - offsetY;
    const svg = event.currentTarget as SVGSVGElement;
    svg.setPointerCapture(event.pointerId);
  }

  function duringDrag(event: PointerEvent) {
    if (!dragging) return;
    offsetX = event.clientX - dragStartX;
    offsetY = event.clientY - dragStartY;
  }

  function endDrag(event: PointerEvent) {
    if (!dragging) return;
    dragging = false;
    try {
      (event.currentTarget as SVGSVGElement).releasePointerCapture(
        event.pointerId,
      );
    } catch {
      /* ignore */
    }
  }

  function onWheel(event: WheelEvent) {
    event.preventDefault();
    const svg = event.currentTarget as SVGSVGElement;
    const rect = svg.getBoundingClientRect();
    const svgX = (event.clientX - rect.left) * (width / rect.width);
    const svgY = (event.clientY - rect.top) * (height / rect.height);

    const oldScale = currentScale;

    const factor = event.deltaY < 0 ? 1.1 : 0.9;
    const newZoom = Math.max(0.3, Math.min(12, zoom * factor));
    const newScale = baseScale * newZoom;

    const worldX = (svgX - width / 2 - offsetX) / oldScale;
    const worldY = (svgY - height / 2 - offsetY) / oldScale;
    offsetX = svgX - width / 2 - worldX * newScale;
    offsetY = svgY - height / 2 - worldY * newScale;
    zoom = newZoom;
  }

  function zoomIn() {
    zoom = Math.min(12, zoom * 1.2);
  }
  function zoomOut() {
    zoom = Math.max(0.3, zoom * 0.8);
  }
  function resetView() {
    zoom = 1;
    offsetX = 0;
    offsetY = 0;
  }

  function jumpToPoint(pt: Point) {
    zoom = 4;
    const scale = baseScale * zoom;
    offsetX = -(pt.x - boundsMidX) * scale;
    offsetY = (pt.y - boundsMidY) * scale;
  }

  function niceGridMeters(scalePxPerMeter: number) {
    const targetPx = 34;
    const rawMeters = targetPx / Math.max(scalePxPerMeter, 0.0001);
    for (const candidate of [0.25, 0.5, 1, 2, 5, 10, 20, 50, 100]) {
      if (candidate >= rawMeters) return candidate;
    }
    return 100;
  }

  // ── Planner ───────────────────────────────────────────────────────────────

  $: missionZones = mission
    ? mission.zoneIds
        .map((id) => zones.find((z) => z.id === id))
        .filter((z): z is Zone => Boolean(z))
    : [];

  $: viewPts =
    perimeter.length >= 3 ? perimeter : missionZones.flatMap((z) => z.polygon);
  $: bounds = boundsOf(viewPts);
  $: boundsMidX = (bounds.minX + bounds.maxX) / 2;
  $: boundsMidY = (bounds.minY + bounds.maxY) / 2;
  $: boundsSpanX = Math.max(1, bounds.maxX - bounds.minX);
  $: boundsSpanY = Math.max(1, bounds.maxY - bounds.minY);
  $: baseScale = Math.min(
    (width - 120) / boundsSpanX,
    (height - 120) / boundsSpanY,
  );
  $: currentScale = baseScale * zoom;
  $: worldOriginX = width / 2 + offsetX - boundsMidX * currentScale;
  $: worldOriginY = height / 2 + offsetY + boundsMidY * currentScale;
  $: worldTransform = `translate(${worldOriginX.toFixed(2)} ${worldOriginY.toFixed(2)}) scale(${currentScale.toFixed(6)} ${(-currentScale).toFixed(6)})`;
  $: gridMeters = niceGridMeters(currentScale);
  $: gridPx = Math.max(14, Number((gridMeters * currentScale).toFixed(2)));
  $: worldZeroScreenX = worldOriginX;
  $: worldZeroScreenY = worldOriginY;
  $: gridOffsetX = ((worldZeroScreenX % gridPx) + gridPx) % gridPx;
  $: gridOffsetY = ((worldZeroScreenY % gridPx) + gridPx) % gridPx;

  let plannerRoutes: PlannerPreviewRoute[] = [];
  let plannerLoading = false;
  let plannerError = "";
  let plannerRequestId = 0;
  let previewInputSignature = "";
  let debugExpanded = true;

  // User-Ansicht: nur Coverage-Bahnen sichtbar, ruhige Grüntöne
  const coverageColor: Partial<Record<RouteSemantic, string>> = {
    coverage_edge: "#34d399", // emerald — Randmähen
    coverage_infill: "#4ade80", // green   — Innenbahnen
  };

  // Debug-Ansicht: alle Semantiken mit technischen Farben
  const semanticColor: Record<RouteSemantic, string> = {
    coverage_edge: "#34d399", // emerald
    coverage_infill: "#4ade80", // green
    transit_within_zone: "#f59e0b", // amber
    transit_between_components: "#a78bfa", // violet
    transit_inter_zone: "#38bdf8", // sky
    dock_approach: "#fbbf24", // yellow
    recovery: "#64748b", // slate
    unknown: "#475569", // muted
  };

  // Nur Coverage-Runs für die User-Ansicht
  $: coverageRuns = semanticRuns.filter(
    (r) => r.semantic === "coverage_edge" || r.semantic === "coverage_infill",
  );

  interface SemanticRun {
    semantic: RouteSemantic;
    points: Point[];
  }

  function routeSemanticRuns(
    route: PlannerPreviewRoute["route"] | undefined,
  ): SemanticRun[] {
    const pts = route?.points;
    if (!pts || pts.length < 2) return [];
    const runs: SemanticRun[] = [];
    let current: SemanticRun = {
      semantic: pts[0].semantic ?? "unknown",
      points: [{ x: pts[0].p[0], y: pts[0].p[1] }],
    };
    for (let i = 1; i < pts.length; i++) {
      const sem: RouteSemantic = pts[i].semantic ?? "unknown";
      current.points.push({ x: pts[i].p[0], y: pts[i].p[1] });
      if (sem !== current.semantic || i === pts.length - 1) {
        runs.push(current);
        current = {
          semantic: sem,
          points: [{ x: pts[i].p[0], y: pts[i].p[1] }],
        };
      }
    }
    return runs;
  }

  function routePoints(
    route: PlannerPreviewRoute["route"] | undefined,
  ): Point[] {
    return (
      route?.points.map((entry) => ({ x: entry.p[0], y: entry.p[1] })) ?? []
    );
  }

  function plannerMapSnapshot(source: RobotMap): PlannerPreviewRequest["map"] {
    return {
      perimeter: source.perimeter.map((point) => [point.x, point.y]),
      dock: source.dock.map((point) => [point.x, point.y]),
      exclusions: source.exclusions.map((exclusion) =>
        exclusion.map((point) => [point.x, point.y]),
      ),
      exclusionMeta: source.exclusionMeta?.map((entry) => ({
        type: entry.type,
        clearance: entry.clearance,
        costScale: entry.costScale,
      })),
      zones: source.zones.map((zone) => ({
        id: zone.id,
        order: zone.order,
        name: zone.name,
        polygon: zone.polygon.map((point) => [point.x, point.y]),
      })),
      planner: source.planner
        ? {
            defaultClearance: source.planner.defaultClearance,
            perimeterSoftMargin: source.planner.perimeterSoftMargin,
            perimeterHardMargin: source.planner.perimeterHardMargin,
            obstacleInflation: source.planner.obstacleInflation,
            softNoGoCostScale: source.planner.softNoGoCostScale,
            replanPeriodMs: source.planner.replanPeriodMs,
            gridCellSize: source.planner.gridCellSize,
          }
        : undefined,
      dockMeta: source.dockMeta
        ? {
            approachMode: source.dockMeta.approachMode,
            corridor: source.dockMeta.corridor.map((point) => [
              point.x,
              point.y,
            ]),
            finalAlignHeadingDeg: source.dockMeta.finalAlignHeadingDeg,
            slowZoneRadius: source.dockMeta.slowZoneRadius,
          }
        : undefined,
      captureMeta: source.captureMeta,
    } as unknown as PlannerPreviewRequest["map"];
  }

  function plannerMissionSnapshot(
    source: Mission,
  ): PlannerPreviewRequest["mission"] {
    return {
      id: source.id,
      name: source.name,
      zoneIds: [...source.zoneIds],
      overrides: source.overrides,
      schedule: source.schedule,
    };
  }

  $: previewPayload =
    map && mission && missionZones.length > 0
      ? {
          map: plannerMapSnapshot(map),
          mission: plannerMissionSnapshot(mission),
        }
      : null;

  $: nextPreviewInputSignature = previewPayload
    ? JSON.stringify(previewPayload)
    : "";

  $: if (nextPreviewInputSignature !== previewInputSignature) {
    previewInputSignature = nextPreviewInputSignature;
    plannerRequestId += 1;
    plannerRoutes = [];
    plannerLoading = false;
    plannerError = "";
  }

  async function refreshPlannerPreview() {
    if (!previewPayload) {
      plannerRoutes = [];
      plannerLoading = false;
      plannerError = "";
      return;
    }

    const requestId = ++plannerRequestId;
    plannerLoading = true;
    plannerError = "";
    try {
      const response = await previewPlannerRoutes(previewPayload);
      if (requestId !== plannerRequestId) return;
      plannerRoutes = response.routes ?? [];
      if (mission) {
        const firstRoute = response.routes[0];
        if (firstRoute?.planRef && firstRoute.valid) {
          missionStore.setMissionPlanRef(mission.id, {
            id: firstRoute.planRef.id,
            revision: firstRoute.planRef.revision,
            generatedAtMs: firstRoute.planRef.generatedAtMs,
          });
        } else {
          missionStore.setMissionPlanRef(mission.id, undefined);
        }
      }
      const hasRouteError = response.routes.some((entry) => !entry.ok);
      const hasValidationError = response.routes.some(
        (entry) => entry.valid === false,
      );
      if (hasRouteError) {
        plannerError = "Bahnplanung fehlgeschlagen";
      } else if (hasValidationError) {
        plannerError = "route_invalid";
      } else {
        plannerError = "";
      }
    } catch (err) {
      if (requestId !== plannerRequestId) return;
      plannerRoutes = [];
      plannerError = err instanceof Error ? err.message : "Verbindungsfehler";
    } finally {
      if (requestId === plannerRequestId) plannerLoading = false;
    }
  }

  $: if (!map || !mission || missionZones.length === 0) {
    plannerRoutes = [];
    plannerLoading = false;
    plannerError = "";
  }

  $: previewEntry = plannerRoutes.find((entry) => entry.ok && entry.route);
  $: previewRoute = routePoints(previewEntry?.route);
  $: hasPreviewRoute = previewRoute.length >= 2;
  $: semanticRuns = routeSemanticRuns(previewEntry?.route);
  $: validationErrors = (previewEntry?.validationErrors ??
    []) as RouteValidationError[];
  $: previewDebug = previewEntry?.debug;
  $: debugSemanticCounts = Object.entries(previewDebug?.semanticCounts ?? {});

  // N3.3: flat waypoint sequence from server (mowOn=false = transit between zones).
  $: previewWaypoints = (previewEntry?.waypoints ?? []) as WaypointEntry[];
  $: waypointRuns = groupWaypointRuns(previewWaypoints);

  // Nur die ersten 3 Fehlerpunkte — kein roter Teppich
  $: errorMarkers = validationErrors
    .slice(0, 3)
    .filter(
      (e) => e.pointIndex >= 0 && previewEntry?.route?.points[e.pointIndex],
    )
    .map((e) => {
      const pt = previewEntry!.route!.points[e.pointIndex];
      return {
        point: { x: pt.p[0], y: pt.p[1] },
        message: e.message,
        zoneId: e.zoneId,
      };
    });

  // Fehlersegmente: zusammenhängende Runs von invalid-Indizes → rote Linie nur dort
  $: invalidSegments = (() => {
    if (validationErrors.length === 0) return [];
    const indices = new Set(validationErrors.map((e) => e.pointIndex));
    const pts = previewEntry?.route?.points ?? [];
    const segs: Point[][] = [];
    let seg: Point[] = [];
    for (let i = 0; i < pts.length - 1; i++) {
      if (indices.has(i) || indices.has(i + 1)) {
        if (seg.length === 0) seg.push({ x: pts[i].p[0], y: pts[i].p[1] });
        seg.push({ x: pts[i + 1].p[0], y: pts[i + 1].p[1] });
      } else {
        if (seg.length >= 2) segs.push(seg);
        seg = [];
      }
    }
    if (seg.length >= 2) segs.push(seg);
    return segs;
  })();

  // Fehlerzusammenfassung für die UI
  $: firstError = validationErrors[0];
  $: firstErrorZone = firstError
    ? missionZones.find((z) => z.id === firstError.zoneId)
    : null;
  $: errorSummary = firstError
    ? `Übergang nicht planbar${firstErrorZone ? ` in Zone „${firstErrorZone.name}"` : ""}`
    : plannerError && plannerError !== "route_invalid"
      ? plannerError
      : "";
  $: errorCount = validationErrors.length;
  $: emptyMessage = !mission
    ? "Keine Mission ausgewählt. Ohne Missions- oder Kartendaten wirkt Zoom/Pan hier nahezu unsichtbar."
    : missionZones.length === 0
      ? "Diese Mission enthält noch keine Zonen. Die Vorschau bleibt leer, bis Kartendaten und Mission geladen sind."
      : "Keine Vorschau geladen.";
</script>

<div class="preview-root">
  <!-- Layer-Toggles oben links -->
  <div class="layer-bar">
    <button
      class="layer-btn"
      class:off={!showPerimeter}
      type="button"
      on:click={() => (showPerimeter = !showPerimeter)}>Perimeter</button
    >
    <button
      class="layer-btn"
      class:off={!showZones}
      type="button"
      on:click={() => (showZones = !showZones)}>Zonen</button
    >
    <button
      class="layer-btn"
      class:off={!showPaths}
      type="button"
      on:click={() => (showPaths = !showPaths)}>Bahnen</button
    >
    <button
      class="layer-btn dbg"
      class:active={debugMode}
      type="button"
      on:click={() => (debugMode = !debugMode)}>Debug</button
    >
  </div>

  <!-- Zonen-Legende oben links (unterhalb Layer-Bar) -->
  {#if missionZones.length > 0}
    <div class="ms-legend">
      {#each missionZones as zone, i}
        <button
          type="button"
          class="ms-leg-item"
          class:sel={selectedZoneId === zone.id}
          on:click={() => dispatch("selectzone", { zoneId: zone.id })}
        >
          <span
            class="ms-leg-dot"
            style="background:{zoneColors[i % zoneColors.length]}"
          ></span>
          <span class="ms-leg-name">{zone.name}</span>
          <span class="ms-leg-angle"
            >{mission?.overrides?.[zone.id]?.angle ?? 0}°</span
          >
        </button>
      {/each}
    </div>
  {/if}

  <!-- Statusbereich oben rechts -->
  <div class="status-area">
    {#if plannerLoading}
      <div class="status-pill">Wird berechnet…</div>
    {:else if plannerError === "route_invalid" && errorCount > 0}
      <div class="status-pill error">
        <span class="err-icon">⚠</span>
        <span>{errorCount} Fehler · {errorSummary}</span>
        {#if errorMarkers.length > 0}
          <button
            class="jump-btn"
            type="button"
            on:click={() => jumpToPoint(errorMarkers[0].point)}
          >
            Zum Problem ↗
          </button>
        {/if}
      </div>
    {:else if plannerError}
      <div class="status-pill error">{plannerError}</div>
    {:else if hasPreviewRoute}
      <div class="status-pill ok">Route bereit</div>
    {/if}
  </div>

  {#if debugMode && previewDebug}
    <aside class="debug-panel" class:collapsed={!debugExpanded}>
      <button
        type="button"
        class="debug-panel-toggle"
        on:click={() => (debugExpanded = !debugExpanded)}
        >{debugExpanded ? "Debug einklappen" : "Debug aufklappen"}</button
      >

      {#if debugExpanded}
        <div class="debug-block">
          <strong>Planner</strong>
          <div>Punkte: {previewDebug.pointCount}</div>
          <div>Route aktiv: {previewDebug.active ? "ja" : "nein"}</div>
          <div>Route valide: {previewDebug.valid ? "ja" : "nein"}</div>
          {#if previewDebug.invalidReason}
            <div class="debug-reason">{previewDebug.invalidReason}</div>
          {/if}
        </div>

        <div class="debug-block">
          <strong>Zonenfolge</strong>
          <div class="debug-chip-row">
            {#each previewDebug.zoneOrder as zoneId}
              <span class="debug-chip">{zoneId}</span>
            {/each}
          </div>
        </div>

        <div class="debug-block">
          <strong>Komponentenfolge</strong>
          <div class="debug-list">
            {#each previewDebug.componentOrder as entry}
              <div>{entry.componentId} · {entry.firstSemantic}</div>
            {/each}
          </div>
        </div>

        <div class="debug-block">
          <strong>Semantiken</strong>
          <div class="debug-list">
            {#each debugSemanticCounts as [semantic, count]}
              <div>{semantic}: {count}</div>
            {/each}
          </div>
        </div>

        {#if validationErrors.length > 0}
          <div class="debug-block">
            <strong>Validator</strong>
            <div class="debug-list">
              {#each validationErrors.slice(0, 6) as error}
                <div>{error.message}</div>
              {/each}
            </div>
          </div>
        {/if}
      {/if}
    </aside>
  {/if}

  {#if missionZones.length === 0}
    <div class="empty">
      {emptyMessage}
    </div>
  {/if}

  <svg
    viewBox="0 0 {width} {height}"
    preserveAspectRatio="xMidYMid meet"
    class="canvas-svg"
    style="cursor:{dragging ? 'grabbing' : 'grab'}"
    role="img"
    aria-label="Bahnvorschau der aktuellen Mission"
    on:pointerdown={startDrag}
    on:pointermove={duringDrag}
    on:pointerup={endDrag}
    on:pointerleave={endDrag}
    on:wheel|nonpassive={onWheel}
  >
    <defs>
      <pattern
        id="pp-grid"
        width={gridPx}
        height={gridPx}
        patternUnits="userSpaceOnUse"
        x={gridOffsetX}
        y={gridOffsetY}
      >
        <path
          d={`M${gridPx} 0L0 0 0 ${gridPx}`}
          fill="none"
          stroke="#1e3a5f"
          stroke-width="0.5"
        />
      </pattern>
      {#if perimeter.length >= 3}
        <clipPath id="perimeter-clip" clipPathUnits="userSpaceOnUse">
          <polygon points={worldPoints(perimeter)} />
        </clipPath>
        <clipPath
          id="mow-clip"
          clip-rule="evenodd"
          clipPathUnits="userSpaceOnUse"
        >
          <path d={toWorldPathD(perimeter)} />
          {#each exclusions as exc}
            {#if exc.length >= 3}
              <path d={toWorldPathD(exc)} />
            {/if}
          {/each}
        </clipPath>
      {/if}
    </defs>

    <rect x="-9999" y="-9999" width="19999" height="19999" fill="#070d18" />
    <rect
      x="-9999"
      y="-9999"
      width="19999"
      height="19999"
      fill="url(#pp-grid)"
      opacity="0.6"
    />

    <g transform={worldTransform}>
      <!-- 1. Perimeter — stärkste Grundstruktur, immer sichtbar -->
      {#if showPerimeter && perimeter.length >= 3}
        <polygon
          points={worldPoints(perimeter)}
          fill="rgba(59, 130, 246, 0.07)"
          stroke="#60a5fa"
          stroke-width="1.6"
          stroke-linejoin="round"
          vector-effect="non-scaling-stroke"
        />
      {/if}

      <!-- 2. No-Go-Flächen — subtil, kein dominanter Stroke in User-Ansicht -->
      {#if showPerimeter && exclusions.length > 0}
        {#each exclusions as exclusion}
          {#if exclusion.length >= 3}
            <polygon
              points={worldPoints(exclusion)}
              fill="rgba(220, 38, 38, 0.09)"
              stroke={debugMode ? "#f87171" : "rgba(248,113,113,0.25)"}
              stroke-width={debugMode ? 0.8 : 0.5}
              stroke-dasharray={debugMode ? "4 3" : "3 4"}
              stroke-linejoin="round"
              vector-effect="non-scaling-stroke"
            />
          {/if}
        {/each}
      {/if}

      <!-- 3. Docking-Pfad -->
      {#if dock.length >= 2}
        <polyline
          points={worldPoints(dock)}
          fill="none"
          stroke="#fbbf24"
          stroke-width="0.9"
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-dasharray="5 4"
          vector-effect="non-scaling-stroke"
        />
      {/if}

      <!-- 4. Zonen — geklippt am Perimeter, halbtransparent -->
      {#if showZones}
        <g
          clip-path={perimeter.length >= 3 ? "url(#perimeter-clip)" : undefined}
        >
          {#each missionZones as zone, i}
            {@const color = zoneColors[i % zoneColors.length]}
            <polygon
              points={worldPoints(zone.polygon)}
              fill={selectedZoneId === zone.id ? `${color}28` : `${color}12`}
              stroke={color}
              stroke-width={selectedZoneId === zone.id ? 1.2 : 0.7}
              stroke-dasharray="5 3"
              stroke-linejoin="round"
              vector-effect="non-scaling-stroke"
            />
          {/each}
        </g>
      {/if}

      <!-- 5a. User-Ansicht: nur Coverage-Bahnen (Rand + Infill), kein Transit-Ballast -->
      {#if showPaths && hasPreviewRoute && !debugMode}
        <g clip-path={perimeter.length >= 3 ? "url(#mow-clip)" : undefined}>
          {#each coverageRuns as run}
            {#if run.points.length >= 2}
              <polyline
                points={worldPoints(run.points)}
                fill="none"
                stroke={coverageColor[run.semantic] ?? "#4ade80"}
                stroke-opacity="0.9"
                stroke-width="0.7"
                stroke-linecap="round"
                stroke-linejoin="round"
                vector-effect="non-scaling-stroke"
              />
            {/if}
          {/each}
        </g>
      {/if}

      <!-- 5b. Debug-Ansicht: alle semantischen Runs mit technischen Farben -->
      {#if showPaths && hasPreviewRoute && debugMode}
        <g clip-path={perimeter.length >= 3 ? "url(#mow-clip)" : undefined}>
          {#each semanticRuns as run}
            {#if run.points.length >= 2}
              <polyline
                points={worldPoints(run.points)}
                fill="none"
                stroke={semanticColor[run.semantic]}
                stroke-opacity="0.85"
                stroke-width="0.8"
                stroke-linecap="round"
                stroke-linejoin="round"
                vector-effect="non-scaling-stroke"
              />
            {/if}
          {/each}
        </g>
      {/if}

      <!-- 5c. N3.3 Waypoints layer: flat execution sequence in debug mode.
           Green = mowing (mowOn=true), slate-dashed = inter-zone transit. -->
      {#if showPaths && waypointRuns.length > 0 && debugMode}
        <g clip-path={perimeter.length >= 3 ? "url(#mow-clip)" : undefined}>
          {#each waypointRuns as run}
            {#if run.pts.length >= 2}
              <polyline
                points={worldPoints(run.pts)}
                fill="none"
                stroke={run.mowOn ? "#22c55e" : "#94a3b8"}
                stroke-opacity={run.mowOn ? "0.75" : "0.55"}
                stroke-width={run.mowOn ? "0.9" : "0.7"}
                stroke-dasharray={run.mowOn ? undefined : "3 3"}
                stroke-linecap="round"
                stroke-linejoin="round"
                vector-effect="non-scaling-stroke"
              />
            {/if}
          {/each}
        </g>
      {/if}

      <!-- 6. Debug-Layer: Fehler-Segmente — nur in Debug-Ansicht -->
      {#if debugMode}
        {#each invalidSegments as seg}
          <polyline
            points={worldPoints(seg)}
            fill="none"
            stroke="#ef4444"
            stroke-width="1.8"
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-opacity="0.9"
            vector-effect="non-scaling-stroke"
          />
        {/each}
      {/if}
    </g>

    {#if dock.length >= 2}
      {@const dockEnd = project(dock[dock.length - 1], bounds)}
      <circle cx={dockEnd.x} cy={dockEnd.y} r="3" fill="#fbbf24" />
    {/if}

    {#if showZones}
      {#each missionZones as zone, i}
        {@const color = zoneColors[i % zoneColors.length]}
        {@const label = `${zone.name} · ${mission?.overrides?.[zone.id]?.angle ?? 0}°`}
        {@const labelPos = labelPosition(polygonCenter(zone), bounds, label)}

        <g
          role="button"
          tabindex="0"
          aria-label={`Zone ${zone.name} auswählen`}
          on:click|stopPropagation={() =>
            dispatch("selectzone", { zoneId: zone.id })}
          on:keydown={(e) => {
            if (e.key === "Enter" || e.key === " ") {
              e.preventDefault();
              dispatch("selectzone", { zoneId: zone.id });
            }
          }}
        >
          <rect
            x={labelPos.x - labelPos.width / 2}
            y={labelPos.y - 13}
            width={labelPos.width}
            height="14"
            rx="7"
            fill="rgba(7, 13, 24, 0.75)"
            stroke={selectedZoneId === zone.id
              ? color
              : "rgba(148, 163, 184, 0.18)"}
            stroke-width="0.8"
          />
          <text
            x={labelPos.x}
            y={labelPos.y - 4}
            text-anchor="middle"
            fill="#cbd5e1"
            font-size="8"
            font-weight="600">{label}</text
          >
        </g>
      {/each}
    {/if}

    <!-- Marker nur am ersten Fehlerpunkt -->
    {#if debugMode && errorMarkers.length > 0}
      {@const proj = project(errorMarkers[0].point, bounds)}
      <line
        x1={proj.x - 5}
        y1={proj.y - 5}
        x2={proj.x + 5}
        y2={proj.y + 5}
        stroke="#ef4444"
        stroke-width="1.5"
      />
      <line
        x1={proj.x + 5}
        y1={proj.y - 5}
        x2={proj.x - 5}
        y2={proj.y + 5}
        stroke="#ef4444"
        stroke-width="1.5"
      />
      <circle
        cx={proj.x}
        cy={proj.y}
        r="7"
        fill="none"
        stroke="#ef4444"
        stroke-width="1"
        stroke-opacity="0.5"
      />
    {/if}
  </svg>

  <!-- Zoom-Buttons -->
  <div class="ms-zoom">
    <button type="button" class="ms-zoom-btn" on:click={zoomIn}>+</button>
    <button type="button" class="ms-zoom-btn" on:click={zoomOut}>−</button>
    <button type="button" class="ms-zoom-btn" on:click={resetView}>⊙</button>
    <span class="ms-zoom-readout">{Math.round(zoom * 100)}%</span>
  </div>

  <!-- Vorschau-Button -->
  {#if missionZones.length > 0}
    <button
      type="button"
      class="preview-btn"
      disabled={plannerLoading}
      on:click={() => void refreshPlannerPreview()}
    >
      {plannerLoading ? "Wird berechnet…" : "Vorschau berechnen"}
    </button>
    <!-- N6.2: start exactly the previewed plan -->
    {#if hasPreviewRoute && !plannerLoading && plannerError === ""}
      <button type="button" class="mow-btn" on:click={() => sendCmd("start")}>
        Jetzt mähen ▶
      </button>
    {/if}
  {/if}
</div>

<style>
  .preview-root {
    position: relative;
    width: 100%;
    height: 100%;
    background: #070d18;
    overflow: hidden;
  }

  .canvas-svg {
    display: block;
    width: 100%;
    height: 100%;
    touch-action: none;
    user-select: none;
  }

  /* ── Layer-Bar ─────────────────────────────────────────────── */
  .layer-bar {
    position: absolute;
    top: 10px;
    left: 10px;
    z-index: 3;
    display: flex;
    gap: 4px;
  }

  .layer-btn {
    background: rgba(15, 24, 41, 0.88);
    border: 1px solid #1e3a5f;
    color: #93c5fd;
    border-radius: 5px;
    padding: 3px 8px;
    font-size: 10px;
    cursor: pointer;
    transition: all 0.12s;
  }

  .layer-btn:hover {
    border-color: #3b82f6;
  }
  .layer-btn.off {
    opacity: 0.38;
    color: #475569;
  }
  .layer-btn.dbg {
    color: #94a3b8;
    border-color: rgba(148, 163, 184, 0.25);
  }
  .layer-btn.dbg:hover {
    border-color: #64748b;
  }
  .layer-btn.dbg.active {
    color: #f59e0b;
    border-color: rgba(245, 158, 11, 0.5);
    background: rgba(245, 158, 11, 0.08);
  }

  /* ── Zonen-Legende ─────────────────────────────────────────── */
  .ms-legend {
    position: absolute;
    top: 38px;
    left: 10px;
    z-index: 2;
    display: flex;
    flex-direction: column;
    gap: 4px;
    pointer-events: auto;
  }

  .ms-leg-item {
    display: flex;
    align-items: center;
    gap: 6px;
    background: rgba(15, 24, 41, 0.82);
    border: 1px solid #1e3a5f;
    border-radius: 5px;
    padding: 4px 8px;
    font-size: 10px;
    cursor: pointer;
    transition: border-color 0.15s;
    color: #e2e8f0;
  }

  .ms-leg-item:hover {
    border-color: #2563eb;
  }
  .ms-leg-item.sel {
    border-color: #2563eb;
    background: rgba(12, 26, 58, 0.92);
  }

  .ms-leg-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .ms-leg-name {
    color: #e2e8f0;
  }
  .ms-leg-angle {
    color: #475569;
    font-size: 9px;
    font-family: monospace;
  }

  /* ── Status-Bereich ────────────────────────────────────────── */
  .status-area {
    position: absolute;
    top: 10px;
    right: 10px;
    z-index: 3;
    max-width: 340px;
  }

  .debug-panel {
    position: absolute;
    top: 74px;
    right: 10px;
    z-index: 3;
    width: min(320px, calc(100% - 20px));
    padding: 10px;
    border-radius: 8px;
    background: rgba(15, 24, 41, 0.9);
    border: 1px solid rgba(148, 163, 184, 0.2);
    color: #cbd5e1;
    display: grid;
    gap: 10px;
  }

  .debug-panel.collapsed {
    width: auto;
    gap: 0;
  }

  .debug-panel-toggle {
    justify-self: start;
    padding: 4px 8px;
    border-radius: 5px;
    border: 1px solid rgba(148, 163, 184, 0.25);
    background: rgba(30, 41, 59, 0.7);
    color: #cbd5e1;
    font-size: 11px;
    cursor: pointer;
  }

  .debug-block {
    display: grid;
    gap: 4px;
    font-size: 11px;
  }

  .debug-chip-row {
    display: flex;
    gap: 4px;
    flex-wrap: wrap;
  }

  .debug-chip {
    padding: 2px 6px;
    border-radius: 999px;
    background: rgba(59, 130, 246, 0.16);
    color: #bfdbfe;
  }

  .debug-list {
    display: grid;
    gap: 3px;
    color: #94a3b8;
  }

  .debug-reason {
    color: #fca5a5;
  }

  .status-pill {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 5px 10px;
    border-radius: 8px;
    background: rgba(15, 24, 41, 0.92);
    border: 1px solid #1e3a5f;
    color: #94a3b8;
    font-size: 10px;
    line-height: 1.4;
  }

  .status-pill.ok {
    border-color: rgba(34, 197, 94, 0.4);
    color: #86efac;
  }

  .status-pill.error {
    border-color: rgba(239, 68, 68, 0.4);
    color: #fca5a5;
    flex-wrap: wrap;
  }

  .err-icon {
    font-size: 11px;
    flex-shrink: 0;
  }

  .jump-btn {
    margin-left: auto;
    flex-shrink: 0;
    background: rgba(239, 68, 68, 0.15);
    border: 1px solid rgba(239, 68, 68, 0.4);
    color: #fca5a5;
    border-radius: 4px;
    padding: 2px 7px;
    font-size: 9px;
    cursor: pointer;
    transition: background 0.12s;
  }

  .jump-btn:hover {
    background: rgba(239, 68, 68, 0.28);
  }

  /* ── Zoom-Buttons ──────────────────────────────────────────── */
  .ms-zoom {
    position: absolute;
    bottom: 12px;
    left: 12px;
    display: flex;
    gap: 4px;
    align-items: center;
    z-index: 2;
  }

  .ms-zoom-btn {
    background: rgba(15, 24, 41, 0.88);
    border: 1px solid #1e3a5f;
    color: #60a5fa;
    border-radius: 5px;
    padding: 4px 9px;
    font-size: 13px;
    cursor: pointer;
  }

  .ms-zoom-btn:hover {
    border-color: #2563eb;
  }

  .ms-zoom-readout {
    display: inline-flex;
    align-items: center;
    min-width: 48px;
    justify-content: center;
    background: rgba(15, 24, 41, 0.78);
    border: 1px solid rgba(30, 58, 95, 0.75);
    color: #94a3b8;
    border-radius: 5px;
    padding: 4px 8px;
    font-size: 10px;
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas,
      monospace;
  }

  /* ── Vorschau-Button ───────────────────────────────────────── */
  .preview-btn {
    position: absolute;
    bottom: 12px;
    right: 12px;
    z-index: 2;
    background: rgba(15, 24, 41, 0.88);
    border: 1px solid #2563eb;
    color: #60a5fa;
    border-radius: 6px;
    padding: 5px 14px;
    font-size: 11px;
    cursor: pointer;
    transition: background 0.14s;
  }

  .preview-btn:hover:not(:disabled) {
    background: #1e3a5f;
  }
  .preview-btn:disabled {
    opacity: 0.45;
    cursor: default;
  }

  /* N6.2: "Jetzt mähen" button */
  .mow-btn {
    position: absolute;
    bottom: 48px;
    right: 10px;
    z-index: 4;
    padding: 6px 14px;
    border: 1px solid #16a34a;
    border-radius: 6px;
    background: #052e16;
    color: #4ade80;
    font-size: 11px;
    font-weight: 700;
    cursor: pointer;
  }
  .mow-btn:hover {
    background: #14532d;
  }

  /* ── Leer-Zustand ──────────────────────────────────────────── */
  .empty {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    color: #475569;
    font-size: 0.84rem;
    text-align: center;
    padding: 0 2rem;
    pointer-events: none;
    z-index: 1;
  }
</style>
