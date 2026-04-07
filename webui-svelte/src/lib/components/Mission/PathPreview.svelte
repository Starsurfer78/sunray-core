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

  // Layer-Toggles
  let showPerimeter = true
  let showZones = true
  let showPaths = true
  let showErrors = true

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

  function toPathD(pts: Point[], bounds: Bounds): string {
    if (pts.length < 3) return ''
    const projected = pts.map(p => project(p, bounds))
    return 'M ' + projected.map(p => `${p.x.toFixed(2)},${p.y.toFixed(2)}`).join(' L ') + ' Z'
  }

  function polygonCenter(zone: Zone): Point {
    const total = zone.polygon.reduce((acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }), { x: 0, y: 0 })
    return { x: total.x / Math.max(1, zone.polygon.length), y: total.y / Math.max(1, zone.polygon.length) }
  }

  function labelWidth(text: string) {
    return Math.max(44, Math.min(120, text.length * 5.2 + 10))
  }

  function labelPosition(point: Point, bounds: Bounds, text: string) {
    const anchor = project(point, bounds)
    const w = labelWidth(text)
    return {
      x: Math.max(8, Math.min(width - 8, anchor.x)),
      y: Math.max(12, Math.min(height - 10, anchor.y)),
      width: w,
    }
  }

  // ── Zoom / Pan ────────────────────────────────────────────────────────────

  function startDrag(event: PointerEvent) {
    if (event.button !== 0) return
    dragging = true
    dragStartX = event.clientX - offsetX
    dragStartY = event.clientY - offsetY
    const svg = event.currentTarget as SVGSVGElement
    svg.setPointerCapture(event.pointerId)
  }

  function duringDrag(event: PointerEvent) {
    if (!dragging) return
    offsetX = event.clientX - dragStartX
    offsetY = event.clientY - dragStartY
  }

  function endDrag(event: PointerEvent) {
    if (!dragging) return
    dragging = false
    try {
      (event.currentTarget as SVGSVGElement).releasePointerCapture(event.pointerId)
    } catch { /* ignore */ }
  }

  function onWheel(event: WheelEvent) {
    event.preventDefault()
    const svg = event.currentTarget as SVGSVGElement
    const rect = svg.getBoundingClientRect()
    const svgX = (event.clientX - rect.left) * (width / rect.width)
    const svgY = (event.clientY - rect.top) * (height / rect.height)

    const spanX = Math.max(1, bounds.maxX - bounds.minX)
    const spanY = Math.max(1, bounds.maxY - bounds.minY)
    const baseScale = Math.min((width - 120) / spanX, (height - 120) / spanY)
    const oldScale = baseScale * zoom

    const factor = event.deltaY < 0 ? 1.1 : 0.9
    const newZoom = Math.max(0.3, Math.min(12, zoom * factor))
    const newScale = baseScale * newZoom

    const worldX = (svgX - width / 2 - offsetX) / oldScale
    const worldY = (svgY - height / 2 - offsetY) / oldScale
    offsetX = svgX - width / 2 - worldX * newScale
    offsetY = svgY - height / 2 - worldY * newScale
    zoom = newZoom
  }

  function zoomIn() { zoom = Math.min(12, zoom * 1.2) }
  function zoomOut() { zoom = Math.max(0.3, zoom * 0.8) }
  function resetView() { zoom = 1; offsetX = 0; offsetY = 0 }

  function jumpToPoint(pt: Point) {
    const spanX = Math.max(1, bounds.maxX - bounds.minX)
    const spanY = Math.max(1, bounds.maxY - bounds.minY)
    const baseScale = Math.min((width - 120) / spanX, (height - 120) / spanY)
    zoom = 4
    const scale = baseScale * zoom
    const midX = (bounds.minX + bounds.maxX) / 2
    const midY = (bounds.minY + bounds.maxY) / 2
    offsetX = -(pt.x - midX) * scale
    offsetY =  (pt.y - midY) * scale
  }

  // ── Planner ───────────────────────────────────────────────────────────────

  $: missionZones = mission
    ? mission.zoneIds
        .map(id => zones.find(z => z.id === id))
        .filter((z): z is Zone => Boolean(z))
    : []

  $: viewPts = perimeter.length >= 3
    ? perimeter
    : missionZones.flatMap(z => z.polygon)
  $: bounds = boundsOf(viewPts)

  let plannerRoutes: PlannerPreviewRoute[] = []
  let plannerLoading = false
  let plannerError = ''
  let plannerRequestId = 0

  // Semantische Farben — recovery ist grau (normales Fallback, kein Fehler)
  const semanticColor: Record<RouteSemantic, string> = {
    coverage_edge:       '#22d3ee',   // cyan  — Randmähen
    coverage_infill:     '#4ade80',   // green — Innenbahnen
    transit_within_zone: '#f59e0b',   // amber — Übergang innerhalb Zone
    transit_inter_zone:  '#38bdf8',   // sky   — Übergang zwischen Zonen
    dock_approach:       '#fbbf24',   // yellow — Andocken
    recovery:            '#64748b',   // slate — Fallback-Pfad (kein Fehler!)
    unknown:             '#475569',   // muted
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
        plannerError = 'Bahnplanung fehlgeschlagen'
      } else if (hasValidationError) {
        plannerError = 'route_invalid'
      } else {
        plannerError = ''
      }
    } catch (err) {
      if (requestId !== plannerRequestId) return
      plannerRoutes = []
      plannerError = err instanceof Error ? err.message : 'Verbindungsfehler'
    } finally {
      if (requestId === plannerRequestId) plannerLoading = false
    }
  }

  $: if (!map || !mission || missionZones.length === 0) {
    plannerRoutes = []
    plannerLoading = false
    plannerError = ''
  }

  $: previewEntry = plannerRoutes.find((entry) => entry.ok && entry.route)
  $: previewRoute = routePoints(previewEntry?.route)
  $: hasPreviewRoute = previewRoute.length >= 2
  $: semanticRuns = routeSemanticRuns(previewEntry?.route)
  $: validationErrors = (previewEntry?.validationErrors ?? []) as RouteValidationError[]

  // Nur die ersten 3 Fehlerpunkte — kein roter Teppich
  $: errorMarkers = validationErrors
    .slice(0, 3)
    .filter(e => e.pointIndex >= 0 && previewEntry?.route?.points[e.pointIndex])
    .map(e => {
      const pt = previewEntry!.route!.points[e.pointIndex]
      return { point: { x: pt.p[0], y: pt.p[1] }, message: e.message, zoneId: e.zoneId }
    })

  // Fehlersegmente: zusammenhängende Runs von invalid-Indizes → rote Linie nur dort
  $: invalidSegments = (() => {
    if (validationErrors.length === 0) return []
    const indices = new Set(validationErrors.map(e => e.pointIndex))
    const pts = previewEntry?.route?.points ?? []
    const segs: Point[][] = []
    let seg: Point[] = []
    for (let i = 0; i < pts.length - 1; i++) {
      if (indices.has(i) || indices.has(i + 1)) {
        if (seg.length === 0) seg.push({ x: pts[i].p[0], y: pts[i].p[1] })
        seg.push({ x: pts[i + 1].p[0], y: pts[i + 1].p[1] })
      } else {
        if (seg.length >= 2) segs.push(seg)
        seg = []
      }
    }
    if (seg.length >= 2) segs.push(seg)
    return segs
  })()

  // Fehlerzusammenfassung für die UI
  $: firstError = validationErrors[0]
  $: firstErrorZone = firstError
    ? missionZones.find(z => z.id === firstError.zoneId)
    : null
  $: errorSummary = firstError
    ? `Übergang nicht planbar${firstErrorZone ? ` in Zone „${firstErrorZone.settings.name}"` : ''}`
    : (plannerError && plannerError !== 'route_invalid' ? plannerError : '')
  $: errorCount = validationErrors.length
