<script lang="ts">
  import { createEventDispatcher } from 'svelte'
  import type { Mission } from '../../stores/missions'
  import type { Zone, ZoneSettings } from '../../stores/map'

  export let zones: Zone[] = []
  export let mission: Mission | null = null
  export let selectedZoneId: string | null = null

  const dispatch = createEventDispatcher<{ selectzone: { zoneId: string } }>()

  let zoom = 1
  let offsetX = 0
  let offsetY = 0
  let dragging = false
  let dragStartX = 0
  let dragStartY = 0

  const width = 820
  const height = 520
  const zoneColors = ['#a855f7', '#22d3ee', '#22c55e', '#f59e0b', '#f97316', '#38bdf8']

  type Bounds = { minX: number; minY: number; maxX: number; maxY: number }

  function effectiveSettings(zone: Zone): ZoneSettings {
    const override = mission?.overrides?.[zone.id] ?? {}
    return {
      ...zone.settings,
      ...override,
    }
  }

  function boundsOfZones(input: Zone[]): Bounds {
    const points = input.flatMap((zone) => zone.polygon)
    if (points.length === 0) return { minX: 0, minY: 0, maxX: 10, maxY: 10 }
    return {
      minX: Math.min(...points.map((point) => point.x)),
      minY: Math.min(...points.map((point) => point.y)),
      maxX: Math.max(...points.map((point) => point.x)),
      maxY: Math.max(...points.map((point) => point.y)),
    }
  }

  function polygonPoints(zone: Zone, bounds: Bounds) {
    const spanX = Math.max(1, bounds.maxX - bounds.minX)
    const spanY = Math.max(1, bounds.maxY - bounds.minY)
    const baseScale = Math.min((width - 120) / spanX, (height - 120) / spanY)
    const scale = baseScale * zoom
    const centerX = width / 2 + offsetX
    const centerY = height / 2 + offsetY
    const midX = (bounds.minX + bounds.maxX) / 2
    const midY = (bounds.minY + bounds.maxY) / 2

    return zone.polygon
      .map((point) => {
        const x = centerX + (point.x - midX) * scale
        const y = centerY - (point.y - midY) * scale
        return `${x.toFixed(2)},${y.toFixed(2)}`
      })
      .join(' ')
  }

  function polygonCenter(zone: Zone): { x: number; y: number } {
    const total = zone.polygon.reduce((acc, point) => ({ x: acc.x + point.x, y: acc.y + point.y }), { x: 0, y: 0 })
    return {
      x: total.x / Math.max(1, zone.polygon.length),
      y: total.y / Math.max(1, zone.polygon.length),
    }
  }

  function project(point: { x: number; y: number }, bounds: Bounds) {
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

  function stripeLines(zone: Zone, bounds: Bounds, settings: ZoneSettings) {
    const projected = zone.polygon.map((point) => project(point, bounds))
    const minX = Math.min(...projected.map((point) => point.x))
    const maxX = Math.max(...projected.map((point) => point.x))
    const minY = Math.min(...projected.map((point) => point.y))
    const maxY = Math.max(...projected.map((point) => point.y))
    const lineCount = 18
    const lines: Array<{ x1: number; y1: number; x2: number; y2: number }> = []
    for (let index = -lineCount; index <= lineCount; index += 1) {
      const y = minY + ((index + lineCount) / (lineCount * 2 || 1)) * (maxY - minY)
      lines.push({ x1: minX - 120, y1: y, x2: maxX + 120, y2: y })
    }
    return { lines, angle: settings.angle }
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

  function endDrag() {
    dragging = false
  }

  function onWheel(event: WheelEvent) {
    event.preventDefault()
    const delta = event.deltaY < 0 ? 0.12 : -0.12
    zoom = Math.max(0.5, Math.min(3, zoom + delta))
  }

  function zoomIn() {
    zoom = Math.min(3, zoom + 0.2)
  }

  function zoomOut() {
    zoom = Math.max(0.5, zoom - 0.2)
  }

  function resetView() {
    zoom = 1
    offsetX = 0
    offsetY = 0
  }

  $: missionZones = mission
    ? mission.zoneIds
        .map((zoneId) => zones.find((zone) => zone.id === zoneId))
        .filter((zone): zone is Zone => Boolean(zone))
    : []

  $: bounds = boundsOfZones(missionZones)
</script>

<section class="preview-shell">
  <div class="preview-toolbar">
    <div>
      <span class="label">Bahnvorschau</span>
      <strong>{mission ? mission.name : 'Keine Mission'}</strong>
    </div>
    <div class="zoom-actions">
      <button type="button" on:click={zoomIn}>+</button>
      <button type="button" on:click={zoomOut}>−</button>
      <button type="button" on:click={resetView}>⊙</button>
    </div>
  </div>

  {#if missionZones.length === 0}
    <div class="empty">Noch keine Zonen in dieser Mission.</div>
  {:else}
    <div class="canvas-wrap">
      <div class="legend">
        {#each missionZones as zone, index}
          <button
            type="button"
            class:selected={selectedZoneId === zone.id}
            class="legend-item"
            on:click={() => dispatch('selectzone', { zoneId: zone.id })}
          >
            <span class="legend-dot" style={`background:${zoneColors[index % zoneColors.length]}`}></span>
            <span class="legend-name">{zone.settings.name}</span>
            <span class="legend-angle">{effectiveSettings(zone).angle}°</span>
          </button>
        {/each}
      </div>

      <svg
        viewBox={`0 0 ${width} ${height}`}
        class="canvas"
        role="img"
        aria-label="Bahnvorschau der aktuellen Mission"
        on:pointerdown={startDrag}
        on:pointermove={duringDrag}
        on:pointerup={endDrag}
        on:pointerleave={endDrag}
        on:wheel|preventDefault={onWheel}
      >
        <defs>
          {#each missionZones as zone, index}
            <clipPath id={`zone-clip-${zone.id}`}>
              <polygon points={polygonPoints(zone, bounds)} />
            </clipPath>
          {/each}
        </defs>

        <rect x="0" y="0" width={width} height={height} fill="#070d18" />

        {#each missionZones as zone, index}
          {@const settings = effectiveSettings(zone)}
          {@const color = zoneColors[index % zoneColors.length]}
          {@const center = project(polygonCenter(zone), bounds)}
          {@const stripes = stripeLines(zone, bounds, settings)}

          <g
            role="button"
            tabindex="0"
            aria-label={`Zone ${zone.settings.name} auswählen`}
            on:click={() => dispatch('selectzone', { zoneId: zone.id })}
            on:keydown={(event) => {
              if (event.key === 'Enter' || event.key === ' ') {
                event.preventDefault()
                dispatch('selectzone', { zoneId: zone.id })
              }
            }}
          >
            <polygon
              points={polygonPoints(zone, bounds)}
              fill={selectedZoneId === zone.id ? `${color}44` : `${color}26`}
              stroke={color}
              stroke-width={selectedZoneId === zone.id ? 3 : 1.8}
            />

            <g clip-path={`url(#zone-clip-${zone.id})`} transform={`rotate(${-stripes.angle} ${center.x} ${center.y})`}>
              {#each stripes.lines as line}
                <line x1={line.x1} y1={line.y1} x2={line.x2} y2={line.y2} stroke={color} stroke-opacity="0.55" stroke-width="1.1" />
              {/each}
            </g>

            <text x={center.x} y={center.y} text-anchor="middle" fill="#e2e8f0" font-size="12" font-weight="600">
              {zone.settings.name}
            </text>
          </g>
        {/each}
      </svg>
    </div>
  {/if}
</section>

<style>
  .preview-shell {
    display: grid;
    gap: 0.75rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }
  .preview-toolbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
  }
  .label {
    display: block;
    margin-bottom: 0.2rem;
    color: #7a8da8;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }
  .zoom-actions {
    display: flex;
    gap: 0.35rem;
  }
  .zoom-actions button, .legend-item {
    border: 1px solid #1e3a5f;
    border-radius: 0.6rem;
    background: #0a1020;
    color: #93c5fd;
    cursor: pointer;
  }
  .zoom-actions button {
    width: 2rem;
    height: 2rem;
    font-size: 1rem;
  }
  .canvas-wrap {
    position: relative;
  }
  .legend {
    position: absolute;
    top: 0.75rem;
    left: 0.75rem;
    z-index: 2;
    display: grid;
    gap: 0.35rem;
  }
  .legend-item {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    padding: 0.45rem 0.6rem;
    color: #dce8e8;
  }
  .legend-item.selected {
    border-color: #2563eb;
    background: #0c1a3a;
  }
  .legend-dot {
    width: 0.65rem;
    height: 0.65rem;
    border-radius: 999px;
    flex-shrink: 0;
  }
  .legend-name {
    font-size: 0.8rem;
  }
  .legend-angle {
    color: #64748b;
    font-size: 0.74rem;
    font-family: monospace;
  }
  .canvas {
    width: 100%;
    min-height: 520px;
    border-radius: 0.8rem;
    background: #070d18;
    cursor: grab;
  }
  .empty {
    color: #64748b;
    font-size: 0.84rem;
  }
</style>
