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
  export const zoneColors = ['#a855f7', '#22d3ee', '#22c55e', '#f59e0b', '#f97316', '#38bdf8']

  type Bounds = { minX: number; minY: number; maxX: number; maxY: number }

  function effectiveSettings(zone: Zone): ZoneSettings {
    const override = mission?.overrides?.[zone.id] ?? {}
    return { ...zone.settings, ...override }
  }

  function boundsOfZones(input: Zone[]): Bounds {
    const points = input.flatMap((z) => z.polygon)
    if (points.length === 0) return { minX: 0, minY: 0, maxX: 10, maxY: 10 }
    return {
      minX: Math.min(...points.map((p) => p.x)),
      minY: Math.min(...points.map((p) => p.y)),
      maxX: Math.max(...points.map((p) => p.x)),
      maxY: Math.max(...points.map((p) => p.y)),
    }
  }

  function polygonPoints(zone: Zone, bounds: Bounds): string {
    const spanX = Math.max(1, bounds.maxX - bounds.minX)
    const spanY = Math.max(1, bounds.maxY - bounds.minY)
    const baseScale = Math.min((width - 120) / spanX, (height - 120) / spanY)
    const scale = baseScale * zoom
    const centerX = width / 2 + offsetX
    const centerY = height / 2 + offsetY
    const midX = (bounds.minX + bounds.maxX) / 2
    const midY = (bounds.minY + bounds.maxY) / 2
    return zone.polygon
      .map((p) => {
        const x = centerX + (p.x - midX) * scale
        const y = centerY - (p.y - midY) * scale
        return `${x.toFixed(2)},${y.toFixed(2)}`
      })
      .join(' ')
  }

  function polygonCenter(zone: Zone): { x: number; y: number } {
    const total = zone.polygon.reduce((acc, p) => ({ x: acc.x + p.x, y: acc.y + p.y }), { x: 0, y: 0 })
    return { x: total.x / Math.max(1, zone.polygon.length), y: total.y / Math.max(1, zone.polygon.length) }
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
    const projected = zone.polygon.map((p) => project(p, bounds))
    const minX = Math.min(...projected.map((p) => p.x))
    const maxX = Math.max(...projected.map((p) => p.x))
    const minY = Math.min(...projected.map((p) => p.y))
    const maxY = Math.max(...projected.map((p) => p.y))
    const lineCount = 18
    const lines: Array<{ x1: number; y1: number; x2: number; y2: number }> = []
    for (let i = -lineCount; i <= lineCount; i += 1) {
      const y = minY + ((i + lineCount) / (lineCount * 2 || 1)) * (maxY - minY)
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
        .map((id) => zones.find((z) => z.id === id))
        .filter((z): z is Zone => Boolean(z))
    : []

  $: bounds = boundsOfZones(missionZones)
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
          <span class="ms-leg-angle">{effectiveSettings(zone).angle}°</span>
        </button>
      {/each}
    </div>
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
      {#each missionZones as zone}
        <clipPath id="zone-clip-{zone.id}">
          <polygon points={polygonPoints(zone, bounds)} />
        </clipPath>
      {/each}
    </defs>

    <rect x="-9999" y="-9999" width="19999" height="19999" fill="#070d18" />
    <rect x="-9999" y="-9999" width="19999" height="19999" fill="url(#pp-grid)" />

    {#each missionZones as zone, i}
      {@const settings = effectiveSettings(zone)}
      {@const color = zoneColors[i % zoneColors.length]}
      {@const center = project(polygonCenter(zone), bounds)}
      {@const stripes = stripeLines(zone, bounds, settings)}
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
          points={polygonPoints(zone, bounds)}
          fill={selectedZoneId === zone.id ? `${color}44` : `${color}26`}
          stroke={color}
          stroke-width={selectedZoneId === zone.id ? 3 : 1.8}
        />
        <g clip-path="url(#zone-clip-{zone.id})" transform="rotate({-stripes.angle} {center.x} {center.y})">
          {#each stripes.lines as line}
            <line
              x1={line.x1} y1={line.y1} x2={line.x2} y2={line.y2}
              stroke={color} stroke-opacity="0.55" stroke-width="1.1"
            />
          {/each}
        </g>
        <text x={center.x} y={center.y} text-anchor="middle" fill="#e2e8f0" font-size="12" font-weight="600">
          {zone.settings.name}
        </text>
      </g>
    {/each}
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