</script>

<div class="preview-root">

  <!-- Layer-Toggles oben links -->
  <div class="layer-bar">
    <button class="layer-btn" class:off={!showPerimeter} type="button" on:click={() => showPerimeter = !showPerimeter}>Perimeter</button>
    <button class="layer-btn" class:off={!showZones}     type="button" on:click={() => showZones     = !showZones}>Zonen</button>
    <button class="layer-btn" class:off={!showPaths}     type="button" on:click={() => showPaths     = !showPaths}>Bahnen</button>
    <button class="layer-btn err" class:off={!showErrors} type="button" on:click={() => showErrors   = !showErrors}>Fehler</button>
  </div>

  <!-- Zonen-Legende oben links (unterhalb Layer-Bar) -->
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

  <!-- Statusbereich oben rechts -->
  <div class="status-area">
    {#if plannerLoading}
      <div class="status-pill">Wird berechnet…</div>
    {:else if plannerError === 'route_invalid' && errorCount > 0}
      <div class="status-pill error">
        <span class="err-icon">⚠</span>
        <span>{errorCount} Fehler · {errorSummary}</span>
        {#if errorMarkers.length > 0}
          <button class="jump-btn" type="button" on:click={() => jumpToPoint(errorMarkers[0].point)}>
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
    on:wheel|nonpassive={onWheel}
  >
    <defs>
      <pattern id="pp-grid" width="30" height="30" patternUnits="userSpaceOnUse">
        <path d="M30 0L0 0 0 30" fill="none" stroke="#1e3a5f" stroke-width="0.5" />
      </pattern>
      {#if perimeter.length >= 3}
        <clipPath id="perimeter-clip">
          <polygon points={polyPoints(perimeter, bounds)} />
        </clipPath>
        <clipPath id="mow-clip" clip-rule="evenodd">
          <path d={toPathD(perimeter, bounds)} />
          {#each exclusions as exc}
            {#if exc.length >= 3}
              <path d={toPathD(exc, bounds)} />
            {/if}
          {/each}
        </clipPath>
      {/if}
    </defs>

    <rect x="-9999" y="-9999" width="19999" height="19999" fill="#070d18" />
    <rect x="-9999" y="-9999" width="19999" height="19999" fill="url(#pp-grid)" opacity="0.6" />

    <!-- 1. Perimeter — stärkste Grundstruktur, immer sichtbar -->
    {#if showPerimeter && perimeter.length >= 3}
      <polygon
        points={polyPoints(perimeter, bounds)}
        fill="rgba(59, 130, 246, 0.07)"
        stroke="#60a5fa"
        stroke-width="1.6"
        stroke-linejoin="round"
      />
    {/if}

    <!-- 2. No-Go-Flächen -->
    {#if showPerimeter && exclusions.length > 0}
      {#each exclusions as exclusion}
        {#if exclusion.length >= 3}
          <polygon
            points={polyPoints(exclusion, bounds)}
            fill="rgba(220, 38, 38, 0.15)"
            stroke="#f87171"
            stroke-width="0.8"
            stroke-dasharray="4 3"
            stroke-linejoin="round"
          />
        {/if}
      {/each}
    {/if}

    <!-- 3. Docking-Pfad -->
    {#if dock.length >= 2}
      <polyline
        points={polyPoints(dock, bounds)}
        fill="none"
        stroke="#fbbf24"
        stroke-width="0.9"
        stroke-linecap="round"
        stroke-linejoin="round"
        stroke-dasharray="5 4"
      />
      {@const dockEnd = project(dock[dock.length - 1], bounds)}
      <circle cx={dockEnd.x} cy={dockEnd.y} r="3" fill="#fbbf24" />
    {/if}

    <!-- 4. Zonen — geklippt am Perimeter, halbtransparent -->
    {#if showZones}
      <g clip-path={perimeter.length >= 3 ? 'url(#perimeter-clip)' : undefined}>
        {#each missionZones as zone, i}
          {@const color = zoneColors[i % zoneColors.length]}
          {@const label = `${zone.settings.name} · ${(mission?.overrides?.[zone.id]?.angle ?? zone.settings.angle)}°`}
          {@const labelPos = labelPosition(polygonCenter(zone), bounds, label)}

          <g
            role="button"
            tabindex="0"
            aria-label={`Zone ${zone.settings.name} auswählen`}
            on:click|stopPropagation={() => dispatch('selectzone', { zoneId: zone.id })}
            on:keydown={(e) => {
              if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault()
                dispatch('selectzone', { zoneId: zone.id })
              }
            }}
          >
            <polygon
              points={polyPoints(zone.polygon, bounds)}
              fill={selectedZoneId === zone.id ? `${color}28` : `${color}12`}
              stroke={color}
              stroke-width={selectedZoneId === zone.id ? 1.2 : 0.7}
              stroke-dasharray="5 3"
              stroke-linejoin="round"
            />
            <rect
              x={labelPos.x - labelPos.width / 2}
              y={labelPos.y - 13}
              width={labelPos.width}
              height="14"
              rx="7"
              fill="rgba(7, 13, 24, 0.75)"
              stroke={selectedZoneId === zone.id ? color : 'rgba(148, 163, 184, 0.18)'}
              stroke-width="0.8"
            />
            <text
              x={labelPos.x}
              y={labelPos.y - 4}
              text-anchor="middle"
              fill="#cbd5e1"
              font-size="8"
              font-weight="600"
            >{label}</text>
          </g>
        {/each}
      </g>
    {/if}

    <!-- 5. Mähbahnen — geklippt am Perimeter minus No-Go -->
    {#if showPaths && hasPreviewRoute}
      <g clip-path={perimeter.length >= 3 ? 'url(#mow-clip)' : undefined}>
        {#each semanticRuns as run}
          {#if run.points.length >= 2}
            <polyline
              points={polyPoints(run.points, bounds)}
              fill="none"
              stroke={semanticColor[run.semantic]}
              stroke-opacity="0.85"
              stroke-width="0.8"
              stroke-linecap="round"
              stroke-linejoin="round"
            />
          {/if}
        {/each}
      </g>
    {/if}

    <!-- 6. Fehler — nur die konkreten Problemsegmente, nicht alles rot -->
    {#if showErrors}
      {#each invalidSegments as seg}
        <polyline
          points={polyPoints(seg, bounds)}
          fill="none"
          stroke="#ef4444"
          stroke-width="1.8"
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-opacity="0.9"
        />
      {/each}
      <!-- Marker nur am ersten Fehlerpunkt -->
      {#if errorMarkers.length > 0}
        {@const proj = project(errorMarkers[0].point, bounds)}
        <!-- Kreuz-Markierung -->
        <line x1={proj.x - 5} y1={proj.y - 5} x2={proj.x + 5} y2={proj.y + 5} stroke="#ef4444" stroke-width="1.5" />
        <line x1={proj.x + 5} y1={proj.y - 5} x2={proj.x - 5} y2={proj.y + 5} stroke="#ef4444" stroke-width="1.5" />
        <circle cx={proj.x} cy={proj.y} r="7" fill="none" stroke="#ef4444" stroke-width="1" stroke-opacity="0.5" />
      {/if}
    {/if}
  </svg>

  <!-- Zoom-Buttons -->
  <div class="ms-zoom">
    <button type="button" class="ms-zoom-btn" on:click={zoomIn}>+</button>
    <button type="button" class="ms-zoom-btn" on:click={zoomOut}>−</button>
    <button type="button" class="ms-zoom-btn" on:click={resetView}>⊙</button>
  </div>

  <!-- Vorschau-Button -->
  {#if missionZones.length > 0}
    <button
      type="button"
      class="preview-btn"
      disabled={plannerLoading}
      on:click={() => void refreshPlannerPreview()}
    >
      {plannerLoading ? 'Wird berechnet…' : 'Vorschau berechnen'}
    </button>
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

  .layer-btn:hover { border-color: #3b82f6; }
  .layer-btn.off { opacity: 0.38; color: #475569; }
  .layer-btn.err { color: #fca5a5; border-color: rgba(239,68,68,0.3); }
  .layer-btn.err:hover { border-color: #ef4444; }

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

  .ms-leg-item:hover { border-color: #2563eb; }
  .ms-leg-item.sel   { border-color: #2563eb; background: rgba(12, 26, 58, 0.92); }

  .ms-leg-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .ms-leg-name  { color: #e2e8f0; }
  .ms-leg-angle { color: #475569; font-size: 9px; font-family: monospace; }

  /* ── Status-Bereich ────────────────────────────────────────── */
  .status-area {
    position: absolute;
    top: 10px;
    right: 10px;
    z-index: 3;
    max-width: 340px;
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

  .err-icon { font-size: 11px; flex-shrink: 0; }

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

  .jump-btn:hover { background: rgba(239, 68, 68, 0.28); }

  /* ── Zoom-Buttons ──────────────────────────────────────────── */
  .ms-zoom {
    position: absolute;
    bottom: 12px;
    left: 12px;
    display: flex;
    gap: 4px;
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

  .ms-zoom-btn:hover { border-color: #2563eb; }

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

  .preview-btn:hover:not(:disabled) { background: #1e3a5f; }
  .preview-btn:disabled { opacity: 0.45; cursor: default; }

  /* ── Leer-Zustand ──────────────────────────────────────────── */
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
