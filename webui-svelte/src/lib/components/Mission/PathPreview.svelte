<script lang="ts">
  import { createEventDispatcher } from 'svelte'
  import type { Mission } from '../../stores/missions'
  import type { Zone, Point, RobotMap } from '../../stores/map'
  import { previewPlannerRoutes, type PlannerPreviewRequest, type PlannerPreviewRoute, type RouteSemantic, type RouteValidationError } from '../../api/rest'

  export let zones: Zone[] = []
  export let perimeter: Point[] = []
  export let exclusions: Point[][] = []
  export let dock: Point[] = []
  export let mission: Mission | null = null
  export let selectedZoneId: string | null = null
  export let map: RobotMap | null = null

  const dispatch = createEventDispatcher<{ selectzone: { zoneId: string } }>()

  let zoom = 1
  let offsetX = 0
  let offsetY = 0
  let dragging = false
  let dragStartX = 0
  let dragStartY = 0

  const width = 820
  const height = 520
  export const zoneColors = ['#a855f7', '#22d3ee', '#22c55e', '#f59e0b', '#f97316', '#38bdf8']

  type Bounds = { minX: number; minY: number; maxX: number; maxY: number }

  function boundsOf(pts: Point[]): Bounds {
    if (pts.length === 0) return { minX: 0, minY: 0, maxX: 10, maxY: 10 }
    return {
      minX: Math.min(...pts.map(p => p.x)),
      minY: Math.min(...pts.map(p => p.y)),
      maxX: Math.max(...pts.map(p => p.x)),
      maxY: Math.max(...pts.map(p => p.y)),
    }
  }

  function project(point: Point, bounds: Bounds) {
    const spanX = Math.max(1, bounds.maxX - bounds.minX)
    const spanY = Math.max(1, bounds.maxY - bounds.minY)
    const baseScale = Math.min((width - 120) / spanX, (height - 120) / spanY)
    const scale = baseScale * zoom
    const centerX = width / 2 + offsetX
    const centerY = height / 2 + offsetY
    const midX = (bounds.minX + bounds.maxX) / 2
    const midY = (bounds.minY + bounds.maxY) / 2
    return {
      x: centerX + (point.x - midX) * scale,
      y: centerY - (point.y - midY) * scale,
    }
  }

  function polyPoints(pts: Point[], bounds: Bounds): string {
    return pts.map(p => {
      const { x, y } = project(p, bounds)
      return `${x.toFixed(2)},${y.toFixed(2)}`
    }).join(' ')
  }

  function polygonCenter(zone: Zone): Point {
    const total = zone.polygon.reduce((acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }), { x: 0, y: 0 })
    return { x: total.x / Math.max(1, zone.polygon.length), y: total.y / Math.max(1, zone.polygon.length) }
  }

  function labelWidth(text: string) {
    return Math.max(44, Math.min(108, text.length * 5.2 + 10))
  }

  function labelPosition(point: Point, bounds: Bounds, text: string) {
    const anchor = project(point, bounds)
    const width = labelWidth(text)
    return {
      x: Math.max(8, Math.min(width - 8, anchor.x)),
      y: Math.max(12, Math.min(height - 10, anchor.y)),
      width,
    }
  }

  function startDrag(event: PointerEvent) {
    dragging = true
    dragStartX = event.clientX - offsetX
    dragStartY = event.clientY - offsetY
  }

  function duringDrag(event: PointerEvent) {
    if (!dragging) return
    offsetX = event.clientX - dragStartX
    offsetY = event.clientY - dragStartY
  }

  function endDrag() { dragging = false }

  function onWheel(event: WheelEvent) {
    event.preventDefault()
    const delta = event.deltaY < 0 ? 0.12 : -0.12
    zoom = Math.max(0.5, Math.min(3, zoom + delta))
  }

  function zoomIn() { zoom = Math.min(3, zoom + 0.2) }
  function zoomOut() { zoom = Math.max(0.5, zoom - 0.2) }
  function resetView() { zoom = 1; offsetX = 0; offsetY = 0 }

  $: missionZones = mission
    ? mission.zoneIds
        .map(id => zones.find(z => z.id === id))
        .filter((z): z is Zone => Boolean(z))
    : []

  // Bounds: Perimeter wenn vorhanden, sonst Zonen
  $: viewPts = perimeter.length >= 3
    ? perimeter
    : missionZones.flatMap(z => z.polygon)
  $: bounds = boundsOf(viewPts)

  let plannerRoutes: PlannerPreviewRoute[] = []
  let plannerLoading = false
  let plannerError = ''
  let plannerRequestId = 0

  const semanticColor: Record<RouteSemantic, string> = {
    coverage_edge:       '#22d3ee',   // cyan — headland
    coverage_infill:     '#22c55e',   // green — stripes
    transit_within_zone: '#f59e0b',   // amber — intra-zone transition
    transit_inter_zone:  '#f97316',   // orange — inter-zone transition
    dock_approach:       '#fbbf24',   // yellow — dock
    recovery:            '#ef4444',   // red — recovery
    unknown:             '#94a3b8',   // slate — legacy
  }

  interface SemanticRun {
    semantic: RouteSemantic
    points: Point[]
  }

  function routeSemanticRuns(route: PlannerPreviewRoute['route'] | undefined): SemanticRun[] {
    const pts = route?.points
    if (!pts || pts.length < 2) return []
    const runs: SemanticRun[] = []
    let current: SemanticRun = { semantic: pts[0].semantic ?? 'unknown', points: [{ x: pts[0].p[0], y: pts[0].p[1] }] }
    for (let i = 1; i < pts.length; i++) {
      const sem: RouteSemantic = pts[i].semantic ?? 'unknown'
      current.points.push({ x: pts[i].p[0], y: pts[i].p[1] })
      if (sem !== current.semantic || i === pts.length - 1) {
        runs.push(current)
        current = { semantic: sem, points: [{ x: pts[i].p[0], y: pts[i].p[1] }] }
      }
    }
    return runs
  }

  function routePoints(route: PlannerPreviewRoute['route'] | undefined): Point[] {
    return route?.points.map((entry) => ({ x: entry.p[0], y: entry.p[1] })) ?? []
  }

  function plannerMapSnapshot(source: RobotMap): PlannerPreviewRequest['map'] {
    return {
      perimeter: source.perimeter.map((point) => [point.x, point.y]),
      dock: source.dock.map((point) => [point.x, point.y]),
      mow: source.mow.map((point) => [point.x, point.y]),
      exclusions: source.exclusions.map((exclusion) => exclusion.map((point) => [point.x, point.y])),
      exclusionMeta: source.exclusionMeta?.map((entry) => ({
        type: entry.type,
        clearance: entry.clearance,
        costScale: entry.costScale,
      })),
      zones: source.zones.map((zone) => ({
        id: zone.id,
        order: zone.order,
        polygon: zone.polygon.map((point) => [point.x, point.y]),
        settings: {
          name: zone.settings.name,
          stripWidth: zone.settings.stripWidth,
          angle: zone.settings.angle,
          edgeMowing: zone.settings.edgeMowing,
          edgeRounds: zone.settings.edgeRounds,
          speed: zone.settings.speed,
          pattern: zone.settings.pattern,
          reverseAllowed: zone.settings.reverseAllowed,
          clearance: zone.settings.clearance,
        },
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
            corridor: source.dockMeta.corridor.map((point) => [point.x, point.y]),
            finalAlignHeadingDeg: source.dockMeta.finalAlignHeadingDeg,
            slowZoneRadius: source.dockMeta.slowZoneRadius,
          }
        : undefined,
      captureMeta: source.captureMeta,
    } as unknown as PlannerPreviewRequest['map']
  }

  function plannerMissionSnapshot(source: Mission): PlannerPreviewRequest['mission'] {
    return {
      id: source.id,
      name: source.name,
      zoneIds: [...source.zoneIds],
      overrides: source.overrides,
      schedule: source.schedule,
    }
  }

  async function refreshPlannerPreview() {
    if (!map || !mission || missionZones.length === 0) {
      plannerRoutes = []
      plannerLoading = false
      plannerError = ''
      return
    }

    const requestId = ++plannerRequestId
    plannerLoading = true
    plannerError = ''
    try {
      const response = await previewPlannerRoutes({ map: plannerMapSnapshot(map), mission: plannerMissionSnapshot(mission) })
      if (requestId !== plannerRequestId) return
      plannerRoutes = response.routes ?? []
      const hasRouteError = response.routes.some((entry) => !entry.ok)
      const hasValidationError = response.routes.some((entry) => entry.valid === false)
      if (hasRouteError) {
        plannerError = 'Die C++-Bahnplanung konnte nicht vollständig erzeugt werden.'
      } else if (hasValidationError) {
        const firstErr = response.routes.find(e => e.valid === false)?.error
        plannerError = firstErr
          ? `Route ungültig: ${firstErr}`
          : 'Route enthält ungültige Segmente — Start gesperrt.'
      } else {
        plannerError = ''
      }
    } catch (err) {
      if (requestId !== plannerRequestId) return
      plannerRoutes = []
      plannerError = err instanceof Error ? err.message : 'Planner-Vorschau fehlgeschlagen'
    } finally {
      if (requestId === plannerRequestId) plannerLoading = false
    }
  }

  $: if (map && mission && missionZones.length > 0) {
    void refreshPlannerPreview()
  } else {
    plannerRoutes = []
    plannerLoading = false
    plannerError = ''
  }

  $: previewEntry = plannerRoutes.find((entry) => entry.ok && entry.route)
  $: previewRoute = routePoints(previewEntry?.route)
  $: hasPreviewRoute = previewRoute.length >= 2
  $: semanticRuns = routeSemanticRuns(previewEntry?.route)
  $: validationErrors = (previewEntry?.validationErrors ?? []) as RouteValidationError[]
  $: invalidPoints = validationErrors
    .filter(e => e.pointIndex >= 0 && previewEntry?.route?.points[e.pointIndex])
    .map(e => {
      const pt = previewEntry!.route!.points[e.pointIndex]
      return { point: { x: pt.p[0], y: pt.p[1] }, message: e.message }
    })
</script>

<div class="preview-root">
  {#if missionZones.length > 0}
    <div class="ms-legend">
      {#each missionZones as zone, i}
        <button
          type="button"
          class="ms-leg-item"
          class:sel={selectedZoneId === zone.id}
          on:click={() => dispatch('selectzone', { zoneId: zone.id })}
        >
          <span class="ms-leg-dot" style="background:{zoneColors[i % zoneColors.length]}"></span>
          <span class="ms-leg-name">{zone.settings.name}</span>
          <span class="ms-leg-angle">{(mission?.overrides?.[zone.id]?.angle ?? zone.settings.angle)}°</span>
        </button>
      {/each}
    </div>
  {/if}

  {#if plannerLoading}
    <div class="planner-status">Planner wird geprüft...</div>
  {:else if plannerError}
    <div class="planner-status error">{plannerError}</div>
  {:else if missionZones.length > 0 && !hasPreviewRoute}
    <div class="planner-status warn">Keine Bahnvorschau vom C++-Planner erhalten.</div>
  {/if}

  {#if missionZones.length === 0}
    <div class="empty">
      {mission ? 'Keine Zonen in dieser Mission.' : 'Keine Mission ausgewählt.'}
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
    on:wheel|preventDefault={onWheel}
  >
    <defs>
      <pattern id="pp-grid" width="30" height="30" patternUnits="userSpaceOnUse">
        <path d="M30 0L0 0 0 30" fill="none" stroke="#1e3a5f" stroke-width="0.5" />
      </pattern>
    </defs>

    <rect x="-9999" y="-9999" width="19999" height="19999" fill="#070d18" />
    <rect x="-9999" y="-9999" width="19999" height="19999" fill="url(#pp-grid)" opacity="0.8" />

    <!-- Perimeter -->
    {#if perimeter.length >= 3}
      <polygon
        points={polyPoints(perimeter, bounds)}
        fill="rgba(30, 58, 95, 0.06)"
        stroke="#7c9ccf"
        stroke-width="1.2"
        stroke-dasharray="5 4"
        stroke-linejoin="round"
      />
    {/if}

    <!-- NoGo-Ausschlussflächen -->
    {#if exclusions.length > 0}
      {#each exclusions as exclusion}
        {#if exclusion.length >= 3}
          <polygon
            points={polyPoints(exclusion, bounds)}
            fill="rgba(220, 38, 38, 0.10)"
            stroke="#f87171"
            stroke-width="1.15"
            stroke-dasharray="4 4"
            stroke-linejoin="round"
          />
        {/if}
      {/each}
    {/if}

    <!-- Docking-Pfad -->
    {#if dock.length >= 2}
      <polyline
        points={polyPoints(dock, bounds)}
        fill="none"
        stroke="#fbbf24"
        stroke-width="1.8"
        stroke-linecap="round"
        stroke-linejoin="round"
        stroke-dasharray="6 5"
      />
      {@const dockEnd = project(dock[dock.length - 1], bounds)}
      <circle cx={dockEnd.x} cy={dockEnd.y} r="4" fill="#fbbf24" />
    {/if}

    <!-- Zonen -->
    {#each missionZones as zone, i}
      {@const color = zoneColors[i % zoneColors.length]}
      {@const label = `${zone.settings.name} · ${(mission?.overrides?.[zone.id]?.angle ?? zone.settings.angle)}°`}
      {@const labelPos = labelPosition(polygonCenter(zone), bounds, label)}

      <g
        role="button"
        tabindex="0"
        aria-label={`Zone ${zone.settings.name} auswählen`}
        on:click={() => dispatch('selectzone', { zoneId: zone.id })}
        on:keydown={(e) => {
          if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault()
            dispatch('selectzone', { zoneId: zone.id })
          }
        }}
      >
        <polygon
          points={polyPoints(zone.polygon, bounds)}
          fill={selectedZoneId === zone.id ? `${color}36` : `${color}18`}
          stroke={color}
          stroke-width={selectedZoneId === zone.id ? 2.4 : 1.35}
          stroke-dasharray="4 3"
          stroke-linejoin="round"
        />

        <g>
          <rect
            x={labelPos.x - labelPos.width / 2}
            y={labelPos.y - 13}
            width={labelPos.width}
            height="14"
            rx="7"
            fill="rgba(7, 13, 24, 0.72)"
            stroke={selectedZoneId === zone.id ? color : 'rgba(148, 163, 184, 0.2)'}
            stroke-width="1"
          />
          <text
            x={labelPos.x}
            y={labelPos.y - 4}
            text-anchor="middle"
            fill="#e2e8f0"
            font-size="8.5"
            font-weight="600"
          >
            {label}
          </text>
        </g>
      </g>
    {/each}

    <!-- Invalid route points (validation errors) -->
    {#each invalidPoints as inv}
      {@const proj = project(inv.point, bounds)}
      <circle cx={proj.x} cy={proj.y} r="5" fill="none" stroke="#ef4444" stroke-width="1.5" />
      <circle cx={proj.x} cy={proj.y} r="2.5" fill="#ef4444" />
    {/each}

    {#if hasPreviewRoute}
      <!-- Shadow pass for contrast -->
      <polyline
        points={polyPoints(previewRoute, bounds)}
        fill="none"
        stroke="rgba(7, 13, 24, 0.85)"
        stroke-width="3.2"
        stroke-linecap="round"
        stroke-linejoin="round"
      />
      <!-- Semantic colour runs -->
      {#each semanticRuns as run}
        {#if run.points.length >= 2}
          <polyline
            points={polyPoints(run.points, bounds)}
            fill="none"
            stroke={semanticColor[run.semantic]}
            stroke-opacity="0.92"
            stroke-width="1.5"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
        {/if}
      {/each}
    {/if}
  </svg>

  <div class="ms-zoom">
    <button type="button" class="ms-zoom-btn" on:click={zoomIn}>+</button>
    <button type="button" class="ms-zoom-btn" on:click={zoomOut}>−</button>
    <button type="button" class="ms-zoom-btn" on:click={resetView}>⊙</button>
  </div>
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

  .ms-legend {
    position: absolute;
    top: 10px;
    left: 10px;
    z-index: 2;
    display: flex;
    flex-direction: column;
    gap: 5px;
    pointer-events: auto;
  }

  .ms-leg-item {
    display: flex;
    align-items: center;
    gap: 6px;
    background: rgba(15, 24, 41, 0.85);
    border: 1px solid #1e3a5f;
    border-radius: 6px;
    padding: 5px 9px;
    font-size: 11px;
    cursor: pointer;
    transition: border-color 0.15s;
    color: #e2e8f0;
  }

  .ms-leg-item:hover { border-color: #2563eb; }
  .ms-leg-item.sel { border-color: #2563eb; background: rgba(12, 26, 58, 0.92); }

  .ms-leg-dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .ms-leg-name { color: #e2e8f0; }
  .ms-leg-angle { color: #475569; font-size: 10px; font-family: monospace; }

  .planner-status {
    position: absolute;
    top: 10px;
    right: 10px;
    z-index: 3;
    padding: 5px 9px;
    border-radius: 999px;
    background: rgba(15, 24, 41, 0.88);
    border: 1px solid rgba(251, 191, 36, 0.35);
    color: #fde68a;
    font-size: 10px;
  }

  .planner-status.error {
    border-color: rgba(248, 113, 113, 0.4);
    color: #fecaca;
  }

  .planner-status.warn {
    border-color: rgba(251, 191, 36, 0.35);
    color: #fde68a;
  }

  .ms-zoom {
    position: absolute;
    bottom: 12px;
    left: 12px;
    display: flex;
    gap: 5px;
    z-index: 2;
  }

  .ms-zoom-btn {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    color: #60a5fa;
    border-radius: 6px;
    padding: 5px 9px;
    font-size: 13px;
    cursor: pointer;
  }

  .ms-zoom-btn:hover { border-color: #2563eb; }

  .empty {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    color: #475569;
    font-size: 0.84rem;
    pointer-events: none;
    z-index: 1;
  }
</style>
