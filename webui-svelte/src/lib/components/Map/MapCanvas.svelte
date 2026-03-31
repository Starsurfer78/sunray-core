<script lang="ts">
  import { createEventDispatcher } from 'svelte'
  import { onDestroy } from 'svelte'
  import { telemetry } from '../../stores/telemetry'
  import type { Point } from '../../stores/map'
  import { mapStore } from '../../stores/map'

  export let width = 900
  export let height = 620
  export let interactive = true
  export let showHeader = true
  export let showViewportActions = true
  export let showRobot = true
  export let showZoomControls = false
  export let allowPointAdd = true
  export let pointAddBlockedReason = ''

  const dispatch = createEventDispatcher<{ pointrejected: { tool: string; reason?: string } }>()

  const RAD_TO_DEG = 180 / Math.PI
  const baseScale = 20
  const grid = 1
  const pointHitRadius = 10
  const robotOuterRadiusM = 0.42
  const robotInnerRadiusM = 0.16
  const robotNoseStartM = 0.42
  const robotNoseEndM = 0.82
  const robotArrowTipM = 0.96
  const robotArrowSideM = 0.66
  const robotArrowHalfWidthM = 0.18

  let zoom = 1
  let offsetX = 0
  let offsetY = 0

  $: currentScale = baseScale * zoom
  let isPanning = false
  let panStartX = 0
  let panStartY = 0
  let panOriginX = 0
  let panOriginY = 0
  let suppressClick = false
  let pointerCaptured = false
  let hasAutoFit = false

  type DragTarget =
    | { kind: 'perimeter'; index: number }
    | { kind: 'dock'; index: number }
    | { kind: 'zone'; zoneId: string; index: number }
    | { kind: 'exclusion'; exclusionIndex: number; index: number }

  let dragTarget: DragTarget | null = null
  let selectedTarget: DragTarget | null = null

  function markerSizeInPx(sizeMeters: number) {
    return Number((sizeMeters * currentScale).toFixed(2))
  }

  function worldToScreen(point: Point, scaleValue = currentScale, xOffset = offsetX, yOffset = offsetY) {
    return {
      x: width / 2 + xOffset + point.x * scaleValue,
      y: height / 2 + yOffset - point.y * scaleValue,
    }
  }

  function clientToSvgCoordinates(clientX: number, clientY: number, target: SVGSVGElement) {
    const point = target.createSVGPoint()
    point.x = clientX
    point.y = clientY
    const ctm = target.getScreenCTM()

    if (!ctm) {
      const rect = target.getBoundingClientRect()
      const scaleX = width / rect.width
      const scaleY = height / rect.height
      return {
        x: (clientX - rect.left) * scaleX,
        y: (clientY - rect.top) * scaleY,
      }
    }

    const svgPoint = point.matrixTransform(ctm.inverse())
    return {
      x: svgPoint.x,
      y: svgPoint.y,
    }
  }

  function screenToWorldFromCoordinates(clientX: number, clientY: number, target: SVGSVGElement) {
    const { x: sx, y: sy } = clientToSvgCoordinates(clientX, clientY, target)
    return {
      x: Number((((sx - width / 2 - offsetX) / currentScale)).toFixed(2)),
      y: Number((((height / 2 + offsetY - sy) / currentScale)).toFixed(2)),
    }
  }

  function screenToWorld(event: MouseEvent, target: SVGSVGElement) {
    return screenToWorldFromCoordinates(event.clientX, event.clientY, target)
  }

  function distanceSquared(a: { x: number; y: number }, b: { x: number; y: number }) {
    const dx = a.x - b.x
    const dy = a.y - b.y
    return dx * dx + dy * dy
  }

  function findDragTarget(clientX: number, clientY: number, target: SVGSVGElement) {
    const cursor = clientToSvgCoordinates(clientX, clientY, target)

    for (let index = 0; index < $mapStore.map.dock.length; index += 1) {
      const screen = worldToScreen($mapStore.map.dock[index], currentScale, offsetX, offsetY)
      if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
        return { kind: 'dock', index } as DragTarget
      }
    }

    for (let index = 0; index < $mapStore.map.perimeter.length; index += 1) {
      const screen = worldToScreen($mapStore.map.perimeter[index], currentScale, offsetX, offsetY)
      if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
        return { kind: 'perimeter', index } as DragTarget
      }
    }

    for (let exclusionIndex = 0; exclusionIndex < $mapStore.map.exclusions.length; exclusionIndex += 1) {
      const exclusion = $mapStore.map.exclusions[exclusionIndex]
      for (let index = 0; index < exclusion.length; index += 1) {
        const screen = worldToScreen(exclusion[index], currentScale, offsetX, offsetY)
        if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
          return { kind: 'exclusion', exclusionIndex, index } as DragTarget
        }
      }
    }

    for (const zone of $mapStore.map.zones) {
      for (let index = 0; index < zone.polygon.length; index += 1) {
        const screen = worldToScreen(zone.polygon[index], currentScale, offsetX, offsetY)
        if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
          return { kind: 'zone', zoneId: zone.id, index } as DragTarget
        }
      }
    }

    return null
  }

  function applyDraggedPoint(target: DragTarget, point: Point) {
    if (target.kind === 'perimeter') {
      mapStore.movePerimeterPoint(target.index, point)
    } else if (target.kind === 'dock') {
      mapStore.moveDockPoint(target.index, point)
    } else if (target.kind === 'zone') {
      mapStore.moveZonePoint(target.zoneId, target.index, point)
    } else if (target.kind === 'exclusion') {
      mapStore.moveExclusionPoint(target.exclusionIndex, target.index, point)
    }
  }

  function deleteTarget(target: DragTarget) {
    if (target.kind === 'perimeter') {
      mapStore.deletePerimeterPoint(target.index)
    } else if (target.kind === 'dock') {
      mapStore.deleteDockPoint(target.index)
    } else if (target.kind === 'zone') {
      mapStore.deleteZonePoint(target.zoneId, target.index)
    } else if (target.kind === 'exclusion') {
      mapStore.deleteExclusionPoint(target.exclusionIndex, target.index)
    }
  }

  function isSelected(target: DragTarget) {
    if (!selectedTarget || selectedTarget.kind !== target.kind) return false
    if (target.kind === 'perimeter' || target.kind === 'dock') {
      return selectedTarget.index === target.index
    }
    if (target.kind === 'zone' && selectedTarget.kind === 'zone') {
      return selectedTarget.zoneId === target.zoneId && selectedTarget.index === target.index
    }
    if (target.kind === 'exclusion' && selectedTarget.kind === 'exclusion') {
      return selectedTarget.exclusionIndex === target.exclusionIndex && selectedTarget.index === target.index
    }
    return false
  }

  export function zoomIn() {
    zoom = Math.max(0.5, Math.min(4, Number((zoom * 1.2).toFixed(3))))
  }

  export function zoomOut() {
    zoom = Math.max(0.5, Math.min(4, Number((zoom * 0.8).toFixed(3))))
  }

  function handlePointerDown(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement
    suppressClick = false

    if (interactive) {
      const drag = findDragTarget(event.clientX, event.clientY, target)
      if (drag) {
        selectedTarget = drag
        dragTarget = drag
        pointerCaptured = true
        target.setPointerCapture(event.pointerId)
        return
      }
    }

    isPanning = true
    pointerCaptured = true
    panStartX = event.clientX
    panStartY = event.clientY
    panOriginX = offsetX
    panOriginY = offsetY
    target.setPointerCapture(event.pointerId)
  }

  function handlePointerMove(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement
    if (dragTarget) {
      applyDraggedPoint(dragTarget, screenToWorldFromCoordinates(event.clientX, event.clientY, target))
      suppressClick = true
      return
    }

    if (isPanning) {
      offsetX = panOriginX + (event.clientX - panStartX)
      offsetY = panOriginY + (event.clientY - panStartY)
      suppressClick = true
    }
  }

  function releasePointer(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement
    if (pointerCaptured) {
      try {
        target.releasePointerCapture(event.pointerId)
      } catch {
        // Ignore if pointer capture is already released.
      }
    }
    pointerCaptured = false
    dragTarget = null
    isPanning = false
  }

  function handleCanvasClick(event: MouseEvent) {
    if (!interactive || $mapStore.selectedTool === 'move') return
    if (suppressClick) {
      suppressClick = false
      return
    }
    if (!allowPointAdd) {
      dispatch('pointrejected', {
        tool: $mapStore.selectedTool,
        reason: pointAddBlockedReason,
      })
      return
    }
    const target = event.currentTarget as SVGSVGElement
    const accepted = mapStore.addPoint(screenToWorld(event, target))
    if (!accepted) {
      dispatch('pointrejected', { tool: $mapStore.selectedTool })
    }
  }

  function selectExclusionArea(exclusionIndex: number) {
    mapStore.selectExclusion(exclusionIndex)
    if ($mapStore.selectedTool !== 'move') {
      mapStore.setTool('nogo')
    }
  }

  function handleWheel(event: WheelEvent) {
    event.preventDefault()
    const delta = event.deltaY < 0 ? 1.1 : 0.9
    zoom = Math.max(0.5, Math.min(4, Number((zoom * delta).toFixed(3))))
  }

  function path(points: Point[], scaleValue = currentScale, xOffset = offsetX, yOffset = offsetY) {
    return points.map((point) => {
      const p = worldToScreen(point, scaleValue, xOffset, yOffset)
      return `${p.x},${p.y}`
    }).join(' ')
  }

  function centroid(points: Point[]) {
    if (points.length === 0) return null
    const sum = points.reduce((acc, point) => ({
      x: acc.x + point.x,
      y: acc.y + point.y,
    }), { x: 0, y: 0 })
    return {
      x: sum.x / points.length,
      y: sum.y / points.length,
    }
  }

  function allMapPoints() {
    const points = [
      ...$mapStore.map.perimeter,
      ...$mapStore.map.dock,
      ...$mapStore.map.mow,
      ...$mapStore.map.zones.flatMap((zone) => zone.polygon),
      ...$mapStore.map.exclusions.flatMap((exclusion) => exclusion),
    ]
    if (showRobot) {
      points.push({ x: $telemetry.x, y: $telemetry.y })
    }
    return points
  }

  function resetView() {
    zoom = 1
    offsetX = 0
    offsetY = 0
  }

  export function fitToContent() {
    const points = allMapPoints()
    if (points.length === 0) {
      resetView()
      return
    }

    const minX = Math.min(...points.map((point) => point.x))
    const maxX = Math.max(...points.map((point) => point.x))
    const minY = Math.min(...points.map((point) => point.y))
    const maxY = Math.max(...points.map((point) => point.y))

    const spanX = Math.max(maxX - minX, 1)
    const spanY = Math.max(maxY - minY, 1)
    const padding = 48

    const availableWidth = Math.max(width - padding * 2, 120)
    const availableHeight = Math.max(height - padding * 2, 120)

    const fitScale = Math.min(availableWidth / spanX, availableHeight / spanY)
    const nextZoom = Math.max(0.25, Math.min(6, Number((fitScale / baseScale).toFixed(3))))
    const nextScale = baseScale * nextZoom
    zoom = nextZoom

    const centerX = (minX + maxX) / 2
    const centerY = (minY + maxY) / 2
    offsetX = Number((-centerX * nextScale).toFixed(2))
    offsetY = Number((centerY * nextScale).toFixed(2))
  }

  export function deleteSelectedPoint() {
    if (!selectedTarget) return false
    deleteTarget(selectedTarget)
    selectedTarget = null
    return true
  }

  $: robotScreen = worldToScreen({ x: $telemetry.x, y: $telemetry.y }, currentScale, offsetX, offsetY)
  $: robotHeadingDeg = -(($telemetry.heading ?? 0) * RAD_TO_DEG)
  $: robotOuterRadius = markerSizeInPx(robotOuterRadiusM)
  $: robotInnerRadius = markerSizeInPx(robotInnerRadiusM)
  $: robotNoseStart = markerSizeInPx(robotNoseStartM)
  $: robotNoseEnd = markerSizeInPx(robotNoseEndM)
  $: robotArrowTip = markerSizeInPx(robotArrowTipM)
  $: robotArrowSide = markerSizeInPx(robotArrowSideM)
  $: robotArrowHalfWidth = markerSizeInPx(robotArrowHalfWidthM)
  $: mapPointCount = allMapPoints().length
  $: if (!hasAutoFit && mapPointCount > 1) {
    fitToContent()
    hasAutoFit = true
  }

  onDestroy(() => {
    dragTarget = null
    isPanning = false
  })
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
      <button type="button" class="secondary" on:click={resetView}>Ansicht zentrieren</button>
      <span class="meta">Zoom {(zoom * 100).toFixed(0)}%</span>
    </div>
  {/if}

  <div class="canvas-wrap">
    {#if showZoomControls}
      <div class="zoom-controls">
        <button type="button" title="Heranzoomen" on:click={zoomIn}>+</button>
        <button type="button" title="Herauszoomen" on:click={zoomOut}>−</button>
        <button type="button" title="Auf Inhalt zoomen" on:click={fitToContent}>◎</button>
      </div>
    {/if}
    <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_noninteractive_tabindex a11y_no_noninteractive_element_interactions -->
    <svg
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid slice"
      role="img"
      aria-label="Map canvas"
      tabindex={interactive ? 0 : -1}
      data-readonly={!interactive}
      on:click={handleCanvasClick}
      on:pointerdown={handlePointerDown}
      on:pointermove={handlePointerMove}
      on:pointerup={releasePointer}
      on:pointerleave={releasePointer}
      on:wheel|nonpassive={handleWheel}
    >
      <defs>
        <pattern id="grid" width={currentScale * grid} height={currentScale * grid} patternUnits="userSpaceOnUse" x={offsetX} y={offsetY}>
          <path d={`M ${currentScale * grid} 0 L 0 0 0 ${currentScale * grid}`} fill="none" stroke="rgba(30,58,95,0.45)" stroke-width="1" />
        </pattern>
      </defs>

      <rect x="0" y="0" width={width} height={height} fill="url(#grid)" />
      <line x1="0" y1={height / 2} x2={width} y2={height / 2} stroke="rgba(148,163,184,0.18)" stroke-width="1" />
      <line x1={width / 2} y1="0" x2={width / 2} y2={height} stroke="rgba(148,163,184,0.18)" stroke-width="1" />

      {#if $mapStore.map.perimeter.length > 1}
        <polyline points={path($mapStore.map.perimeter, currentScale, offsetX, offsetY)} fill="rgba(29, 78, 216, 0.10)" stroke="#2563eb" stroke-width="2" />
      {/if}

      {#each $mapStore.map.perimeter as point, index}
        {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
        <circle cx={p.x} cy={p.y} r="5" fill="#2563eb" />
        {#if isSelected({ kind: 'perimeter', index })}
          <circle cx={p.x} cy={p.y} r="9" class="selection-ring" />
        {/if}
      {/each}

      {#each $mapStore.map.dock as point, index}
        {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
        <rect x={p.x - 8} y={p.y - 8} width="16" height="16" fill="#d97706" rx="3" />
        <text x={p.x} y={p.y + 4} text-anchor="middle" class="label dock-label">D</text>
        {#if isSelected({ kind: 'dock', index })}
          <circle cx={p.x} cy={p.y} r="11" class="selection-ring dock-selection" />
        {/if}
      {/each}

      {#if showRobot}
        <g transform={`translate(${robotScreen.x} ${robotScreen.y}) rotate(${robotHeadingDeg})`}>
          <circle r={robotOuterRadius} fill="#0f1829" stroke="#3b82f6" stroke-width="2" />
          <circle r={robotInnerRadius} fill="#60a5fa" />
          <line
            x1="0"
            y1={-robotNoseStart}
            x2="0"
            y2={-robotNoseEnd}
            stroke="#60a5fa"
            stroke-width="2.5"
            stroke-linecap="round"
          />
          <polygon
            points={`0,${-robotArrowTip} ${-robotArrowHalfWidth},${-robotArrowSide} ${robotArrowHalfWidth},${-robotArrowSide}`}
            fill="#60a5fa"
          />
        </g>
      {/if}

      {#each $mapStore.map.exclusions as exclusion, exclusionIndex}
        {#if exclusion.length === 2}
          <polyline
            points={path(exclusion, currentScale, offsetX, offsetY)}
            fill="none"
            stroke={$mapStore.selectedExclusionIndex === exclusionIndex ? '#fca5a5' : '#dc2626'}
            stroke-width={$mapStore.selectedExclusionIndex === exclusionIndex ? '3' : '2'}
            stroke-dasharray="5 3"
          />
        {:else if exclusion.length > 2}
          <polygon
            points={path(exclusion, currentScale, offsetX, offsetY)}
            fill={$mapStore.selectedExclusionIndex === exclusionIndex ? 'rgba(220, 38, 38, 0.16)' : 'rgba(220, 38, 38, 0.08)'}
            stroke={$mapStore.selectedExclusionIndex === exclusionIndex ? '#fca5a5' : '#dc2626'}
            stroke-width={$mapStore.selectedExclusionIndex === exclusionIndex ? '3' : '2'}
            stroke-dasharray="5 3"
            role="button"
            tabindex="-1"
            aria-label={`NoGo ${exclusionIndex + 1} auswaehlen`}
            on:click|stopPropagation={() => selectExclusionArea(exclusionIndex)}
          />
        {/if}
        {#each exclusion as point, index}
          {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
          <circle cx={p.x} cy={p.y} r="5" fill={$mapStore.selectedExclusionIndex === exclusionIndex ? '#fca5a5' : '#dc2626'} />
          {#if isSelected({ kind: 'exclusion', exclusionIndex, index })}
            <circle cx={p.x} cy={p.y} r="9" class="selection-ring nogo-selection" />
          {/if}
        {/each}
        {@const exclusionCenter = centroid(exclusion)}
        {#if exclusionCenter}
          {@const center = worldToScreen(exclusionCenter, currentScale, offsetX, offsetY)}
          <text
            x={center.x + 8}
            y={center.y - 8}
            class="label exclusion-label"
            role="button"
            tabindex="-1"
            aria-label={`NoGo ${exclusionIndex + 1} auswaehlen`}
            on:click|stopPropagation={() => selectExclusionArea(exclusionIndex)}
          >NoGo {exclusionIndex + 1}</text>
        {/if}
      {/each}

      {#each $mapStore.map.zones as zone}
        {#if zone.polygon.length === 2}
          <polyline
            points={path(zone.polygon, currentScale, offsetX, offsetY)}
            fill="none"
            stroke="#0891b2"
            stroke-width="1.8"
          />
        {:else if zone.polygon.length > 2}
          <polygon points={path(zone.polygon, currentScale, offsetX, offsetY)} fill="rgba(8, 145, 178, 0.08)" stroke="#0891b2" stroke-width="1.8" />
        {/if}
        {#each zone.polygon as point, index}
          {@const p = worldToScreen(point, currentScale, offsetX, offsetY)}
          <circle cx={p.x} cy={p.y} r="5" fill="#0891b2" />
          {#if isSelected({ kind: 'zone', zoneId: zone.id, index })}
            <circle cx={p.x} cy={p.y} r="9" class="selection-ring zone-selection" />
          {/if}
        {/each}
        {@const zoneCenter = centroid(zone.polygon)}
        {#if zoneCenter}
          {@const center = worldToScreen(zoneCenter, currentScale, offsetX, offsetY)}
          <text x={center.x + 8} y={center.y - 8} class="label zone-label">{zone.settings.name}</text>
        {/if}
      {/each}
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

  svg[tabindex='0'] {
    outline: none;
  }

  svg[data-readonly='true'] {
    cursor: default;
  }

  .label {
    font-size: 12px;
    font-weight: 700;
    paint-order: stroke;
    stroke: rgba(7, 17, 15, 0.85);
    stroke-width: 3px;
    stroke-linejoin: round;
  }

  .zone-label {
    fill: #67e8f9;
  }

  .exclusion-label {
    fill: #fecdd3;
  }

  .dock-label {
    fill: #0a0f1a;
    stroke: none;
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
</style>
