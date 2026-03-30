<script lang="ts">
  import { onDestroy } from 'svelte'
  import { telemetry } from '../../stores/telemetry'
  import type { Point } from '../../stores/map'
  import { mapStore } from '../../stores/map'

  export let width = 900
  export let height = 620

  const baseScale = 20
  const grid = 1
  const pointHitRadius = 10

  let zoom = 1
  let offsetX = 0
  let offsetY = 0
  let isPanning = false
  let panStartX = 0
  let panStartY = 0
  let panOriginX = 0
  let panOriginY = 0
  let suppressClick = false
  let pointerCaptured = false

  type DragTarget =
    | { kind: 'perimeter'; index: number }
    | { kind: 'dock'; index: number }
    | { kind: 'zone'; zoneId: string; index: number }
    | { kind: 'exclusion'; exclusionIndex: number; index: number }

  let dragTarget: DragTarget | null = null

  const scale = () => baseScale * zoom

  function worldToScreen(point: Point) {
    return {
      x: width / 2 + offsetX + point.x * scale(),
      y: height / 2 + offsetY - point.y * scale(),
    }
  }

  function clientToSvgCoordinates(clientX: number, clientY: number, target: SVGSVGElement) {
    const rect = target.getBoundingClientRect()
    const scaleX = width / rect.width
    const scaleY = height / rect.height
    return {
      x: (clientX - rect.left) * scaleX,
      y: (clientY - rect.top) * scaleY,
    }
  }

  function screenToWorldFromCoordinates(clientX: number, clientY: number, target: SVGSVGElement) {
    const { x: sx, y: sy } = clientToSvgCoordinates(clientX, clientY, target)
    return {
      x: Number((((sx - width / 2 - offsetX) / scale())).toFixed(2)),
      y: Number((((height / 2 + offsetY - sy) / scale())).toFixed(2)),
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
      const screen = worldToScreen($mapStore.map.dock[index])
      if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
        return { kind: 'dock', index } as DragTarget
      }
    }

    for (let index = 0; index < $mapStore.map.perimeter.length; index += 1) {
      const screen = worldToScreen($mapStore.map.perimeter[index])
      if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
        return { kind: 'perimeter', index } as DragTarget
      }
    }

    for (let exclusionIndex = 0; exclusionIndex < $mapStore.map.exclusions.length; exclusionIndex += 1) {
      const exclusion = $mapStore.map.exclusions[exclusionIndex]
      for (let index = 0; index < exclusion.length; index += 1) {
        const screen = worldToScreen(exclusion[index])
        if (distanceSquared(cursor, screen) <= pointHitRadius * pointHitRadius) {
          return { kind: 'exclusion', exclusionIndex, index } as DragTarget
        }
      }
    }

    for (const zone of $mapStore.map.zones) {
      for (let index = 0; index < zone.polygon.length; index += 1) {
        const screen = worldToScreen(zone.polygon[index])
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

  function handlePointerDown(event: PointerEvent) {
    const target = event.currentTarget as SVGSVGElement
    const drag = findDragTarget(event.clientX, event.clientY, target)
    suppressClick = false

    if (drag) {
      dragTarget = drag
      pointerCaptured = true
      target.setPointerCapture(event.pointerId)
      return
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
    if (suppressClick) {
      suppressClick = false
      return
    }
    const target = event.currentTarget as SVGSVGElement
    mapStore.addPoint(screenToWorld(event, target))
  }

  function handleWheel(event: WheelEvent) {
    event.preventDefault()
    const delta = event.deltaY < 0 ? 1.1 : 0.9
    zoom = Math.max(0.5, Math.min(4, Number((zoom * delta).toFixed(3))))
  }

  function path(points: Point[]) {
    return points.map((point) => {
      const p = worldToScreen(point)
      return `${p.x},${p.y}`
    }).join(' ')
  }

  onDestroy(() => {
    dragTarget = null
    isPanning = false
  })
</script>

<section class="panel">
  <header>
    <h2>Arbeitsraum</h2>
    <p>Lokales Gitter in Meterkoordinaten. Klicks setzen Punkte fuer das aktuell aktive Werkzeug.</p>
  </header>

  <div class="canvas-wrap">
    <svg
      viewBox={`0 0 ${width} ${height}`}
      role="img"
      aria-label="Map canvas"
      on:click={handleCanvasClick}
      on:pointerdown={handlePointerDown}
      on:pointermove={handlePointerMove}
      on:pointerup={releasePointer}
      on:pointerleave={releasePointer}
      on:wheel={handleWheel}
    >
      <defs>
        <pattern id="grid" width={scale() * grid} height={scale() * grid} patternUnits="userSpaceOnUse" x={offsetX} y={offsetY}>
          <path d={`M ${scale() * grid} 0 L 0 0 0 ${scale() * grid}`} fill="none" stroke="rgba(140,170,150,0.15)" stroke-width="1" />
        </pattern>
      </defs>

      <rect x="0" y="0" width={width} height={height} fill="url(#grid)" />
      <line x1="0" y1={height / 2} x2={width} y2={height / 2} stroke="rgba(220,232,232,0.2)" stroke-width="1" />
      <line x1={width / 2} y1="0" x2={width / 2} y2={height} stroke="rgba(220,232,232,0.2)" stroke-width="1" />

      {#if $mapStore.map.perimeter.length > 1}
        <polyline points={path($mapStore.map.perimeter)} fill="rgba(143, 207, 114, 0.14)" stroke="#8fcf72" stroke-width="2" />
      {/if}

      {#each $mapStore.map.perimeter as point}
        {@const p = worldToScreen(point)}
        <circle cx={p.x} cy={p.y} r="5" fill="#8fcf72" />
      {/each}

      {#each $mapStore.map.dock as point}
        {@const p = worldToScreen(point)}
        <rect x={p.x - 6} y={p.y - 6} width="12" height="12" fill="#e1c57e" rx="2" />
      {/each}

      {@const robot = worldToScreen({ x: $telemetry.x, y: $telemetry.y })}
      <circle cx={robot.x} cy={robot.y} r="7" fill="#f2f6f1" stroke="#0c1513" stroke-width="2" />

      {#each $mapStore.map.exclusions as exclusion}
        {#if exclusion.length > 1}
          <polygon points={path(exclusion)} fill="rgba(224, 123, 141, 0.14)" stroke="#de8899" stroke-width="2" />
        {/if}
        {#each exclusion as point}
          {@const p = worldToScreen(point)}
          <circle cx={p.x} cy={p.y} r="5" fill="#de8899" />
        {/each}
      {/each}

      {#each $mapStore.map.zones as zone}
        {#if zone.polygon.length > 1}
          <polygon points={path(zone.polygon)} fill="rgba(124, 181, 240, 0.12)" stroke="#86b8ea" stroke-width="2" />
        {/if}
        {#each zone.polygon as point}
          {@const p = worldToScreen(point)}
          <circle cx={p.x} cy={p.y} r="5" fill="#86b8ea" />
        {/each}
      {/each}
    </svg>
  </div>
</section>

<style>
  .panel {
    display: grid;
    gap: 1rem;
    padding: 1.1rem;
    border-radius: 1rem;
    background: rgba(13, 25, 22, 0.82);
    border: 1px solid rgba(152, 187, 170, 0.14);
  }

  header {
    display: grid;
    gap: 0.25rem;
  }

  h2,
  p {
    margin: 0;
  }

  p {
    color: #9db3ab;
  }

  .canvas-wrap {
    overflow: hidden;
    border-radius: 1rem;
    border: 1px solid rgba(152, 187, 170, 0.14);
    background: linear-gradient(180deg, rgba(12, 24, 21, 0.92), rgba(16, 30, 26, 0.96));
  }

  svg {
    display: block;
    width: 100%;
    height: auto;
    cursor: grab;
    touch-action: none;
  }
</style>
